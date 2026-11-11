#include "print_status_panel.h"

#include <cstdlib>
#include "finetune_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>

// Acompanhamento (Proposta B, quadros 04 e 05). Coordenadas da subtela
// (424 x 272): x = x_da_maquete - 56, y igual.

namespace {
  // Nome mostrado: sem o ".gcode" do fim (o caminho guardado não muda).
  std::string nome_sem_gcode(const std::string &fname) {
    const std::string ext = ".gcode";
    if (fname.size() > ext.size()
        && fname.compare(fname.size() - ext.size(), ext.size(), ext) == 0) {
      return fname.substr(0, fname.size() - ext.size());
    }
    return fname;
  }

  // Rótulo com largura fixa, uma linha só, "..." no fim se não couber.
  lv_obj_t *rotulo_fixo(lv_obj_t *pai, const char *txt, const lv_font_t *f, uint32_t cor,
                        lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_text_align_t alinhamento) {
    lv_obj_t *l = Marca::criar_rotulo(pai, txt, f, cor);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    // sempre uma linha só: com a altura solta, o LVGL 8.3 deixa a caixa
    // crescer com o texto inteiro antes de pôr os "..."
    lv_obj_set_height(l, lv_font_get_line_height(f));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(l, alinhamento, 0);
    return l;
  }
}

