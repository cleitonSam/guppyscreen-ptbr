#include "extruder_panel.h"
#include "state.h"
#include "config.h"
#include "utils.h"
#include "marca.h"
#include "navegacao.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

// How close (in C) the hotend must be to the target before we fire a pending
// extrude/retract. Klipper still enforces its own min_extrude_temp.
static const int HEAT_TOLERANCE = 5;
// Max time to wait for the hotend to reach the requested temperature before
// giving up and clearing the pending action.
static const uint32_t HEAT_TIMEOUT_MS = 5 * 60 * 1000;
// Safety cap on how long we keep buttons locked while waiting for a gcode
// response. Long enough for a 50mm slow load; trips only if the ws dies.
static const uint32_t ACTION_TIMEOUT_MS = 5 * 60 * 1000;
// Troca de filamento parada no meio (ninguém mexendo) por este tempo: a troca
// é encerrada e o bico desligado, para o aquecedor não ficar ligado à toa.
static const uint32_t TROCA_ESPERA_MS = 10 * 60 * 1000;

// Chunked load (stoppable): the stock _GUPPY_LOAD_MATERIAL is one uninterruptible
// G1 move, so we feed in small steps issued one at a time and the user can Stop
// between chunks. To match the stock load's quality (and actually seat the
// filament) we feed in two phases: a quick "approach" to get filament down to the
// hotend, then a slow "purge" through the melt zone. Feeds are mm/min (F-value);
// a 10mm chunk is the worst-case stop latency (~1s fast, ~3s slow).
static const int LOAD_CHUNK_MM  = 10;
static const int LOAD_FAST_MM   = 80;    // approach: quick feed toward the nozzle
static const int LOAD_FAST_FEED = 600;   // 10 mm/s
static const int LOAD_SLOW_MM   = 50;    // purge: slow feed through the melt zone
static const int LOAD_SLOW_FEED = 180;   // 3 mm/s (matches the stock load macro)
static const int LOAD_TOTAL_MM  = LOAD_FAST_MM + LOAD_SLOW_MM;  // 130

// Temperaturas do − / + (os mesmos valores do antigo seletor "Temperatura do
// bico"; o padrão continua vindo de /default_extruder_temp).
// 230-250 de 5 em 5: o PETG do dono é 240-250 (carregar/descarregar a 250,
// igual às macros)
// sem 300: e o limite do bico (max_temp) e a subida passa um pouco do alvo
static const int TEMPS[] = {180, 200, 220, 230, 240, 245, 250, 260, 280};
static const int N_TEMPS = sizeof(TEMPS) / sizeof(TEMPS[0]);
static const int TEMP_IDX_PADRAO = 6;  // 250

// Extrudar / Retrair: velocidade padrão de antes (5 mm/s, o índice 2 do antigo
// seletor), agora sem seletor na tela.
static const int EXTRUDE_SPEED_MM_S = 5;
// Purga da troca de filamento (etapa 4 e "Purgar mais").
static const int PURGA_MM = 10;

// Mapa do segmentado de comprimento (o Selector guarda o vetor).
static const int COMPRIMENTOS[] = {10, 50, 100};

LV_IMG_DECLARE(unload_filament_img);
LV_IMG_DECLARE(load_filament_img);
LV_IMG_DECLARE(filament_img);
LV_IMG_DECLARE(extruder);