PrintStatusPanel::PrintStatusPanel(KWebSocketClient &websocket_client,
				   std::mutex &lock,
				   lv_obj_t *mini_parent)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , finetune_panel(websocket_client, lock)
  , zoffset_panel(websocket_client, lock)
  , exclude_object_panel(websocket_client, lock)
  , status_cont(lv_obj_create(lv_scr_act()))
  , complete_cont(lv_obj_create(lv_scr_act()))
  , estimated_time_s(0)
  , extruder_target(-1)
  , heater_bed_target(-1)
{
  (void)mini_parent;  // a sobreposição pequena do Início saiu

  // ---- acompanhamento: subtela registrada (pad 0, PRETO, 424 x 272) ----
  Navegacao::subtela(status_cont);
  Navegacao::registrar_acompanhamento(status_cont);

  // miniatura: cartão 128 x 128 em (8,8); sem miniatura, ícone de arquivo
  thumb_card = Marca::criar_cartao(status_cont, 8, 8, 128, 128);
  sem_thumb = Marca::criar_rotulo(thumb_card, LV_SYMBOL_FILE, Marca::f20(), Marca::GRAFITE);
  lv_obj_center(sem_thumb);
  thumbnail = lv_img_create(thumb_card);
  lv_obj_clear_flag(thumbnail, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);

  // chip de estado em (148,8) 112 x 24
  chip = Marca::criar_chip(status_cont);
  lv_obj_set_pos(chip, 148, 8);
  lv_obj_set_width(chip, 112);
  Marca::chip_estado(chip, Marca::Estado::IMPRIMINDO, "Imprimindo");

  camada_label = rotulo_fixo(status_cont, "", Marca::f12(), Marca::GRAFITE,
                             276, 11, 140, LV_TEXT_ALIGN_RIGHT);
  lv_label_set_recolor(camada_label, true);   // número em PAPEL, como na maquete
  filename_label = rotulo_fixo(status_cont, "", Marca::f14(), Marca::PAPEL,
                               148, 38, 268, LV_TEXT_ALIGN_LEFT);

  // porcentagem: f40 (sem acento: só dígitos e %), caixa 120 x 44
  progress_label = rotulo_fixo(status_cont, "0%", Marca::f40(), Marca::PAPEL,
                               148, 52, 120, LV_TEXT_ALIGN_LEFT);
  lv_obj_set_height(progress_label, 44);

  falta_titulo = rotulo_fixo(status_cont, "Falta", Marca::f12(), Marca::GRAFITE,
                             276, 60, 140, LV_TEXT_ALIGN_RIGHT);
  falta_label = rotulo_fixo(status_cont, "--", Marca::f20(), Marca::PAPEL,
                            256, 75, 160, LV_TEXT_ALIGN_RIGHT);

  // barra 268 x 8 (trilho PRETO_3, indicador LIMAO pelo tema), sem animação
  progress_bar = lv_bar_create(status_cont);
  lv_obj_set_pos(progress_bar, 148, 104);
  lv_obj_set_size(progress_bar, 268, 8);
  lv_bar_set_range(progress_bar, 0, 100);
  lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
  lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_CLICKABLE);

  decorrido_label = rotulo_fixo(status_cont, "", Marca::f12(), Marca::GRAFITE,
                                148, 120, 140, LV_TEXT_ALIGN_LEFT);
  lv_label_set_recolor(decorrido_label, true);
  total_label = rotulo_fixo(status_cont, "", Marca::f12(), Marca::GRAFITE,
                            276, 120, 140, LV_TEXT_ALIGN_RIGHT);

  // preparo: no mesmo lugar do progresso (148,58 a 416,136), escondido
  preparo_cont = lv_obj_create(status_cont);
  lv_obj_set_pos(preparo_cont, 148, 58);
  lv_obj_set_size(preparo_cont, 268, 78);
  lv_obj_set_style_bg_opa(preparo_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(preparo_cont, 0, 0);
  lv_obj_set_style_pad_all(preparo_cont, 0, 0);
  lv_obj_clear_flag(preparo_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(preparo_cont, LV_OBJ_FLAG_CLICKABLE);
  preparo_titulo = rotulo_fixo(preparo_cont, "", Marca::f12(), Marca::GRAFITE,
                               0, 0, 268, LV_TEXT_ALIGN_LEFT);
  preparo_etapa = rotulo_fixo(preparo_cont, "", Marca::f20(), Marca::PAPEL,
                              0, 18, 268, LV_TEXT_ALIGN_LEFT);
  preparo_detalhe = rotulo_fixo(preparo_cont, "", Marca::f12(), Marca::GRAFITE,
                                0, 46, 268, LV_TEXT_ALIGN_LEFT);
  for (int i = 0; i < ETAPAS; i++) {
    // 7 traços de 34 x 6 com 5 px entre eles (7*34 + 6*5 = 268)
    lv_obj_t *p = lv_obj_create(preparo_cont);
    lv_obj_set_pos(p, i * 39, 68);
    lv_obj_set_size(p, 34, 6);
    lv_obj_set_style_radius(p, 3, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(p, lv_color_hex(Marca::PRETO_3), 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    preparo_pontos[i] = p;
  }
  lv_obj_add_flag(preparo_cont, LV_OBJ_FLAG_HIDDEN);

  // 4 cartões 96 x 56 em y = 144
  bico       = criar_cartao_info(8,   "Bico", "--");
  mesa       = criar_cartao_info(112, "Mesa", "--");
  velocidade = criar_cartao_info(216, "Velocidade", "100%");
  ventoinha  = criar_cartao_info(320, "Ventoinha", "--");

  // faixa de baixo: altura 52 em y = 212
  pause_btn = Marca::criar_botao(status_cont, LV_SYMBOL_PAUSE " Pausar", Marca::Tipo::NORMAL);
  lv_obj_set_pos(pause_btn, 8, 212);
  lv_obj_set_size(pause_btn, 130, 52);
  lv_obj_add_event_cb(pause_btn, &PrintStatusPanel::_handle_callback, LV_EVENT_CLICKED, this);

  resume_btn = Marca::criar_botao(status_cont, LV_SYMBOL_PLAY " Retomar", Marca::Tipo::DESTAQUE);
  lv_obj_set_pos(resume_btn, 8, 212);
  lv_obj_set_size(resume_btn, 130, 52);
  lv_obj_add_event_cb(resume_btn, &PrintStatusPanel::_handle_callback, LV_EVENT_CLICKED, this);
  lv_obj_add_flag(resume_btn, LV_OBJ_FLAG_HIDDEN);

  finetune_btn = Marca::criar_botao(status_cont, LV_SYMBOL_EDIT " Ajuste fino", Marca::Tipo::NORMAL);
  lv_obj_set_pos(finetune_btn, 147, 212);
  lv_obj_set_size(finetune_btn, 131, 52);
  lv_obj_add_event_cb(finetune_btn, &PrintStatusPanel::_handle_callback, LV_EVENT_CLICKED, this);

  cancel_btn = Marca::criar_botao(status_cont, LV_SYMBOL_CLOSE " Cancelar", Marca::Tipo::CONTORNO_PERIGO);
  lv_obj_set_pos(cancel_btn, 287, 212);
  lv_obj_set_size(cancel_btn, 129, 52);
  lv_obj_add_event_cb(cancel_btn, &PrintStatusPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // "Objetos" mora agora no cabeçalho do Ajuste fino
  finetune_panel.set_objetos_cb([this]() { exclude_object_panel.foreground(); });

  // ---- tela de concluída: subtela própria, sem cabeçalho ----
  Navegacao::subtela(complete_cont);

  complete_selo = Marca::criar_selo(complete_cont, 56, Marca::OK, Marca::PAPEL, LV_SYMBOL_OK);
  lv_obj_set_pos(complete_selo, 184, 40);

  complete_title = rotulo_fixo(complete_cont, "Impressão concluída", Marca::f20(), Marca::PAPEL,
                               0, 108, Marca::CONT_L, LV_TEXT_ALIGN_CENTER);
  complete_file = rotulo_fixo(complete_cont, "", Marca::f14(), Marca::PAPEL,
                              12, 138, 400, LV_TEXT_ALIGN_CENTER);
  complete_stats = rotulo_fixo(complete_cont, "", Marca::f12(), Marca::GRAFITE,
                               0, 160, Marca::CONT_L, LV_TEXT_ALIGN_CENTER);

  complete_done_btn = Marca::criar_botao(complete_cont, "OK", Marca::Tipo::NORMAL);
  lv_obj_set_pos(complete_done_btn, 8, 212);
  lv_obj_set_size(complete_done_btn, 130, Marca::TOQUE);
  lv_obj_add_event_cb(complete_done_btn, &PrintStatusPanel::_handle_callback, LV_EVENT_CLICKED, this);

  complete_again_btn = Marca::criar_botao(complete_cont, LV_SYMBOL_PLAY " Imprimir de novo",
                                          Marca::Tipo::DESTAQUE);
  lv_obj_set_pos(complete_again_btn, 146, 212);
  lv_obj_set_size(complete_again_btn, 270, Marca::TOQUE);
  lv_obj_add_event_cb(complete_again_btn, &PrintStatusPanel::_handle_callback, LV_EVENT_CLICKED, this);

  ws.register_notify_update(this);
}

PrintStatusPanel::~PrintStatusPanel() {
  if (status_cont != NULL) {
    lv_obj_del(status_cont);
    status_cont = NULL;
  }
  if (complete_cont != NULL) {
    lv_obj_del(complete_cont);
    complete_cont = NULL;
  }

  ws.unregister_notify_update(this);
}

PrintStatusPanel::Cartao PrintStatusPanel::criar_cartao_info(lv_coord_t x, const char *nome,
                                                             const char *valor) {
  Cartao c;
  c.cont = Marca::criar_cartao(status_cont, x, 144, 96, 56);
  Marca::cartao_tocavel(c.cont);
  lv_obj_add_event_cb(c.cont, &PrintStatusPanel::_handle_callback, LV_EVENT_CLICKED, this);
  rotulo_fixo(c.cont, nome, Marca::f12(), Marca::GRAFITE, 10, 8, 76, LV_TEXT_ALIGN_LEFT);
  c.valor = rotulo_fixo(c.cont, valor, Marca::f16(), Marca::PAPEL, 10, 27, 76, LV_TEXT_ALIGN_LEFT);
  return c;
}

void PrintStatusPanel::foreground() {
  Navegacao::abrir_acompanhamento();
}

void PrintStatusPanel::background() {
  Navegacao::fechar(status_cont);
}

void PrintStatusPanel::show_completion(const std::string &state) {
  State *s = State::get_instance();
  bool ok = (state == "complete");

  lv_obj_set_style_bg_color(complete_selo, lv_color_hex(ok ? Marca::OK : Marca::ERRO), 0);
  lv_obj_t *selo_txt = lv_obj_get_child(complete_selo, 0);
  if (selo_txt != NULL) {
    Marca::texto(selo_txt, ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
  }
  Marca::texto(complete_title, ok ? "Impressão concluída" : "Impressão cancelada");

  auto fn = s->get_data("/printer_state/print_stats/filename"_json_pointer);
  complete_arquivo = fn.is_string() ? fn.template get<std::string>() : arquivo_atual;
  Marca::texto(complete_file, nome_sem_gcode(complete_arquivo).c_str());

  std::string stats;
  auto dur = s->get_data("/printer_state/print_stats/print_duration"_json_pointer);
  if (dur.is_number()) {
    stats = "Tempo " + KUtils::tempo_curto((int64_t)dur.template get<double>());
  }
  auto fil = s->get_data("/printer_state/print_stats/filament_used"_json_pointer);
  if (fil.is_number()) {
    if (!stats.empty()) stats += " · ";
    stats += fmt::format("Filamento {:.1f} m", fil.template get<double>() / 1000.0);
  }
  Marca::texto(complete_stats, stats.c_str());

  if (complete_arquivo.empty()) {
    lv_obj_add_flag(complete_again_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(complete_again_btn, LV_OBJ_FLAG_HIDDEN);
  }

  // por cima do que estiver na tela; NÃO acende a tela (sem lv_disp_trig_activity)
  Navegacao::fechar(status_cont);
  Navegacao::abrir(complete_cont);
}

void PrintStatusPanel::reset() {
  lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
  Marca::texto(progress_label, "0%");
  Marca::texto(falta_label, "--");
  Marca::texto(decorrido_label, "");
  Marca::texto(total_label, "");
  Marca::texto(camada_label, "");
  Marca::texto(filename_label, "");
  estimated_time_s = 0;
  current_progress = 0.0;
  current_filament_used = 0.0;
  pct_atual = -1;
  camada_atual = 0;
  camadas_total = 0;
  current_file = json();
  arquivo_atual.clear();
  extruder_target = -1;
  heater_bed_target = -1;
  fase_atual.clear();

  // impressão nova: os botões voltam a responder
  lv_obj_clear_state(pause_btn, LV_STATE_DISABLED);
  lv_obj_clear_state(resume_btn, LV_STATE_DISABLED);

  // solta a imagem anterior e mostra o ícone até a nova chegar
  lv_img_set_src(thumbnail, NULL);
  lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(sem_thumb, LV_OBJ_FLAG_HIDDEN);
}

void PrintStatusPanel::init(json &fans) {
  fan_speeds.clear();
  for (auto &f : fans.items()) {
    std::string fan_name = f.key();

    auto fan_value = State::get_instance()
      ->get_data(json::json_pointer(fmt::format("/printer_state/{}/value", fan_name)));
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      fan_speeds.insert({fan_name, v});
    }

    fan_value = State::get_instance()
      ->get_data(json::json_pointer(fmt::format("/printer_state/{}/speed", fan_name)));
    if (!fan_value.is_null()) {
      int v = static_cast<int>(fan_value.template get<double>() * 100);
      fan_speeds.insert({fan_name, v});
    }
  }
  atualizar_ventoinha();

  reset();
  populate();

  // Ao ligar a tela (ou reconectar) no meio de uma impressão, ela já mostra o
  // acompanhamento, como a de fábrica.
  json pstat_state = State::get_instance()
    ->get_data("/printer_state/print_stats/state"_json_pointer);
  last_print_state = pstat_state.is_string() ? pstat_state.template get<std::string>() : "";
  if (last_print_state == "printing" || last_print_state == "paused") {
    Navegacao::abrir_acompanhamento();
  } else {
    Navegacao::fechar(status_cont);
  }
}

void PrintStatusPanel::populate() {
  State* s = State::get_instance();
  json printfile = s->get_data("/printer_state/print_stats/filename"_json_pointer);
  if (printfile.is_string()) {
    const std::string fname = printfile.template get<std::string>();
    if (fname.length() > 0) {
      arquivo_atual = fname;
      Marca::texto(filename_label, nome_sem_gcode(fname).c_str());
      json fname_input = {{"filename", fname }};
      ws.send_jsonrpc("server.files.metadata", fname_input,
		      [fname, this](json &d) { this->handle_metadata(fname, d); });
    }
  }

  auto pstate = s->get_data("/printer_state/print_stats/state"_json_pointer);
  mostrar_estado(pstate.is_string() ? pstate.template get<std::string>() : "");

  // progress percentage
  auto v = s->get_data("/printer_state/virtual_sdcard/progress"_json_pointer);
  if (v.is_number()) {
    current_progress = v.template get<double>();
    atualizar_progresso(static_cast<int>(current_progress * 100));
  }

  auto fil_used = s->get_data("/printer_state/print_stats/filament_used"_json_pointer);
  if (fil_used.is_number()) {
    current_filament_used = fil_used.template get<double>();
  }

  ler_cartoes_do_estado();

  auto pd = s->get_data("/printer_state/print_stats/print_duration"_json_pointer);
  if (pd.is_number()) {
    update_time_progress(static_cast<uint32_t>(pd.template get<double>()));
  }

  json info = s->get_data("/printer_state/print_stats/info"_json_pointer);
  update_layers(info);

  // tela ligada no meio do preparo: pega a etapa atual
  auto msg = s->get_data("/printer_state/display_status/message"_json_pointer);
  if (msg.is_string() && msg.template get<std::string>().rfind("FASE:", 0) == 0) {
    fase_atual = msg.template get<std::string>().substr(5);
  }
  atualizar_preparo();
}

void PrintStatusPanel::atualizar_preparo() {
  State *s = State::get_instance();
  auto num = [s](const char *p) {
    auto v = s->get_data(json::json_pointer(p));
    return v.is_number() ? v.template get<double>() : 0.0;
  };
  auto pst = s->get_data("/printer_state/print_stats/state"_json_pointer);
  std::string estado = pst.is_string() ? pst.template get<std::string>() : "";
  bool preparando = (estado == "printing" || estado == "paused") && current_filament_used <= 0.0;

  lv_obj_t *progresso[] = {progress_label, falta_titulo, falta_label, progress_bar,
                           decorrido_label, total_label};
  if (preparando != preparo_visivel) {
    preparo_visivel = preparando;
    for (lv_obj_t *o : progresso) {
      if (preparando) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
    if (preparando) lv_obj_clear_flag(preparo_cont, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(preparo_cont, LV_OBJ_FLAG_HIDDEN);
    if (estado == "printing" || estado == "paused") {
      mostrar_estado(estado);
    }
  }
  if (!preparando) {
    return;
  }

  double bt = num("/printer_state/heater_bed/temperature");
  double bt_alvo = num("/printer_state/heater_bed/target");
  double et = num("/printer_state/extruder/temperature");
  double et_alvo = num("/printer_state/extruder/target");
  bool mesa_esquentando = bt_alvo > 0 && bt < bt_alvo - 1.5;
  bool bico_esquentando = et_alvo >= 170 && et < et_alvo - 2;

  int etapa;
  std::string nome, detalhe;
  if (fase_atual == "assentar") {
    etapa = 3; nome = "Mesa assentando";
    detalhe = "Uns 2 minutos, para a medição sair certa";
  } else if (fase_atual == "altura") {
    etapa = 4; nome = "Medindo a altura do bico";
    detalhe = "O bico encosta de leve na mesa";
  } else if (fase_atual == "mede") {
    etapa = 5; nome = "Medindo a mesa";
    detalhe = "Cada ponto é medido duas vezes";
  } else if (fase_atual == "confere") {
    etapa = 6; nome = "Conferindo a medição";
    detalhe = "Medindo o centro da mesa de novo";
  } else if (fase_atual == "ok" || fase_atual == "corrigiu") {
    etapa = 7;
    if (bico_esquentando) {
      nome = "Aquecendo o bico";
      detalhe = fmt::format("{} / {} °C · medição conferida", (int)et, (int)et_alvo);
    } else {
      nome = "Começando a imprimir";
      detalhe = fase_atual == "corrigiu" ? "Medição conferida, altura do bico corrigida"
                                         : "Medição conferida";
    }
  } else if (mesa_esquentando) {
    etapa = 1; nome = "Aquecendo a mesa";
    detalhe = fmt::format("{} / {} °C", (int)bt, (int)bt_alvo);
  } else if (bico_esquentando) {
    etapa = 7; nome = "Aquecendo o bico";
    detalhe = fmt::format("{} / {} °C", (int)et, (int)et_alvo);
  } else {
    etapa = 2; nome = "Zerando os eixos";
    detalhe = "Preparando para medir a mesa";
  }

  Marca::texto(preparo_titulo, fmt::format("Preparando a impressão · etapa {} de {}", etapa, ETAPAS).c_str());
  Marca::texto(preparo_etapa, nome.c_str());
  Marca::texto(preparo_detalhe, detalhe.c_str());
  for (int i = 0; i < ETAPAS; i++) {
    lv_obj_set_style_bg_color(preparo_pontos[i],
                              lv_color_hex(i < etapa ? Marca::LIMAO : Marca::PRETO_3), 0);
  }
}

void PrintStatusPanel::ler_cartoes_do_estado() {
  State *s = State::get_instance();

  auto v = s->get_data("/printer_state/extruder/target"_json_pointer);
  if (v.is_number()) extruder_target = v.template get<int>();

  v = s->get_data("/printer_state/heater_bed/target"_json_pointer);
  if (v.is_number()) heater_bed_target = v.template get<int>();

  v = s->get_data("/printer_state/extruder/temperature"_json_pointer);
  if (v.is_number()) {
    Marca::texto(bico.valor, fmt::format("{}°C", v.template get<int>()).c_str());
  }

  v = s->get_data("/printer_state/heater_bed/temperature"_json_pointer);
  if (v.is_number()) {
    Marca::texto(mesa.valor, fmt::format("{}°C", v.template get<int>()).c_str());
  }

  v = s->get_data("/printer_state/gcode_move/speed_factor"_json_pointer);
  if (v.is_number()) {
    Marca::texto(velocidade.valor, fmt::format("{}%",
      static_cast<int>(std::lround(v.template get<double>() * 100))).c_str());
  }
}

void PrintStatusPanel::mostrar_estado(const std::string &st) {
  if (st == "paused") {
    Marca::chip_estado(chip, Marca::Estado::PAUSADA, "Pausada");
    lv_obj_set_width(chip, 92);    // maquete: 92 px no "Pausada", 112 no "Imprimindo"
    lv_obj_add_flag(pause_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(resume_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    Marca::chip_estado(chip, Marca::Estado::IMPRIMINDO, preparo_visivel ? "Preparando" : "Imprimindo");
    lv_obj_set_width(chip, 112);
    lv_obj_add_flag(resume_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pause_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

void PrintStatusPanel::atualizar_progresso(int pct) {
  pct = std::max(0, std::min(100, pct));
  if (pct == pct_atual) {
    return;
  }
  pct_atual = pct;
  lv_bar_set_value(progress_bar, pct, LV_ANIM_OFF);
  Marca::texto(progress_label, fmt::format("{}%", pct).c_str());
  atualizar_resumo();
}

void PrintStatusPanel::atualizar_resumo() {
  std::string r = fmt::format("{}%", pct_atual < 0 ? 0 : pct_atual);
  if (camadas_total > 0 && camada_atual > 0) {
    r += fmt::format(" · camada {}/{}", camada_atual, camadas_total);
  }
  finetune_panel.set_resumo(r);
}

void PrintStatusPanel::atualizar_ventoinha() {
  if (fan_speeds.empty()) {
    Marca::texto(ventoinha.valor, "--");
  } else {
    Marca::texto(ventoinha.valor, fmt::format("{}%", fan_speeds.begin()->second).c_str());
  }
}

void PrintStatusPanel::handle_metadata(const std::string &gcode_file, json &j) {
  // Roda na thread do websocket: a miniatura (download, se preciso) é buscada
  // fora do lv_lock; o resto, com o lock preso.
  std::pair<std::string, std::pair<size_t, size_t>> thumb_detail;
  try {
    thumb_detail = KUtils::get_thumbnail(gcode_file, j, 120);
  } catch (const std::exception &e) {
    spdlog::warn("miniatura de {} ilegível: {}", gcode_file, e.what());
  }

  std::lock_guard<std::mutex> lock(lv_lock);
  if (gcode_file != arquivo_atual) {
    return;  // chegou atrasado: já é outra impressão
  }

  current_file = j["/result"_json_pointer];

  auto eta = j["/result/estimated_time"_json_pointer];
  if (eta.is_number()) {
    estimated_time_s = static_cast<uint32_t>(eta.template get<float>());
    spdlog::trace("updated eta {}", estimated_time_s);

    json v = State::get_instance()->get_data("/printer_state/print_stats/print_duration"_json_pointer);
    if (v.is_number()) {
      uint32_t passed = static_cast<uint32_t>(v.template get<float>());
      update_time_progress(passed);
    }
  }

  json info = State::get_instance()->get_data("/printer_state/print_stats/info"_json_pointer);
  update_layers(info);

  std::string fullpath = thumb_detail.first;
  size_t thumb_w = thumb_detail.second.first;
  size_t thumb_h = thumb_detail.second.second;
  if (fullpath.length() > 0 && thumb_w > 0 && thumb_h > 0) {
    spdlog::trace("thumb path: {}", fullpath);
    const std::string img_path = "A:" + fullpath;
    lv_img_set_src(thumbnail, img_path.c_str());

    // cabe em 120 x 120 no meio do cartão de 128
    size_t maior = std::max(thumb_w, thumb_h);
    uint32_t zoom = static_cast<uint32_t>(120 * 256 / maior);
    zoom = std::max<uint32_t>(1, std::min<uint32_t>(zoom, 512));
    lv_img_set_pivot(thumbnail, (lv_coord_t)(thumb_w / 2), (lv_coord_t)(thumb_h / 2));
    lv_img_set_zoom(thumbnail, (uint16_t)zoom);
    lv_obj_center(thumbnail);
    lv_obj_clear_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sem_thumb, LV_OBJ_FLAG_HIDDEN);
  }
}


void PrintStatusPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  // Arquivo novo: busca metadados e miniatura (sem trazer a tela para a
  // frente aqui; quem abre é a passagem para "printing", logo abaixo).
  bool recarregado = false;
  auto printfile = j["/params/0/print_stats/filename"_json_pointer];
  if (printfile.is_string() && !printfile.template get<std::string>().empty()) {
    reset();
    populate();
    recarregado = true;
  }

  auto &pstate = j["/params/0/print_stats/state"_json_pointer];
  if (pstate.is_string()) {
    std::string st = pstate.template get<std::string>();
    bool ativo     = st == "printing" || st == "paused";
    bool era_ativo = last_print_state == "printing" || last_print_state == "paused";
    if (st == "printing" && !era_ativo) {
      // começou (de qualquer origem: tela, Orca, Mainsail; também reimprimir
      // o mesmo arquivo, quando o nome não muda)
      if (!recarregado) {
        reset();
        populate();
      }
      Navegacao::abrir_acompanhamento();   // fecha subtelas, Início, acompanhamento na frente, acende a tela
    } else if (!ativo && era_ativo) {
      // terminou
      if (st == "complete") {
        show_completion(st);               // concluída por cima; NÃO acende a tela
      } else {                             // cancelled, error, standby...
        Navegacao::fechar(status_cont);
        if (Navegacao::pagina_atual() == Pagina::INICIO) {
          Navegacao::ir_para(Pagina::INICIO);
        }
      }
    }
    // pausada <-> imprimindo: só troca o chip e Pausar/Retomar
    if (ativo) {
      mostrar_estado(st);
    }
    last_print_state = st;
  }

  // Track virtual_sdcard progress and filament usage so update_time_progress
  // can average the file-, filament- and slicer-based ETAs like Mainsail (#126).
  auto progress = j["/params/0/virtual_sdcard/progress"_json_pointer];
  if (progress.is_number()) {
    current_progress = progress.template get<double>();
  }

  auto fil_used = j["/params/0/print_stats/filament_used"_json_pointer];
  if (fil_used.is_number()) {
    current_filament_used = fil_used.template get<double>();
  }

  auto v = j["/params/0/extruder/target"_json_pointer];
  if (v.is_number()) {
    extruder_target = v.template get<int>();
  }

  v = j["/params/0/heater_bed/target"_json_pointer];
  if (v.is_number()) {
    heater_bed_target = v.template get<int>();
  }

  v = j["/params/0/extruder/temperature"_json_pointer];
  if (v.is_number()) {
    Marca::texto(bico.valor, fmt::format("{}°C", v.template get<int>()).c_str());
  }

  v = j["/params/0/heater_bed/temperature"_json_pointer];
  if (v.is_number()) {
    Marca::texto(mesa.valor, fmt::format("{}°C", v.template get<int>()).c_str());
  }

  v = j["/params/0/gcode_move/speed_factor"_json_pointer];
  if (v.is_number()) {
    Marca::texto(velocidade.valor, fmt::format("{}%",
      static_cast<int>(std::lround(v.template get<double>() * 100))).c_str());
  }

  bool mudou_ventoinha = false;
  for (auto &f : fan_speeds) {
    const std::string &fan_name = f.first;

    auto fan_value = j[json::json_pointer(fmt::format("/params/0/{}/value", fan_name))];
    if (fan_value.is_number()) {
      f.second = static_cast<int>(fan_value.template get<double>() * 100);
      mudou_ventoinha = true;
    }

    fan_value = j[json::json_pointer(fmt::format("/params/0/{}/speed", fan_name))];
    if (fan_value.is_number()) {
      f.second = static_cast<int>(fan_value.template get<double>() * 100);
      mudou_ventoinha = true;
    }
  }
  if (mudou_ventoinha) {
    atualizar_ventoinha();
  }

  // tempo
  v = j["/params/0/print_stats/print_duration"_json_pointer];
  if (v.is_number()) {
    uint32_t passed = static_cast<uint32_t>(v.template get<float>());
    update_time_progress(passed);
  }

  // porcentagem
  if (progress.is_number()) {
    atualizar_progresso(static_cast<int>(progress.template get<double>() * 100));
  }

  v = j["/params/0/pause_resume/is_paused"_json_pointer];
  if (v.is_boolean()) {
    bool is_paused = v.template get<bool>();
    if (is_paused) {
      lv_obj_clear_state(resume_btn, LV_STATE_DISABLED);
      lv_obj_clear_flag(resume_btn, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_state(pause_btn, LV_STATE_DISABLED);
      lv_obj_add_flag(pause_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_state(pause_btn, LV_STATE_DISABLED);
      lv_obj_clear_flag(pause_btn, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_state(resume_btn, LV_STATE_DISABLED);
      lv_obj_add_flag(resume_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }

  // camadas
  v = j["/params/0/print_stats/info"_json_pointer];
  update_layers(v);

  // etapa do preparo (M117 FASE:xxx da trava da mesa)
  v = j["/params/0/display_status/message"_json_pointer];
  if (v.is_string() && v.template get<std::string>().rfind("FASE:", 0) == 0) {
    fase_atual = v.template get<std::string>().substr(5);
  }
  atualizar_preparo();
}

void PrintStatusPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(event);

  if (btn == complete_done_btn) {
    Navegacao::fechar(complete_cont);
    Navegacao::ir_para(Pagina::INICIO);

  } else if (btn == complete_again_btn) {
    // Não imprime direto: abre os Detalhes do arquivo, e lá o dono toca Imprimir.
    Navegacao::fechar(complete_cont);
    if (!Navegacao::executar("abrir_arquivo", complete_arquivo)) {
      Navegacao::ir_para(Pagina::ARQUIVOS);
    }

  } else if (btn == pause_btn) {
    // um toque sem querer não pausa (a pausa deixa marca na peça)
    KUtils::confirmar("Pausar a impressão?\nO bico sai de cima da peça e espera.",
      [this]() {
        ws.send_jsonrpc("printer.print.pause");
        lv_obj_add_state(pause_btn, LV_STATE_DISABLED);
      });

  } else if (btn == resume_btn) {
    ws.send_jsonrpc("printer.print.resume");
    lv_obj_add_state(resume_btn, LV_STATE_DISABLED);

  } else if (btn == cancel_btn) {
    confirmar_cancelar();

  } else if (btn == finetune_btn) {
    finetune_panel.foreground();

  } else if (btn == bico.cont || btn == mesa.cont) {
    Navegacao::ir_para(Aba::TEMPERATURA);

  } else if (btn == velocidade.cont || btn == ventoinha.cont) {
    finetune_panel.foreground();
  }
}

void PrintStatusPanel::confirmar_cancelar() {
  auto s = State::get_instance();
  auto etarget_j = s->get_data("/printer_state/extruder/target"_json_pointer);
  auto etemp_j   = s->get_data("/printer_state/extruder/temperature"_json_pointer);
  auto btarget_j = s->get_data("/printer_state/heater_bed/target"_json_pointer);
  auto btemp_j   = s->get_data("/printer_state/heater_bed/temperature"_json_pointer);
  auto pdur_j    = s->get_data("/printer_state/print_stats/print_duration"_json_pointer);
  double etarget = etarget_j.is_number() ? etarget_j.template get<double>() : 0.0;
  double etemp   = etemp_j.is_number()   ? etemp_j.template get<double>()   : 0.0;
  double btarget = btarget_j.is_number() ? btarget_j.template get<double>() : 0.0;
  double btemp   = btemp_j.is_number()   ? btemp_j.template get<double>()   : 0.0;
  double pdur    = pdur_j.is_number()    ? pdur_j.template get<double>()    : 1.0;
  bool e_heating  = etarget > 0 && etemp < etarget - 2.0;
  bool b_heating  = btarget > 0 && btemp < btarget - 2.0;
  bool in_startup = pdur < 0.1;

  std::string delay;
  if (e_heating && b_heating)
    delay = fmt::format("\n\nBico {:.0f}/{:.0f}°C + mesa {:.0f}/{:.0f}°C\n"
                        "O cancelamento fica na fila até o aquecimento terminar.", etemp, etarget, btemp, btarget);
  else if (e_heating)
    delay = fmt::format("\n\nAquecendo o bico: {:.0f}/{:.0f}°C\n"
                        "O cancelamento fica na fila até o aquecimento terminar.", etemp, etarget);
  else if (b_heating)
    delay = fmt::format("\n\nAquecendo a mesa: {:.0f}/{:.0f}°C\n"
                        "O cancelamento fica na fila até o aquecimento terminar.", btemp, btarget);
  else if (in_startup)
    delay = "\n\nA impressão está começando.\n"
            "O cancelamento fica na fila até essa etapa terminar.";

  static const char *cancel_dlg_btns[] = {"Cancelar impressão", "Voltar", ""};
  std::string cancel_msg = fmt::format("Cancelar a impressão?{}", delay);
  lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, cancel_msg.c_str(), cancel_dlg_btns, false);
  lv_obj_add_event_cb(mbox, [](lv_event_t *ev) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(ev));
    auto *self = static_cast<PrintStatusPanel *>(lv_event_get_user_data(ev));
    if (lv_msgbox_get_active_btn(obj) == 0) {
      spdlog::debug("cancel print confirmed");
      self->ws.send_jsonrpc("printer.print.cancel");
    }
    lv_msgbox_close(obj);
  }, LV_EVENT_VALUE_CHANGED, this);
  KUtils::style_lock_mbox(mbox, 90);
  if (!delay.empty()) {
    // o texto de espera tem 4 ou 5 linhas: caixa maior para não cortar
    lv_obj_set_size(mbox, 400, 216);
    lv_obj_center(mbox);
  }
  lv_obj_align(((lv_msgbox_t *)mbox)->text, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *btnm = lv_msgbox_get_btns(mbox);
  lv_obj_add_event_cb(btnm, [](lv_event_t *e) {
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part == LV_PART_ITEMS && dsc->id == 0) {
      dsc->rect_dsc->bg_color = lv_color_hex(Marca::ERRO);
      dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
      dsc->label_dsc->color   = lv_color_hex(Marca::PAPEL);
    }
  }, LV_EVENT_DRAW_PART_BEGIN, NULL);
}

void PrintStatusPanel::update_time_progress(uint32_t time_passed) {
    // Average the available remaining-time estimates the way Mainsail does: each
    // method (file, filament, slicer) contributes only when it yields a positive
    // value, and the displayed ETA is their unweighted mean (#126). This tracks
    // Mainsail's ETA, which stays accurate when a print over/under-runs the
    // slicer's static guess.
    double total = 0.0;
    int count = 0;

    // file: extrapolate elapsed time over the virtual_sdcard progress fraction.
    if (current_progress > 0.0 && time_passed > 0) {
      double file_remaining = time_passed / current_progress - time_passed;
      if (file_remaining > 0.0) { total += file_remaining; ++count; }
    }

    // filament: extrapolate elapsed time over the consumed/total filament ratio.
    double filament_total = 0.0;
    if (current_file.is_object()) {
      auto ft = current_file["/filament_total"_json_pointer];
      if (ft.is_number()) filament_total = ft.template get<double>();
    }
    if (current_filament_used > 0.0 && filament_total > current_filament_used
        && time_passed > 0) {
      double filament_remaining =
        time_passed / (current_filament_used / filament_total) - time_passed;
      if (filament_remaining > 0.0) { total += filament_remaining; ++count; }
    }

    // slicer: the static estimate from the gcode metadata.
    if (estimated_time_s > 0) {
      double slicer_remaining = static_cast<double>(estimated_time_s) - time_passed;
      if (slicer_remaining > 0.0) { total += slicer_remaining; ++count; }
    }

    if (count > 0) {
      int64_t eta_secs = static_cast<int64_t>(total / count);
      Marca::texto(falta_label, KUtils::tempo_curto(eta_secs).c_str());
      Marca::texto(total_label,
        ("Total: " + KUtils::tempo_curto((int64_t)time_passed + eta_secs)).c_str());
    } else {
      Marca::texto(falta_label, "--");
      Marca::texto(total_label, "");
    }

    Marca::texto(decorrido_label, ("Decorrido #f5f4f0 " + KUtils::tempo_curto(time_passed) + "#").c_str());
}

void PrintStatusPanel::update_layers(json &info) {
  int atual = current_layer(info);
  int total = max_layer(info);
  if (atual == camada_atual && total == camadas_total) {
    return;
  }
  camada_atual = atual;
  camadas_total = total;
  // camada 0 = ainda preparando (aquecendo/medindo): não mostra "Camada 0/120"
  if (total > 0 && atual > 0) {
    Marca::texto(camada_label, fmt::format("Camada #f5f4f0 {}/{}#", atual, total).c_str());
  } else {
    Marca::texto(camada_label, "");
  }
  atualizar_resumo();
}

int PrintStatusPanel::max_layer(json &info) {
  if (info.is_object()) {
    auto v = info["/total_layer"_json_pointer];
    if (v.is_number()) {
      return v.template get<int>();
    }
  }

  if (current_file.is_object()) {
    auto v = current_file["/layer_count"_json_pointer];
    if (v.is_number()) {
      return v.template get<int>();
    } else {
      auto first_layer_height = current_file["/first_layer_height"_json_pointer];
      auto layer_height = current_file["/layer_height"_json_pointer];
      auto object_height = current_file["/object_height"_json_pointer];

      if (first_layer_height.is_number() && layer_height.is_number() && object_height.is_number()
          && layer_height.template get<double>() > 0.0) {
        auto layer = static_cast<int>(std::ceil((object_height.template get<double>() - first_layer_height.template get<double>()) / layer_height.template get<double>() + 1));
        return layer > 0 ? layer : 0;
      }
    }
  }
  return 0;
}

int PrintStatusPanel::current_layer(json &info) {
  if (info.is_object()) {
    auto v = info["/current_layer"_json_pointer];
    if (v.is_number()) {
      return v.template get<int>();
    }
  }

  if (current_file.is_object()) {
    State *s = State::get_instance();
    auto pd = s->get_data("/printer_state/print_stats/print_duration"_json_pointer);
    auto zpos = s->get_data("/printer_state/gcode_move/gcode_position/2"_json_pointer);

    auto first_layer_height = current_file["/first_layer_height"_json_pointer];
    auto layer_height = current_file["/layer_height"_json_pointer];

    if (pd.is_number()
        && pd.template get<int>() > 0
        && zpos.is_number()
        && first_layer_height.is_number()
        && layer_height.is_number()
        && layer_height.template get<double>() > 0.0) {
      auto layer = static_cast<int>(std::ceil((zpos.template get<double>() - first_layer_height.template get<double>()) / layer_height.template get<double>() + 1));
      auto total = max_layer(info);
      if (layer > total) {
        return total;
      }

      if (layer > 0) {
        return layer;
      }
    }
  }

  return 0;
}

FineTunePanel &PrintStatusPanel::get_finetune_panel() {
  return finetune_panel;
}

ZOffsetPanel &PrintStatusPanel::get_zoffset_panel() {
  return zoffset_panel;
}

#ifdef SIMULATOR
void PrintStatusPanel::sim_setup_mock_data() {
  // Mid-print snapshot: 20% in, 10 minutes elapsed.
  json mock = {{"params", {{
    {"print_stats", {
      {"filename", "BunnyHighResLongFilenameTest_v3.gcode"},
      {"state", "printing"},
      {"print_duration", 600.0},
      {"filament_used", 1234.5},
      {"info", {{"current_layer", 12}, {"total_layer", 60}}}
    }},
    {"virtual_sdcard", {{"progress", 0.20}}},
    {"extruder", {{"temperature", 210}, {"target", 220}}},
    {"heater_bed", {{"temperature", 62}, {"target", 65}}},
    {"gcode_move", {{"speed_factor", 1.0}, {"extrude_factor", 1.0}}},
    // Labelled objects so the conditional "Objetos" button shows in the sim.
    {"exclude_object", {
      {"objects", {
        {{"name", "benchy_1"}, {"center", {60, 60}},
         {"polygon", {{40, 40}, {80, 40}, {80, 80}, {40, 80}}}},
        {{"name", "benchy_2"}, {"center", {165, 85}},
         {"polygon", {{145, 60}, {185, 60}, {185, 110}, {145, 110}}}},
        {{"name", "LongBracketMountFilename_v3_id2"}, {"center", {110, 165}},
         {"polygon", {{90, 145}, {130, 145}, {130, 185}, {90, 185}}}}
      }},
      {"excluded_objects", {"benchy_2"}},
      {"current_object", "benchy_1"}
    }}
  }}}};
  // GS_PREPARO=altura|mede|confere|assentar|ok|mesa|bico|zerando: impressão ainda
  // no preparo (antes do primeiro filamento), para ver a faixa de etapas
  const char *preparo = std::getenv("GS_PREPARO");
  if (preparo != nullptr) {
    std::string f = preparo;
    mock["params"][0]["print_stats"]["filament_used"] = 0.0;
    mock["params"][0]["print_stats"]["info"]["current_layer"] = 0;
    mock["params"][0]["print_stats"]["print_duration"] = 0.0;
    mock["params"][0]["virtual_sdcard"]["progress"] = 0.0;
    mock["params"][0]["heater_bed"] = {{"temperature", f == "mesa" ? 48 : 70}, {"target", 70}};
    mock["params"][0]["extruder"] = {{"temperature", f == "bico" ? 186 : 150}, {"target", f == "bico" ? 240 : 150}};
    std::string msg = (f == "mesa" || f == "zerando" || f == "bico") ? "" : "FASE:" + f;
    mock["params"][0]["display_status"] = {{"message", msg}};
  }
  State::get_instance()->consume(mock);
  // consume() faz reset() + populate() (nome novo) e abre o acompanhamento
  // (passagem para "printing").
  consume(mock);
  if (preparo != nullptr) {
    return;
  }

  // reset() (inside consume) cleared the slicer estimate and metadata, so only
  // the file method had data above. Inject a slicer estimate + filament total
  // the way handle_metadata would, then recompute so the displayed ETA shows
  // the full Mainsail-style average of all three methods (#126).
  estimated_time_s = 3000;
  current_file = {{"filament_total", 5000.0}};
  {
    std::lock_guard<std::mutex> lock(lv_lock);
    update_time_progress(600);
  }

  // preview the completion summary (no real print finishes in the sim)
  lv_timer_t *t = lv_timer_create([](lv_timer_t *tm) {
    ((PrintStatusPanel *)tm->user_data)->show_completion("complete");
  }, 11000, this);
  lv_timer_set_repeat_count(t, 1);
}
#endif