ExtruderPanel::ExtruderPanel(KWebSocketClient &websocket_client,
  std::mutex &lock,
  Numpad &numpad,
  SpoolmanPanel &sm)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , spoolman_panel(sm)
  , load_btn(panel_cont, &load_filament_img, "Carregar", &ExtruderPanel::_handle_callback, this)
  , unload_btn(panel_cont, &unload_filament_img, "Descarregar", &ExtruderPanel::_handle_callback, this)
  , troca_btn(panel_cont, &filament_img, "Trocar", &ExtruderPanel::_handle_callback, this)
  , extrude_obj(NULL)
  , retract_obj(NULL)
  , length_selector(panel_cont, "", {"10 mm", "50 mm", "100 mm", ""}, 0,
    &ExtruderPanel::_handle_callback, this)
  , load_filament_macro("LOAD_FILAMENT")
  , unload_filament_macro("UNLOAD_FILAMENT")
  , cooldown_macro("SET_HEATER_TEMPERATURE HEATER=extruder TARGET=0")
  , troca([this]() { this->troca_esquerdo(); }, [this]() { this->troca_direito(); })
{
  LV_UNUSED(numpad);  // o valor da temperatura padrão anda por − / + (sem teclado)

  Config *conf = Config::get_instance();
  auto df = conf->get_json("/default_printer");
  if (!df.empty()) {
    auto v = conf->get_json(conf->df() + "default_macros/load_filament");
    if (!v.is_null()) {
      load_filament_macro = v.template get<std::string>();
    }

    v = conf->get_json(conf->df() + "default_macros/unload_filament");
    if (!v.is_null()) {
      unload_filament_macro = v.template get<std::string>();
    }

    v = conf->get_json(conf->df() + "default_macros/cooldown");
    if (!v.is_null()) {
      cooldown_macro = v.template get<std::string>();
    }
  }

  // ---- a aba: Controle › Filamento (424 x 227) -----------------------------
  Navegacao::colocar(panel_cont, Aba::FILAMENTO);
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(panel_cont, 0, 0);
  lv_obj_set_style_border_width(panel_cont, 0, 0);
  lv_obj_set_style_radius(panel_cont, 0, 0);
  // nasceu filho da tela (o tema pinta de PRETO opaco); dentro da aba não
  // precisa desenhar fundo nenhum
  lv_obj_set_style_bg_opa(panel_cont, LV_OPA_TRANSP, 0);

  // ---- cartão "Temperatura" 408 x 76 em (8,7) -----------------------------
  temp_cartao = Marca::criar_cartao(panel_cont, 8, 7, 408, 76);

  lv_obj_t *ico = Marca::criar_icone(temp_cartao, &extruder, 24, Marca::GRAFITE);
  lv_obj_set_pos(ico, 12, 12);

  lv_obj_t *nome = Marca::criar_rotulo(temp_cartao, "Temperatura", Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(nome, 42, 15);
  lv_obj_set_width(nome, 140);
  lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);

  // linha de estado: bolinha de 8 px + texto 12 grafite
  estado_ponto = lv_obj_create(temp_cartao);
  lv_obj_set_pos(estado_ponto, 12, 52);
  lv_obj_set_size(estado_ponto, 8, 8);
  lv_obj_set_style_radius(estado_ponto, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(estado_ponto, lv_color_hex(Marca::LIMAO), 0);
  lv_obj_set_style_bg_opa(estado_ponto, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(estado_ponto, 0, 0);
  lv_obj_set_style_pad_all(estado_ponto, 0, 0);
  lv_obj_clear_flag(estado_ponto, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(estado_ponto, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(estado_ponto, LV_OBJ_FLAG_HIDDEN);

  estado_label = Marca::criar_rotulo(temp_cartao, "", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(estado_label, 24, 48);
  lv_obj_set_width(estado_label, 160);
  lv_label_set_long_mode(estado_label, LV_LABEL_LONG_WRAP);

  lv_obj_t *alvo_titulo = Marca::criar_rotulo(temp_cartao, "Alvo", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(alvo_titulo, 244, 4);
  lv_obj_set_width(alvo_titulo, 92);
  lv_obj_set_style_text_align(alvo_titulo, LV_TEXT_ALIGN_CENTER, 0);

  menos_btn = Marca::criar_botao(temp_cartao, LV_SYMBOL_MINUS, Marca::Tipo::NORMAL, Marca::f20());
  lv_obj_set_pos(menos_btn, 188, 24);
  lv_obj_set_size(menos_btn, 52, 44);
  lv_obj_add_event_cb(menos_btn, &ExtruderPanel::_handle_callback, LV_EVENT_CLICKED, this);

  lv_obj_t *valor_cont = lv_obj_create(temp_cartao);
  lv_obj_set_pos(valor_cont, 244, 24);
  lv_obj_set_size(valor_cont, 92, 44);
  lv_obj_set_style_bg_color(valor_cont, lv_color_hex(Marca::PRETO), 0);
  lv_obj_set_style_bg_opa(valor_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(valor_cont, 1, 0);
  lv_obj_set_style_border_color(valor_cont, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_set_style_radius(valor_cont, Marca::RAIO, 0);
  lv_obj_set_style_pad_all(valor_cont, 0, 0);
  lv_obj_clear_flag(valor_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(valor_cont, LV_OBJ_FLAG_CLICKABLE);

  temp_valor = Marca::criar_rotulo(valor_cont, "240°C", Marca::f20(), Marca::PAPEL);
  lv_obj_center(temp_valor);

  mais_btn = Marca::criar_botao(temp_cartao, LV_SYMBOL_PLUS, Marca::Tipo::NORMAL, Marca::f20());
  lv_obj_set_pos(mais_btn, 340, 24);
  lv_obj_set_size(mais_btn, 52, 44);
  lv_obj_add_event_cb(mais_btn, &ExtruderPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // ---- Carregar / Descarregar / Trocar: 72 de altura em y = 91 -------------
  struct { ButtonContainer *b; lv_coord_t x; lv_coord_t w; } grandes[] = {
    { &load_btn,   8,   130 },
    { &unload_btn, 147, 131 },
    { &troca_btn,  287, 129 },
  };
  for (auto &g : grandes) {
    lv_obj_t *c = g.b->get_container();
    lv_obj_set_size(c, g.w, 72);
    lv_obj_set_pos(c, g.x, 91);
    g.b->set_tipo(Marca::Tipo::NORMAL);
  }

  // ---- Extrudar / Retrair 100 x 44 e comprimento 192 x 44 em y = 175 -------
  extrude_obj = Marca::criar_botao(panel_cont, LV_SYMBOL_DOWN " Empurrar", Marca::Tipo::NORMAL);
  lv_obj_set_pos(extrude_obj, 8, 175);
  lv_obj_set_size(extrude_obj, 100, 44);
  lv_obj_add_event_cb(extrude_obj, &ExtruderPanel::_handle_callback, LV_EVENT_CLICKED, this);

  retract_obj = Marca::criar_botao(panel_cont, LV_SYMBOL_UP " Puxar", Marca::Tipo::NORMAL);
  lv_obj_set_pos(retract_obj, 116, 175);
  lv_obj_set_size(retract_obj, 100, 44);
  lv_obj_add_event_cb(retract_obj, &ExtruderPanel::_handle_callback, LV_EVENT_CLICKED, this);

  length_selector.sem_titulo();
  lv_obj_set_size(length_selector.get_container(), 192, 44);
  lv_obj_set_pos(length_selector.get_container(), 224, 175);
  lv_obj_set_size(length_selector.get_selector(), 192, 44);

  // ---- botões de ação (para o "Parar" e o liga/desliga) ---------------------
  acao_carregar    = { load_btn.get_container(),   load_btn.get_label(),   "Carregar",    &load_btn };
  acao_descarregar = { unload_btn.get_container(), unload_btn.get_label(), "Descarregar", &unload_btn };
  acao_trocar      = { troca_btn.get_container(),  troca_btn.get_label(),  "Trocar",      &troca_btn };
  acao_extrudar    = { extrude_obj, lv_obj_get_child(extrude_obj, 0), LV_SYMBOL_DOWN " Empurrar", NULL };
  acao_retrair     = { retract_obj, lv_obj_get_child(retract_obj, 0), LV_SYMBOL_UP " Puxar",       NULL };

  ler_padrao();
  mostrar_temp();

  build_filament_dialog();

  // Ao abrir a aba: confere a trava da impressão e a temperatura padrão
  // (pode ter mudado em Configurações). Roda com o lv_lock já preso.
  Navegacao::ao_mostrar(Aba::FILAMENTO, [this]() {
    this->ler_padrao();
    this->refresh_button_state();
  });

  // O acompanhamento abriu sozinho (começou uma impressão): a pergunta do
  // filamento não fica por cima dele. Só esconde a janela; o resto da troca
  // já é cancelado em consume() quando a impressão começa.
  Navegacao::ao_abrir_acompanhamento([this]() {
    if (this->fil_cont != NULL && !lv_obj_has_flag(this->fil_cont, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_move_background(this->fil_cont);
      lv_obj_add_flag(this->fil_cont, LV_OBJ_FLAG_HIDDEN);
      this->dialogo_da_troca = false;
    }
  });

  // vigia da troca parada: criado uma vez só e pausado (sem custo parado)
  troca_espera = lv_timer_create([](lv_timer_t *t) {
    auto *self = (ExtruderPanel *)t->user_data;
    lv_timer_pause(t);
    self->troca_espera_ativa = false;
    if (self->troca_etapa >= 0 && !self->ocupado()) {
      spdlog::warn("troca de filamento parada por falta de uso");
      if (self->dialogo_da_troca && self->fil_cont != NULL) {
        lv_obj_move_background(self->fil_cont);
        lv_obj_add_flag(self->fil_cont, LV_OBJ_FLAG_HIDDEN);
      }
      KUtils::notify_toast(KUtils::is_printing()
        ? "Troca de filamento parada por falta de uso."
        : "Troca de filamento parada por falta de uso. O bico foi desligado.", 4000);
      self->parar();   // não esfria se houver impressão
    }
  }, TROCA_ESPERA_MS, this);
  lv_timer_pause(troca_espera);

  refresh_button_state();

  // por último: consume() só começa com tudo montado
  ws.register_notify_update(this);
}

ExtruderPanel::~ExtruderPanel() {
  cancel_safety_timer();
  if (troca_espera != NULL) {
    lv_timer_del(troca_espera);
    troca_espera = NULL;
  }
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  if (fil_cont != NULL) {
    lv_obj_del(fil_cont);
    fil_cont = NULL;
  }
}

void ExtruderPanel::foreground() {
  Navegacao::ir_para(Aba::FILAMENTO);
}

void ExtruderPanel::enable_spoolman() {
  spoolman_enabled = true;
}

// ---- temperatura padrão -----------------------------------------------------

void ExtruderPanel::ler_padrao() {
  // Aplica /default_extruder_temp quando ele muda (na criação e quando o dono
  // troca em Configurações); fora isso, o − / + manda.
  auto v = Config::get_instance()->get_json("/default_extruder_temp");
  if (v.is_null()) {
    return;
  }
  int def_temp = 0;
  try {
    def_temp = v.template get<int>();
  } catch (const std::exception &) {
    return;
  }
  if (def_temp == padrao_lido) {
    return;
  }
  padrao_lido = def_temp;
  for (int i = 0; i < N_TEMPS; i++) {
    if (TEMPS[i] == def_temp) {
      temp_idx = i;
      mostrar_temp();
      break;
    }
  }
}

int ExtruderPanel::temp_escolhida() {
  if (temp_idx < 0 || temp_idx >= N_TEMPS) {
    temp_idx = TEMP_IDX_PADRAO;
  }
  return TEMPS[temp_idx];
}

void ExtruderPanel::mostrar_temp() {
  std::string t = fmt::format("{}°C", temp_escolhida());
  Marca::texto(temp_valor, t.c_str());
  if (temp_idx <= 0) {
    lv_obj_add_state(menos_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(menos_btn, LV_STATE_DISABLED);
  }
  if (temp_idx >= N_TEMPS - 1) {
    lv_obj_add_state(mais_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(mais_btn, LV_STATE_DISABLED);
  }
}

void ExtruderPanel::mudar_temp(int passo) {
  int novo = std::max(0, std::min(N_TEMPS - 1, temp_idx + passo));
  if (novo != temp_idx) {
    temp_idx = novo;
    mostrar_temp();
    if (troca_etapa >= 0) {
      atualizar_troca();
    }
  }
}

// ---- pergunta do Spoolman (janela em lv_layer_top) ---------------------------

void ExtruderPanel::build_filament_dialog() {
  fil_cont = lv_obj_create(lv_layer_top());
  fil_box = lv_obj_create(fil_cont);
  fil_content_cont = lv_obj_create(fil_box);
  fil_row_cont = lv_obj_create(fil_content_cont);
  fil_swatch = lv_obj_create(fil_row_cont);
  fil_name_label = lv_label_create(fil_row_cont);
  fil_detail_label = lv_label_create(fil_content_cont);
  fil_yes_btn = lv_btn_create(fil_box);
  fil_no_btn = lv_btn_create(fil_box);

  // fundo PRETO opaco na tela inteira (bloqueia o toque no que está atrás)
  KUtils::style_dialog_overlay(fil_cont);
  lv_obj_add_flag(fil_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(fil_cont, LV_OBJ_FLAG_HIDDEN);

  KUtils::style_dialog_box(fil_box);
  lv_obj_set_size(fil_box, 400, 180);
  lv_obj_align(fil_box, LV_ALIGN_CENTER, 0, 0);

  // header (text swapped per spool state in show_load_filament_dialog)
  fil_title_label = lv_label_create(fil_box);
  lv_label_set_text(fil_title_label, "Usar este filamento?");
  lv_obj_set_width(fil_title_label, LV_PCT(100));
  lv_label_set_long_mode(fil_title_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(fil_title_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(fil_title_label, lv_color_hex(Marca::PAPEL), 0);
  KUtils::style_dialog_title(fil_title_label);

  lv_obj_remove_style_all(fil_content_cont);
  lv_obj_set_width(fil_content_cont, LV_PCT(100));
  lv_obj_set_height(fil_content_cont, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(fil_content_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(fil_content_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(fil_content_cont, 7, 0);
  lv_obj_clear_flag(fil_content_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(fil_content_cont, LV_ALIGN_CENTER, 0, -6);

  lv_obj_remove_style_all(fil_row_cont);
  lv_obj_set_width(fil_row_cont, LV_SIZE_CONTENT);
  lv_obj_set_height(fil_row_cont, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(fil_row_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(fil_row_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(fil_row_cont, 8, 0);
  lv_obj_clear_flag(fil_row_cont, LV_OBJ_FLAG_SCROLLABLE);

  // amostra da cor do carretel (a cor vem do Spoolman; a borda é da marca)
  lv_obj_set_size(fil_swatch, 20, 20);
  lv_obj_set_style_bg_opa(fil_swatch, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(fil_swatch, 1, 0);
  lv_obj_set_style_border_color(fil_swatch, lv_color_hex(Marca::PAPEL), 0);
  lv_obj_set_style_radius(fil_swatch, 4, 0);
  lv_obj_set_style_pad_all(fil_swatch, 0, 0);
  lv_obj_clear_flag(fil_swatch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(fil_swatch, LV_OBJ_FLAG_CLICKABLE);

  lv_label_set_long_mode(fil_name_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_max_width(fil_name_label, 300, 0);
  lv_obj_set_style_text_font(fil_name_label, Marca::f14(), 0);
  lv_obj_set_style_text_color(fil_name_label, lv_color_hex(Marca::PAPEL), 0);
  lv_label_set_text(fil_name_label, "");

  lv_obj_set_style_text_align(fil_detail_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(fil_detail_label, Marca::f12(), 0);
  lv_obj_set_style_text_color(fil_detail_label, lv_color_hex(Marca::GRAFITE), 0);
  lv_label_set_text(fil_detail_label, "");

  // Sim (esquerda, limão) e Não (direita): mesma posição de antes
  lv_obj_set_size(fil_yes_btn, 160, 44);
  lv_obj_set_style_pad_all(fil_yes_btn, 0, 0);
  lv_obj_add_event_cb(fil_yes_btn, &ExtruderPanel::_handle_filament_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(fil_yes_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t *yl = lv_label_create(fil_yes_btn);
  lv_label_set_text(yl, "Sim");
  lv_obj_set_style_text_font(yl, Marca::f14(), 0);
  lv_obj_center(yl);
  Marca::estilo_botao(fil_yes_btn, Marca::Tipo::DESTAQUE);

  lv_obj_set_size(fil_no_btn, 160, 44);
  lv_obj_set_style_pad_all(fil_no_btn, 0, 0);
  lv_obj_add_event_cb(fil_no_btn, &ExtruderPanel::_handle_filament_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(fil_no_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_t *nl = lv_label_create(fil_no_btn);
  lv_label_set_text(nl, "Não");
  lv_obj_set_style_text_font(nl, Marca::f14(), 0);
  lv_obj_center(nl);
  Marca::estilo_botao(fil_no_btn, Marca::Tipo::NORMAL);
}

void ExtruderPanel::show_load_filament_dialog() {
  json spool = spoolman_panel.get_active_spool();
  if (spool.is_null()) {
    lv_label_set_text(fil_title_label, "Sem carretel ativo. Carregar mesmo assim?");
    lv_obj_add_flag(fil_swatch, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(fil_name_label, "");
    lv_label_set_text(fil_detail_label, "");
  } else {
    lv_label_set_text(fil_title_label, "Usar este filamento?");
    auto vendor_json = spool["/filament/vendor/name"_json_pointer];
    auto vendor = !vendor_json.is_null() ? vendor_json.template get<std::string>() : "";
    auto name_json = spool["/filament/name"_json_pointer];
    auto name = !name_json.is_null() ? name_json.template get<std::string>() : "";
    lv_label_set_text(fil_name_label, fmt::format("{} - {}", vendor, name).c_str());

    auto color_json = spool["/filament/color_hex"_json_pointer];
    if (!color_json.is_null()) {
      uint32_t rgb = static_cast<uint32_t>(strtol(color_json.template get<std::string>().c_str(), NULL, 16));
      lv_obj_set_style_bg_color(fil_swatch, lv_color_hex(rgb), 0);
      lv_obj_clear_flag(fil_swatch, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(fil_swatch, LV_OBJ_FLAG_HIDDEN);
    }

    auto material_json = spool["/filament/material"_json_pointer];
    auto material = !material_json.is_null() ? material_json.template get<std::string>() : "?";
    auto remaining_json = spool["/remaining_weight"_json_pointer];
    if (!remaining_json.is_null()) {
      lv_label_set_text(fil_detail_label,
        fmt::format("{}, restam {:.0f} g", material, remaining_json.template get<double>()).c_str());
    } else {
      lv_label_set_text(fil_detail_label, material.c_str());
    }
  }
  lv_obj_clear_flag(fil_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(fil_cont);
}

void ExtruderPanel::handle_filament_btns(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(event);

  // dismiss the dialog regardless of choice
  lv_obj_move_background(fil_cont);
  lv_obj_add_flag(fil_cont, LV_OBJ_FLAG_HIDDEN);

  if (btn == fil_yes_btn) {
    if (ocupado()) {
      return;  // outra ação começou enquanto a pergunta estava aberta
    }
    if (travado_imprimindo()) {
      KUtils::notify_locked();  // uma impressão começou com a pergunta aberta
      return;
    }
    if (dialogo_da_troca) {
      dialogo_da_troca = false;
      if (troca_etapa != 2) {
        return;  // a troca foi cancelada no meio do caminho
      }
      botao_ativo = &acao_trocar;
      troca_passo = true;
      run_when_hot(PA_LOAD, "Carregando...", "", false);
      atualizar_troca();
    } else {
      botao_ativo = &acao_carregar;
      run_when_hot(PA_LOAD, "Carregando...", "");
    }
  } else if (btn == fil_no_btn) {
    // open Spoolman to pick/fix the active spool; re-show this confirm once a
    // spool is chosen so the user re-confirms before the load starts.
    spoolman_panel.request_select_for_print([this]() { this->show_load_filament_dialog(); });
    spoolman_panel.foreground();
  }
}

// ---- estado vindo do Moonraker -------------------------------------------------

void ExtruderPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto target_value = j["/params/0/extruder/target"_json_pointer];
  if (!target_value.is_null()) {
    current_target = target_value.template get<int>();
  }

  auto temp_value = j["/params/0/extruder/temperature"_json_pointer];
  if (!temp_value.is_null()) {
    current_temp = temp_value.template get<int>();
  }

  // começou/parou/pausou uma impressão: refaz a trava dos botões
  auto &pstate = j["/params/0/print_stats/state"_json_pointer];
  if (!pstate.is_null()) {
    refresh_button_state();
  }

  // Uma impressão começou (Orca, Mainsail...) ou foi retomada com uma ação do
  // filamento em andamento: nada mais é mandado ao bico. Não esfria (a
  // impressão usa o bico). Nenhum G-code novo, só deixa de mandar.
  if (travado_imprimindo()) {
    bool cancelou = false;
    if (pending_kind != PA_NONE) {
      auto_cool_after = false;
      clear_pending();
      troca_resultado(false);
      cancelou = true;
    }
    if (load_active && !load_stop) {
      auto_cool_after = false;
      load_stop = true;            // o pedaço em andamento termina e para
      cancelou = true;
    }
    if (troca_etapa >= 0) {
      troca_etapa = -1;
      troca_passo = false;
      dialogo_da_troca = false;
      troca.fechar();
      refresh_button_state();
      cancelou = true;
    }
    if (cancelou) {
      spdlog::warn("impressão começou: ação do filamento cancelada");
      KUtils::notify_toast("A impressão começou: a ação do filamento foi cancelada.", 3000);
    }
  }

  if (pending_kind != PA_NONE && current_temp + HEAT_TOLERANCE >= pending_want) {
    fire_pending();
  } else {
    // linha do cartão ("Aquecendo... 180/240°C") e frase da troca
    atualizar_estado();
  }
}

int ExtruderPanel::effective_temp() {
  return std::max(temp_escolhida(), current_target);
}

bool ExtruderPanel::travado_imprimindo() {
  // Igual à trava de antes (que ficava no botão que abria esta tela):
  // imprimindo e não pausada. Pausada libera (troca de cor, falta de filamento).
  return KUtils::is_printing() && !KUtils::is_paused();
}

// ---- botões ------------------------------------------------------------------

void ExtruderPanel::pintar(Acao &a, bool modo_parar) {
  Marca::Tipo t = modo_parar ? Marca::Tipo::PERIGO : Marca::Tipo::NORMAL;
  const char *txt = modo_parar ? "Parar" : a.texto;
  if (a.bc != NULL) {
    a.bc->set_tipo(t);
  } else {
    Marca::estilo_botao(a.obj, t);
  }
  Marca::texto(a.rotulo, txt);
}

void ExtruderPanel::habilitar(Acao &a, bool ok) {
  if (a.bc != NULL) {
    if (ok) {
      a.bc->enable();
    } else {
      a.bc->disable();
    }
  } else if (ok) {
    lv_obj_clear_state(a.obj, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(a.obj, LV_STATE_DISABLED);
  }
}

void ExtruderPanel::refresh_button_state() {
  bool busy = ocupado();
  travado = travado_imprimindo();

  if (!busy) {
    // nada rodando: só a troca (se estiver no meio) continua "dona" do Trocar
    botao_ativo = troca_etapa >= 0 ? &acao_trocar : NULL;
  }

  // "Parar" só enquanto dá para parar: esperando aquecer ou carregando em
  // pedaços. Um G-code único em andamento (descarregar, extrudar, esfriar) não
  // tem como ser interrompido: aí tudo fica desabilitado até a resposta, como
  // antes.
  bool pode_parar = !action_in_flight && (pending_kind != PA_NONE || load_active);
  Acao *parar_em = pode_parar ? botao_ativo : NULL;
  if (parar_em != botao_parar) {
    if (botao_parar != NULL) {
      pintar(*botao_parar, false);
    }
    if (parar_em != NULL) {
      pintar(*parar_em, true);
    }
    botao_parar = parar_em;
  }

  Acao *todas[] = { &acao_carregar, &acao_descarregar, &acao_trocar, &acao_extrudar, &acao_retrair };
  for (Acao *a : todas) {
    bool ok;
    if (a == botao_parar) {
      ok = true;                       // o Parar fica sempre à mão
    } else if (troca_etapa >= 0 && a == &acao_trocar) {
      ok = true;                       // no meio da troca: Trocar reabre a subtela
    } else if (busy || travado || troca_etapa >= 0) {
      ok = false;
    } else {
      ok = true;
    }
    habilitar(*a, ok);
  }

  atualizar_estado();
  vigiar_troca();
}

void ExtruderPanel::vigiar_troca() {
  if (troca_espera == NULL) {
    return;  // ainda no construtor
  }
  // conta só enquanto a troca está parada esperando o dono; recomeça do zero
  // a cada vez que ela volta a ficar parada
  bool parada = troca_etapa >= 0 && !ocupado();
  if (parada && !troca_espera_ativa) {
    lv_timer_reset(troca_espera);
    lv_timer_resume(troca_espera);
    troca_espera_ativa = true;
  } else if (!parada && troca_espera_ativa) {
    lv_timer_pause(troca_espera);
    troca_espera_ativa = false;
  }
}

void ExtruderPanel::atualizar_estado() {
  if (estado_label == NULL) {
    return;
  }

  std::string txt;
  uint32_t ponto = 0;      // 0 = sem bolinha
  bool duas_linhas = false;

  if (load_active) {
    // Show progress, so the slow feed never looks like a hang.
    txt = fmt::format("Carregando... {}/{} mm", LOAD_TOTAL_MM - load_remaining_mm, LOAD_TOTAL_MM);
    ponto = Marca::LIMAO;
  } else if (pending_kind != PA_NONE) {
    // Heating toward the action: live current/target so the number climbs.
    txt = fmt::format("Aquecendo... {}/{}°C", current_temp, pending_want);
    ponto = Marca::LIMAO;
  } else if (action_in_flight) {
    txt = action_name;
    ponto = Marca::LIMAO;
  } else if (travado) {
    txt = "Indisponível durante a impressão";
    duas_linhas = true;
  } else if (troca_etapa >= 0) {
    txt = fmt::format("Troca: etapa {} de 4", std::min(troca_etapa + 1, 4));
    ponto = Marca::LIMAO;
  } else {
    // parado: temperatura do bico e o estado com palavra (como na aba
    // Temperatura)
    const char *est;
    if (current_target <= 0) {
      est = "Desligado";
    } else if (std::abs(current_temp - current_target) <= 2) {
      est = "No alvo";
      ponto = Marca::OK_PONTO;
    } else if (current_temp < current_target) {
      est = "Aquecendo...";
      ponto = Marca::LIMAO;
    } else {
      est = "Esfriando...";
    }
    txt = fmt::format("Bico {}°C · {}", current_temp, est);
  }

  Marca::texto(estado_label, txt.c_str());
  lv_obj_set_y(estado_label, duas_linhas ? 40 : 48);

  // só mexe na bolinha quando ela muda (consume chama isto várias vezes por segundo)
  if (ponto != ponto_mostrado) {
    if (ponto == 0) {
      lv_obj_add_flag(estado_ponto, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_set_style_bg_color(estado_ponto, lv_color_hex(ponto), 0);
      lv_obj_clear_flag(estado_ponto, LV_OBJ_FLAG_HIDDEN);
    }
    ponto_mostrado = ponto;
  }

  if (troca_etapa >= 0) {
    atualizar_troca();
  }
}

// ---- execução das ações (lógica de antes) ------------------------------------

void ExtruderPanel::arm_safety_timer(uint32_t ms) {
  cancel_safety_timer();
  safety_timer = lv_timer_create([](lv_timer_t *t) {
    auto *self = (ExtruderPanel *)t->user_data;
    self->safety_timer = NULL;  // one-shot, LVGL deletes after this callback
    if (self->load_active) {
      // A load chunk's response never arrived - stop feeding rather than
      // leaving the load stuck mid-way with the buttons locked.
      spdlog::warn("load chunk response timeout, stopping load");
      self->finish_load();
      KUtils::notify_toast("O carregamento demorou demais.");
    } else if (self->action_in_flight) {
      // Response never arrived - assume the ws/klipper lost it and recover.
      spdlog::warn("extruder action response timeout, unlocking buttons");
      self->action_in_flight = false;
      self->refresh_button_state();
      self->troca_resultado(false);
    } else if (self->pending_kind != PA_NONE) {
      spdlog::warn("extruder heat-up timed out, clearing pending action");
      self->clear_pending();
      KUtils::notify_toast("O aquecimento demorou demais.");
      self->troca_resultado(false);
    }
  }, ms, this);
  lv_timer_set_repeat_count(safety_timer, 1);
}

void ExtruderPanel::cancel_safety_timer() {
  if (safety_timer != NULL) {
    lv_timer_del(safety_timer);
    safety_timer = NULL;
  }
}

void ExtruderPanel::send_action(const std::string &gcode) {
  action_in_flight = true;
  refresh_button_state();
  arm_safety_timer(ACTION_TIMEOUT_MS);
  // Moonraker only sends the JSON-RPC response for printer.gcode.script once
  // the script has finished executing - that's our reliable "done" signal.
  ws.gcode_script(gcode, [this](json &) {
    std::lock_guard<std::mutex> lock(lv_lock);
    on_action_response();
  });
}

void ExtruderPanel::on_action_response() {
  if (!action_in_flight) {
    return;  // safety timer already recovered us
  }
  action_in_flight = false;
  cancel_safety_timer();
  refresh_button_state();
  maybe_auto_cooldown();  // extrude/retract/unload finished -> drop the heater
  troca_resultado(true);  // etapa da troca terminou (sem efeito fora da troca)
}

void ExtruderPanel::maybe_auto_cooldown() {
  if (!auto_cool_after) {
    return;
  }
  auto_cool_after = false;  // clear first so the cooldown action can't re-trigger
  if (KUtils::is_printing()) {
    // Impressão pausada (troca de cor no meio da impressão) ou em andamento:
    // não desliga o bico, senão a impressão voltaria com o bico frio.
    refresh_button_state();
    return;
  }
  action_name = "Esfriando...";
  botao_ativo = NULL;
  // Wait for all queued extruder moves to FINISH before dropping the heater.
  // Moonraker's gcode.script "done" fires when commands are queued, not when the
  // motion completes -- and _GUPPY_QUIT_MATERIAL has no trailing M400 -- so without
  // this the heater starts cooling mid load/unload and the filament jams half in/out
  // (the "not completely loaded/unloaded" report). M400 blocks until motion is done.
  send_action("M400\n" + cooldown_macro);
}

void ExtruderPanel::clear_pending() {
  if (pending_kind == PA_NONE) {
    return;
  }
  pending_kind = PA_NONE;
  pending_gcode.clear();
  pending_want = 0;
  cancel_safety_timer();
  refresh_button_state();
}

void ExtruderPanel::executar_agora(PendingKind kind, const std::string &gcode) {
  if (kind == PA_LOAD) {
    begin_load();    // chunked, stoppable - not a one-shot macro
  } else if (kind == PA_AQUECER) {
    // só aquecer (etapa 1 da troca): chegou na temperatura, etapa feita
    refresh_button_state();
    troca_resultado(true);
  } else {
    send_action(gcode);
  }
}

void ExtruderPanel::fire_pending() {
  PendingKind kind = pending_kind;
  std::string gcode = pending_gcode;
  clear_pending();   // resets pending state + button enables; the call below re-locks
  // action_name carries over so the caption stays correct.
  executar_agora(kind, gcode);
}

void ExtruderPanel::begin_load() {
  load_active = true;
  load_stop = false;
  load_remaining_mm = LOAD_TOTAL_MM;
  action_name = "Carregando...";
  refresh_button_state();  // caption; the action's button stays live as Stop
  send_load_chunk();
}

void ExtruderPanel::send_load_chunk() {
  if (!load_active) {
    return;
  }
  if (load_stop || load_remaining_mm <= 0) {
    finish_load(!load_stop);  // natural completion only when not stopped
    return;
  }
  int chunk = std::min(LOAD_CHUNK_MM, load_remaining_mm);
  int fed_before = LOAD_TOTAL_MM - load_remaining_mm;  // mm fed before this chunk
  load_remaining_mm -= chunk;
  // Two-phase: fast until the approach length is fed, then slow to purge.
  int feed = (fed_before < LOAD_FAST_MM) ? LOAD_FAST_FEED : LOAD_SLOW_FEED;
  atualizar_estado();
  // Per-chunk watchdog: if the response never arrives (ws died), stop feeding
  // rather than hang. The next chunk is only sent from the response callback,
  // so the feed naturally halts the moment load_stop is set.
  arm_safety_timer(ACTION_TIMEOUT_MS);
  ws.gcode_script(fmt::format("M83\nG1 E{} F{}", chunk, feed),
    [this](json &) {
      std::lock_guard<std::mutex> lock(lv_lock);
      cancel_safety_timer();
      send_load_chunk();  // feed the next chunk, or finish_load() when done/stopped
    });
}

void ExtruderPanel::finish_load(bool natural) {
  load_active = false;
  load_stop = false;
  load_remaining_mm = 0;
  cancel_safety_timer();
  refresh_button_state();
  if (natural) {
    maybe_auto_cooldown();  // load fed to completion -> drop the heater
  }
  troca_resultado(natural);
}

void ExtruderPanel::run_when_hot(PendingKind kind, const std::string &name,
                                 const std::string &gcode, bool esfriar_depois) {
  action_name = name;
  // Extrude/retract/load/unload all heat the hotend just for the action; cool it
  // back down once the action completes (on_action_response / finish_load).
  // Na troca de filamento o bico fica quente entre as etapas (esfria no fim).
  auto_cool_after = esfriar_depois;
  int want = effective_temp();

  // Non-blocking heat request. Avoids the M109 hang from #65 - we watch the
  // temp stream ourselves and run the action once we're hot enough. Applies to
  // load/unload too, so the user's selected temp is honoured regardless of
  // which filament macro is configured.
  if (want > current_target) {
    ws.gcode_script(fmt::format("M104 S{}", want));
  }

  if (current_temp + HEAT_TOLERANCE >= want) {
    executar_agora(kind, gcode);
    return;
  }

  // Cold - queue it. consume() will fire it once we reach the threshold.
  pending_kind = kind;
  pending_gcode = gcode;
  pending_want = want;
  refresh_button_state();
  arm_safety_timer(HEAT_TIMEOUT_MS);
}

std::string ExtruderPanel::gcode_descarregar() {
  // Heat to the selected temp first (run_when_hot), then run the macro.
  // _GUPPY_QUIT_MATERIAL also takes EXTRUDER_TEMP; a plain UNLOAD_FILAMENT
  // macro is whatever the user defined, but now always runs hot.
  return unload_filament_macro == "_GUPPY_QUIT_MATERIAL"
    ? fmt::format("{} EXTRUDER_TEMP={}", unload_filament_macro, effective_temp())
    : unload_filament_macro;
}

void ExtruderPanel::parar() {
  // O mesmo caminho do antigo "Esfriar" durante uma ação. Um G-code único já
  // em andamento não tem como parar (o botão fica desabilitado nessa hora).
  if (action_in_flight) {
    return;
  }
  // Stop: this IS the cooldown, so don't let it auto-chain another.
  auto_cool_after = false;

  // Parar encerra também a troca de filamento (Cancelar dela vem para cá).
  bool estava_na_troca = troca_etapa >= 0;
  troca_passo = false;
  troca_etapa = -1;
  dialogo_da_troca = false;
  if (estava_na_troca) {
    troca.fechar();
  }

  // Com impressão pausada ou em andamento: para a ação mas não desliga o bico
  // (a impressão precisa dele quente).
  bool esfriar = !KUtils::is_printing();

  if (load_active) {
    // Stop the load: send_load_chunk() won't queue another chunk, and we
    // drop the heater now. finish_load() runs from the in-flight chunk's
    // response, so motion ceases within ~one chunk.
    load_stop = true;
    if (esfriar) {
      ws.gcode_script(cooldown_macro);
    }
    KUtils::notify_toast("Parando o carregamento...");
    refresh_button_state();
    return;
  }

  clear_pending();
  if (esfriar) {
    action_name = "Esfriando...";
    botao_ativo = NULL;
    send_action(cooldown_macro);
  } else {
    refresh_button_state();
  }
}

void ExtruderPanel::handle_callback(lv_event_t *e) {
  spdlog::trace("handling extruder panel callback");
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *selector = lv_event_get_target(e);
    uint32_t idx = lv_btnmatrix_get_selected_btn(selector);
    if (selector == length_selector.get_selector() && idx != LV_BTNMATRIX_BTN_NONE) {
      length_selector.set_selected_idx(idx);
    }
    return;
  }

  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(e);

  // − / + só mudam o número na tela (não mandam nada para a impressora)
  if (btn == menos_btn) {
    mudar_temp(-1);
    return;
  }
  if (btn == mais_btn) {
    mudar_temp(+1);
    return;
  }

  // O botão que está mostrando "Parar"
  if (botao_parar != NULL && btn == botao_parar->obj) {
    parar();
    return;
  }

  // Trocar com a troca já começada: reabre a subtela onde parou
  if (btn == acao_trocar.obj && troca_etapa >= 0) {
    troca.abrir();
    atualizar_troca();
    troca_espera_ativa = false;   // o dono voltou: a espera recomeça
    if (troca_espera != NULL) {
      lv_timer_pause(troca_espera);
    }
    vigiar_troca();
    return;
  }

  // Remaining actions: blocked while another action is running OR while a
  // heat-up is pending. refresh_button_state() also disables their widgets,
  // but check the flags too in case of an event in flight before refresh.
  if (ocupado()) {
    return;
  }

  // Trava da impressão (a mesma de antes): imprimindo e não pausada
  if (travado_imprimindo()) {
    KUtils::notify_locked();
    return;
  }

  // Bico frio: avisa antes de aquecer (um toque sem querer não liga o bico).
  // Com o bico já quente a ação roda direto.
  bool aquece = btn == acao_trocar.obj || btn == acao_extrudar.obj || btn == acao_retrair.obj ||
                btn == acao_descarregar.obj || btn == acao_carregar.obj;
  if (aquece && troca_etapa < 0 && current_temp + HEAT_TOLERANCE < effective_temp()) {
    KUtils::confirmar(fmt::format("Vai aquecer o bico a {}°C.\nContinuar?", effective_temp()),
      [this, btn]() {
        // a situação pode ter mudado enquanto a janela estava aberta
        if (ocupado() || troca_etapa >= 0) {
          return;
        }
        if (travado_imprimindo()) {
          KUtils::notify_locked();
          return;
        }
        executar_acao(btn);
      });
    return;
  }
  executar_acao(btn);
}

void ExtruderPanel::executar_acao(lv_obj_t *btn) {
  if (btn == acao_trocar.obj) {
    troca_iniciar();
    return;
  }

  if (troca_etapa >= 0) {
    return;  // no meio da troca só o Trocar responde
  }

  if (btn == acao_extrudar.obj || btn == acao_retrair.obj) {
    bool retract = btn == acao_retrair.obj;
    uint32_t li = length_selector.get_selected_idx();
    int len = COMPRIMENTOS[li < 3 ? li : 0];
    int feed = EXTRUDE_SPEED_MM_S * 60;
    botao_ativo = retract ? &acao_retrair : &acao_extrudar;
    run_when_hot(retract ? PA_RETRACT : PA_EXTRUDE,
      retract ? "Puxando..." : "Empurrando...",
      fmt::format("M83\nG1 E{}{} F{}", retract ? "-" : "", len, feed));
    return;
  }

  if (btn == acao_descarregar.obj) {
    botao_ativo = &acao_descarregar;
    run_when_hot(PA_UNLOAD, "Descarregando...", gcode_descarregar());
    return;
  }

  if (btn == acao_carregar.obj) {
    // Loading deducts from the active Spoolman spool (Moonraker tracks raw
    // E-axis movement, not just prints), so confirm the spool first when
    // Spoolman is configured. Yes -> load, No -> open the Spoolman panel.
    // The load itself heats then feeds in bounded, stoppable chunks
    // (run_when_hot -> begin_load), bypassing the uninterruptible stock macro.
    if (spoolman_enabled) {
      dialogo_da_troca = false;
      show_load_filament_dialog();
    } else {
      botao_ativo = &acao_carregar;
      run_when_hot(PA_LOAD, "Carregando...", "");
    }
    return;
  }
}

// ---- troca de filamento (quadro 10) ------------------------------------------
// Cada etapa usa as mesmas funções dos botões da aba: aquecer e esperar
// (run_when_hot), descarregar (macro do config), carregar em pedaços
// (begin_load) e extrudar 10 mm para purgar. O bico fica quente entre as
// etapas e esfria no "Concluir" ou no "Cancelar" (que é o mesmo "Parar").

void ExtruderPanel::troca_iniciar() {
  troca_etapa = 0;
  troca_passo = false;
  troca.abrir();
  troca_executar();  // a etapa 1 (aquecer) começa sozinha
}

void ExtruderPanel::troca_executar() {
  if (troca_etapa < 0 || ocupado()) {
    return;
  }
  if (travado_imprimindo()) {
    KUtils::notify_locked();
    return;
  }

  botao_ativo = &acao_trocar;
  switch (troca_etapa) {
  case 0:
    troca_passo = true;
    run_when_hot(PA_AQUECER, "Aquecendo...", "", false);
    break;
  case 1:
    troca_passo = true;
    run_when_hot(PA_UNLOAD, "Descarregando...", gcode_descarregar(), false);
    break;
  case 2:
    if (spoolman_enabled) {
      // a mesma pergunta do Carregar; o "Sim" continua a etapa
      dialogo_da_troca = true;
      show_load_filament_dialog();
    } else {
      troca_passo = true;
      run_when_hot(PA_LOAD, "Carregando...", "", false);
    }
    break;
  default:  // 3 (purgar) e 4 (purgar mais)
    troca_passo = true;
    run_when_hot(PA_EXTRUDE, "Purgando...",
      fmt::format("M83\nG1 E{} F{}", PURGA_MM, EXTRUDE_SPEED_MM_S * 60), false);
    break;
  }
  atualizar_troca();
}

void ExtruderPanel::troca_resultado(bool ok) {
  if (!troca_passo) {
    return;
  }
  troca_passo = false;
  if (ok && troca_etapa >= 0 && troca_etapa < 4) {
    troca_etapa++;
  }
  // deu errado (tempo esgotado): fica na mesma etapa; Continuar tenta de novo
  refresh_button_state();
}

void ExtruderPanel::troca_esquerdo() {
  if (troca_etapa >= 4 && !ocupado()) {
    troca_executar();  // "Purgar mais"
    return;
  }
  parar();             // "Cancelar": o mesmo caminho do Parar
}

void ExtruderPanel::troca_direito() {
  if (ocupado()) {
    return;
  }
  if (troca_etapa >= 4) {
    troca_concluir();
    return;
  }
  troca_executar();    // "Continuar"
}

void ExtruderPanel::troca_concluir() {
  troca_etapa = -1;
  troca_passo = false;
  dialogo_da_troca = false;
  troca.fechar();
  // fim da troca: esfria o bico como no fim das outras ações (com o M400
  // antes, para a purga terminar de sair)
  auto_cool_after = true;
  maybe_auto_cooldown();
  refresh_button_state();
}

void ExtruderPanel::atualizar_troca() {
  if (troca_etapa < 0) {
    return;
  }

  TrocaFilamentoPanel::Vista v;
  v.etapa = troca_etapa;
  v.temp = pending_kind != PA_NONE ? pending_want : effective_temp();

  bool busy = ocupado();
  if (pending_kind != PA_NONE) {
    v.frase = fmt::format("Aquecendo o bico: {}/{}°C", current_temp, pending_want);
  } else if (load_active) {
    v.frase = fmt::format("Carregando... {}/{} mm", LOAD_TOTAL_MM - load_remaining_mm, LOAD_TOTAL_MM);
  } else if (action_in_flight) {
    v.frase = action_name;
  } else {
    switch (troca_etapa) {
    case 0:  v.frase = "Toque em Continuar para aquecer o bico."; break;
    case 1:  v.frase = "Toque em Continuar para descarregar."; break;
    case 2:  v.frase = "Carregue o filamento novo e toque em Continuar."; break;
    case 3:  v.frase = fmt::format("Toque em Continuar para purgar {} mm.", PURGA_MM); break;
    default: v.frase = "Saiu limpo? Toque em Concluir."; break;
    }
  }

  if (troca_etapa >= 4 && !busy) {
    v.esquerdo = "Purgar mais";
    v.direito = LV_SYMBOL_OK " Concluir";
  } else {
    v.esquerdo = "Cancelar";
    v.direito = troca_etapa >= 4 ? LV_SYMBOL_OK " Concluir" : LV_SYMBOL_RIGHT " Continuar";
  }
  // Cancelar para a espera do aquecimento ou a carga; um G-code único em
  // andamento (descarregar, purgar) não tem como parar
  v.esquerdo_ok = !action_in_flight;
  v.direito_ok = !busy;

  troca.mostrar(v);
}

#ifdef SIMULATOR
void ExtruderPanel::sim_show_busy() {
  // Pretend Extrude was pressed cold: heating toward target, action buttons
  // disabled, caption visible. We don't send_action() (no websocket; the safety
  // timer would also fire ACTION_TIMEOUT_MS later) - just stage the pending
  // state so the heat-up caption can be visually verified.
  action_name = "Empurrando...";
  botao_ativo = &acao_extrudar;
  pending_kind = PA_EXTRUDE;
  pending_want = 220;
  current_temp = 25;
  refresh_button_state();
  Navegacao::ir_para(Aba::FILAMENTO);
}
#endif
