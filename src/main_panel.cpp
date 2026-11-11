#include "main_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "utils.h"
#include "sysinfo_panel.h"
#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

LV_IMG_DECLARE(light_img);
LV_IMG_DECLARE(extruder);
LV_IMG_DECLARE(bed);
LV_IMG_DECLARE(heater);
LV_IMG_DECLARE(cooldown_img);

namespace {
  // Leitura segura do json (não cria chaves nulas no json da notificação,
  // que ainda passa pelos outros consumidores).
  const json *status_da_notificacao(const json &j) {
    auto it = j.find("params");
    if (it == j.end() || !it->is_array() || it->empty()) {
      return nullptr;
    }
    const json &p0 = (*it)[0];
    return p0.is_object() ? &p0 : nullptr;
  }

  const json *status_da_assinatura(const json &j) {
    auto r = j.find("result");
    if (r == j.end() || !r->is_object()) {
      return nullptr;
    }
    auto s = r->find("status");
    if (s == r->end() || !s->is_object()) {
      return nullptr;
    }
    return &*s;
  }

  const json *objeto(const json *status, const std::string &nome) {
    if (status == nullptr) {
      return nullptr;
    }
    auto o = status->find(nome);
    if (o == status->end() || !o->is_object()) {
      return nullptr;
    }
    return &*o;
  }

  const json *campo(const json *status, const std::string &nome, const char *chave) {
    const json *o = objeto(status, nome);
    if (o == nullptr) {
      return nullptr;
    }
    auto v = o->find(chave);
    if (v == o->end() || v->is_null()) {
      return nullptr;
    }
    return &*v;
  }

  // -1 = não dá para saber, 0 = desligada, 1 = ligada
  int luz_de(const json &o) {
    if (!o.is_object()) {
      return -1;
    }
    auto v = o.find("value");
    if (v != o.end() && v->is_number()) {
      return v->template get<double>() > 0 ? 1 : 0;
    }
    auto cd = o.find("color_data");
    if (cd != o.end() && cd->is_array() && !cd->empty() && (*cd)[0].is_array()) {
      for (auto &canal : (*cd)[0]) {
        if (canal.is_number() && canal.template get<double>() > 0) {
          return 1;
        }
      }
      return 0;
    }
    return -1;
  }

  // "pasta/teste_PETG.gcode" -> "teste_PETG"
  std::string nome_curto(const std::string &arquivo) {
    std::string n = arquivo;
    size_t barra = n.find_last_of('/');
    if (barra != std::string::npos) {
      n = n.substr(barra + 1);
    }
    const std::string ext = ".gcode";
    if (n.size() > ext.size() && n.compare(n.size() - ext.size(), ext.size(), ext) == 0) {
      n = n.substr(0, n.size() - ext.size());
    }
    return n;
  }

  lv_obj_t *nova_subtela() {
    lv_obj_t *c = lv_obj_create(lv_scr_act());
    Navegacao::subtela(c);
    return c;
  }
}

MainPanel::MainPanel(KWebSocketClient &websocket,
  std::mutex &lock,
  SpoolmanPanel &sm)
  : NotifyConsumer(lock)
  , ws(websocket)
  , homing_panel(ws, lock)
  , fan_panel(ws, lock)
  , led_panel(ws, lock)
  , macros_cont(nova_subtela())
  , macros_panel(ws, lock, macros_cont, [this]() { show_console(); })
  , console_cont(nova_subtela())
  , console_panel(ws, lock, console_cont)
  , setting_panel(websocket, lock, Navegacao::aba(Aba::SISTEMA), sm)
  , print_status_panel(websocket, lock, Navegacao::pagina(Pagina::INICIO))
  , print_panel(ws, lock, print_status_panel, sm)
  , printertune_panel(ws, lock, Navegacao::aba(Aba::AVANCADO),
                      print_status_panel.get_finetune_panel(), print_status_panel.get_zoffset_panel())
  , numpad(lv_layer_top())
  , extruder_panel(ws, lock, numpad, sm)
  , prompt_panel(websocket, lock, Navegacao::pagina(Pagina::INICIO))
  , spoolman_panel(sm)
  , inicio_chip(NULL)
  , bico_alvo(NULL)
  , bico_valor(NULL)
  , mesa_alvo(NULL)
  , mesa_valor(NULL)
  , cartao_luz(NULL)
  , luz_icone(NULL)
  , luz_valor(NULL)
  , ultima_thumb(NULL)
  , ultima_sem_thumb(NULL)
  , ultima_nome(NULL)
  , ultima_selo(NULL)
  , ultima_status(NULL)
  , ultima_btn(NULL)
  , ultima_estado(-1)
  , luz_ligada(-1)
{
  ws.register_notify_update(this);
}

MainPanel::~MainPanel() {
  sensors.clear();
}

void MainPanel::subscribe() {
  spdlog::trace("main panel subscribing");
  ws.send_jsonrpc("printer.gcode.help", [this](json &d) { console_panel.handle_macros(d); });
  print_panel.subscribe();
}

PrinterTunePanel &MainPanel::get_tune_panel() {
  return printertune_panel;
}

void MainPanel::init(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  atualizar_de(status_da_assinatura(j), false);

  macros_panel.populate();

  auto fans = State::get_instance()->get_display_fans();
  print_status_panel.init(fans);
  printertune_panel.init(j);

  pedir_ultima_impressao();
}

void MainPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  atualizar_de(status_da_notificacao(j), true);
}

void MainPanel::atualizar_de(const json *st, bool transicoes) {
  if (st == nullptr) {
    return;
  }

  // cartões de temperatura (aba Temperatura)
  for (const auto &el : sensors) {
    const json *alvo = campo(st, el.first, "target");
    if (alvo != nullptr && alvo->is_number()) {
      el.second->update_target(alvo->template get<int>());
    }

    const json *temp = campo(st, el.first, "temperature");
    if (temp != nullptr && temp->is_number()) {
      int value = temp->template get<int>();
      el.second->update_series(value);
      el.second->update_value(value);
    }
  }

  if (inicio_chip == NULL) {
    return;  // Início ainda não montado
  }

  // cartões Bico e Mesa do Início
  struct { const char *id; lv_obj_t *valor; lv_obj_t *alvo; } aquecedores[] = {
    {"extruder", bico_valor, bico_alvo},
    {"heater_bed", mesa_valor, mesa_alvo},
  };
  for (auto &a : aquecedores) {
    const json *temp = campo(st, a.id, "temperature");
    if (temp != nullptr && temp->is_number()) {
      Marca::texto(a.valor, fmt::format("{}°C", temp->template get<int>()).c_str());
    }
    const json *alvo = campo(st, a.id, "target");
    if (alvo != nullptr && alvo->is_number()) {
      int t = alvo->template get<int>();
      Marca::texto(a.alvo, t > 0 ? fmt::format("/{}", t).c_str() : "");
    }
  }

  // estado da impressão
  const json *ps = campo(st, "print_stats", "state");
  if (ps != nullptr && ps->is_string()) {
    std::string novo = ps->template get<std::string>();
    bool era_ativo = estado_impressao == "printing" || estado_impressao == "paused";
    bool ativo = novo == "printing" || novo == "paused";
    estado_impressao = novo;
    atualizar_chip();
    if (transicoes && era_ativo && !ativo) {
      // terminou (concluída, cancelada, erro): a última impressão mudou
      agendar_ultima_impressao();
    }
  }

  // luz
  if (!luz_id.empty()) {
    const json *o = objeto(st, luz_id);
    if (o != nullptr) {
      atualizar_luz(*o);
    }
  }
}

void MainPanel::atualizar_chip() {
  if (inicio_chip == NULL) {
    return;
  }
  const std::string &s = estado_impressao;
  // maquete: 92 px no "Pausada", 112 nos outros (só muda se for diferente)
  lv_coord_t largura = s == "paused" ? 92 : 112;
  if (lv_obj_get_style_width(inicio_chip, LV_PART_MAIN) != largura) {
    lv_obj_set_width(inicio_chip, largura);
    lv_obj_set_x(inicio_chip, 416 - largura);   // a borda direita fica no lugar (x = 416)
  }
  if (s == "printing") {
    Marca::chip_estado(inicio_chip, Marca::Estado::IMPRIMINDO, "Imprimindo");
  } else if (s == "paused") {
    Marca::chip_estado(inicio_chip, Marca::Estado::PAUSADA, "Pausada");
  } else if (s == "error") {
    Marca::chip_estado(inicio_chip, Marca::Estado::ERRO, "Erro");
  } else {
    // standby, complete, cancelled e vazio
    Marca::chip_estado(inicio_chip, Marca::Estado::PRONTA, "Pronta");
  }
}

void MainPanel::atualizar_luz(const json &o) {
  int l = luz_de(o);
  if (l < 0 || l == luz_ligada || luz_valor == NULL) {
    return;
  }
  luz_ligada = l;
  Marca::texto(luz_valor, l ? "Ligada" : "Desligada");
  Marca::cor_icone(luz_icone, l ? Marca::LIMAO : Marca::GRAFITE);
}

void MainPanel::create_panel() {
  criar_inicio();
  criar_temperatura();

  // Macros e Console: subtelas abertas pela aba Avançado
  Navegacao::registrar_acao("macros", [this](const std::string &) {
    Navegacao::abrir(macros_cont);
    macros_panel.reset_to_favorites();
  });
  Navegacao::registrar_acao("console", [this](const std::string &) {
    Navegacao::abrir(console_cont);
    console_panel.reset_to_terminal();
  });

  // tudo pronto: começa no Início (fecha qualquer subtela que ficou na frente)
  Navegacao::ir_para(Pagina::INICIO);
}

void MainPanel::criar_inicio() {
  lv_obj_t *pg = Navegacao::pagina(Pagina::INICIO);

  lv_obj_t *titulo = Marca::criar_rotulo(pg, "Início", Marca::f20(), Marca::PAPEL);
  lv_obj_set_pos(titulo, 8, 10);
  lv_obj_set_width(titulo, 260);
  lv_label_set_long_mode(titulo, LV_LABEL_LONG_DOT);

  // indicador de rede: PAPEL com endereço no Wi-Fi, GRAFITE sem. Conferido
  // ao mostrar o Início (só lê as interfaces, não fala com o wpa_supplicant).
  lv_obj_t *wifi = Marca::criar_rotulo(pg, LV_SYMBOL_WIFI, Marca::f16(), Marca::GRAFITE);
  lv_obj_set_pos(wifi, 276, 12);
  auto cor_wifi = [wifi]() {
    uint32_t cor = ListaCfg::ip_atual(true).empty() ? Marca::GRAFITE : Marca::PAPEL;
    lv_color_t nova = lv_color_hex(cor);
    if (lv_obj_get_style_text_color(wifi, LV_PART_MAIN).full != nova.full) {
      lv_obj_set_style_text_color(wifi, nova, 0);
    }
  };
  cor_wifi();
  Navegacao::ao_mostrar(Pagina::INICIO, cor_wifi);

  inicio_chip = Marca::criar_chip(pg);
  lv_obj_set_pos(inicio_chip, 304, 10);
  lv_obj_set_width(inicio_chip, 112);
  atualizar_chip();

  // ---- 3 cartões: Bico, Mesa, Luz ----
  struct Cartao { const void *img; const char *nome; lv_coord_t x; };
  const Cartao cartoes[] = {
    {&extruder, "Bico", 8},
    {&bed, "Mesa", 147},
    {&light_img, "Luz", 286},
  };
  lv_obj_t *alvos[3];
  lv_obj_t *valores[3];
  lv_obj_t *icones[3];
  lv_obj_t *objs[3];
  for (int i = 0; i < 3; i++) {
    lv_obj_t *c = Marca::criar_cartao(pg, cartoes[i].x, 52, 130, 72);
    Marca::cartao_tocavel(c);
    objs[i] = c;

    icones[i] = Marca::criar_icone(c, cartoes[i].img, 24, Marca::GRAFITE);
    lv_obj_set_pos(icones[i], 10, 10);

    lv_obj_t *nome = Marca::criar_rotulo(c, cartoes[i].nome, Marca::f12(), Marca::GRAFITE);
    lv_obj_set_pos(nome, 40, 14);

    alvos[i] = Marca::criar_rotulo(c, "", Marca::f12(), Marca::GRAFITE);
    lv_obj_set_pos(alvos[i], 70, 14);
    lv_obj_set_width(alvos[i], 50);
    lv_obj_set_style_text_align(alvos[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(alvos[i], LV_LABEL_LONG_CLIP);

    valores[i] = Marca::criar_rotulo(c, "--", Marca::f20(), Marca::PAPEL);
    lv_obj_set_pos(valores[i], 10, 40);
    lv_obj_set_width(valores[i], 110);
    lv_label_set_long_mode(valores[i], LV_LABEL_LONG_CLIP);
  }
  bico_valor = valores[0];
  bico_alvo = alvos[0];
  mesa_valor = valores[1];
  mesa_alvo = alvos[1];
  cartao_luz = objs[2];
  luz_icone = icones[2];
  luz_valor = valores[2];
  lv_obj_add_flag(alvos[2], LV_OBJ_FLAG_HIDDEN);
  // sem luz configurada o cartão não reage (create_leds liga quando houver)
  lv_obj_clear_flag(cartao_luz, LV_OBJ_FLAG_CLICKABLE);

  auto ir_temperatura = [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      Navegacao::ir_para(Aba::TEMPERATURA);
    }
  };
  lv_obj_add_event_cb(objs[0], ir_temperatura, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(objs[1], ir_temperatura, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(cartao_luz, [](lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
      return;
    }
    MainPanel *mp = (MainPanel *)lv_event_get_user_data(e);
    if (mp->luz_id.empty()) {
      return;
    }
    if (!Navegacao::executar("luz")) {
      mp->led_panel.foreground();  // até o lote Controle registrar a ação
    }
  }, LV_EVENT_CLICKED, this);

  // ---- Última impressão ----
  lv_obj_t *ult = Marca::criar_cartao(pg, 8, 132, 272, 132);

  lv_obj_t *mini = lv_obj_create(ult);
  lv_obj_set_pos(mini, 12, 12);
  lv_obj_set_size(mini, 64, 64);
  lv_obj_set_style_bg_color(mini, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_set_style_bg_opa(mini, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(mini, Marca::RAIO_MINI, 0);
  lv_obj_set_style_border_width(mini, 0, 0);
  lv_obj_set_style_pad_all(mini, 0, 0);
  lv_obj_clear_flag(mini, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(mini, LV_OBJ_FLAG_CLICKABLE);

  ultima_sem_thumb = Marca::criar_rotulo(mini, LV_SYMBOL_FILE, Marca::f20(), Marca::GRAFITE);
  lv_obj_center(ultima_sem_thumb);

  ultima_thumb = lv_img_create(mini);
  lv_img_set_size_mode(ultima_thumb, LV_IMG_SIZE_MODE_REAL);
  lv_obj_add_flag(ultima_thumb, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *ult_titulo = Marca::criar_rotulo(ult, "Última impressão", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(ult_titulo, 88, 12);
  lv_obj_set_width(ult_titulo, 172);
  lv_label_set_long_mode(ult_titulo, LV_LABEL_LONG_DOT);

  ultima_nome = Marca::criar_rotulo(ult, "", Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(ultima_nome, 88, 29);
  lv_obj_set_size(ultima_nome, 172, 34);
  lv_label_set_long_mode(ultima_nome, LV_LABEL_LONG_DOT);

  ultima_selo = Marca::criar_selo(ult, 14, Marca::OK, Marca::PAPEL, LV_SYMBOL_OK);
  lv_obj_set_pos(ultima_selo, 88, 65);
  lv_obj_add_flag(ultima_selo, LV_OBJ_FLAG_HIDDEN);

  ultima_status = Marca::criar_rotulo(ult, "", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(ultima_status, 106, 64);
  lv_obj_set_width(ultima_status, 154);
  lv_label_set_long_mode(ultima_status, LV_LABEL_LONG_DOT);

  ultima_btn = Marca::criar_botao(ult, "Ver arquivos", Marca::Tipo::NORMAL, Marca::f14());
  lv_obj_set_pos(ultima_btn, 12, 82);
  lv_obj_set_size(ultima_btn, 248, 44);
  lv_obj_add_event_cb(ultima_btn, [](lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
      return;
    }
    MainPanel *mp = (MainPanel *)lv_event_get_user_data(e);
    // Não imprime direto: abre os Detalhes do arquivo, e lá o dono toca
    // Imprimir (pergunta do Spoolman e trava de "já está imprimindo").
    if (mp->ultima_estado == 1 && !mp->ultima_arquivo.empty()
        && Navegacao::executar("abrir_arquivo", mp->ultima_arquivo)) {
      return;
    }
    Navegacao::ir_para(Pagina::ARQUIVOS);
  }, LV_EVENT_CLICKED, this);

  mostrar_ultima_impressao(nullptr);

  // ---- Atalhos: Preaquecer e Mover ----
  lv_obj_t *pre = Marca::criar_cartao(pg, 288, 132, 128, 62);
  Marca::cartao_tocavel(pre);
  lv_obj_t *pre_t = Marca::criar_rotulo(pre, "Preaquecer", Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(pre_t, 12, 12);
  lv_obj_t *pre_s = Marca::criar_rotulo(pre,
    fmt::format("PETG · {}/{}°C", Marca::PETG_BICO, Marca::PETG_MESA).c_str(), Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(pre_s, 12, 34);
  lv_obj_set_width(pre_s, 104);
  lv_label_set_long_mode(pre_s, LV_LABEL_LONG_DOT);
  // não aquece direto: leva à aba Temperatura
  lv_obj_add_event_cb(pre, ir_temperatura, LV_EVENT_CLICKED, NULL);

  lv_obj_t *mov = Marca::criar_cartao(pg, 288, 202, 128, 62);
  Marca::cartao_tocavel(mov);
  lv_obj_t *mov_t = Marca::criar_rotulo(mov, "Mover", Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(mov_t, 12, 12);
  lv_obj_t *mov_s = Marca::criar_rotulo(mov, "Eixos e origem", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(mov_s, 12, 34);
  lv_obj_set_width(mov_s, 104);
  lv_label_set_long_mode(mov_s, LV_LABEL_LONG_DOT);
  lv_obj_add_event_cb(mov, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      Navegacao::ir_para(Aba::MOVER);
    }
  }, LV_EVENT_CLICKED, NULL);
}

void MainPanel::criar_temperatura() {
  lv_obj_t *aba = Navegacao::aba(Aba::TEMPERATURA);

  // PETG (destaque: é o filamento do dono)
  lv_obj_t *petg = lv_btn_create(aba);
  lv_obj_set_style_pad_all(petg, 0, 0);
  lv_obj_set_pos(petg, 8, 175);
  lv_obj_set_size(petg, 130, 44);
  lv_obj_set_flex_flow(petg, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(petg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(petg, 1, 0);
  lv_obj_t *petg_t = lv_label_create(petg);
  lv_label_set_text(petg_t, "PETG");
  lv_obj_set_style_text_font(petg_t, Marca::f14(), 0);
  lv_obj_t *petg_s = lv_label_create(petg);
  lv_label_set_text(petg_s, fmt::format("{}°C · {}°C", Marca::PETG_BICO, Marca::PETG_MESA).c_str());
  lv_obj_set_style_text_font(petg_s, Marca::f12(), 0);
  Marca::estilo_botao(petg, Marca::Tipo::DESTAQUE);   // os dois textos herdam o PRETO
  lv_obj_add_event_cb(petg, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      ((MainPanel *)lv_event_get_user_data(e))->preaquecer(Marca::PETG_BICO, Marca::PETG_MESA);
    }
  }, LV_EVENT_CLICKED, this);

  // PLA
  lv_obj_t *pla = lv_btn_create(aba);
  lv_obj_set_style_pad_all(pla, 0, 0);
  lv_obj_set_pos(pla, 147, 175);
  lv_obj_set_size(pla, 131, 44);
  lv_obj_set_flex_flow(pla, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(pla, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(pla, 1, 0);
  lv_obj_t *pla_t = lv_label_create(pla);
  lv_label_set_text(pla_t, "PLA");
  lv_obj_set_style_text_font(pla_t, Marca::f14(), 0);
  lv_obj_t *pla_s = Marca::criar_rotulo(pla,
    fmt::format("{}°C · {}°C", Marca::PLA_BICO, Marca::PLA_MESA).c_str(), Marca::f12(), Marca::GRAFITE);
  LV_UNUSED(pla_s);
  Marca::estilo_botao(pla, Marca::Tipo::NORMAL);
  lv_obj_add_event_cb(pla, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      ((MainPanel *)lv_event_get_user_data(e))->preaquecer(Marca::PLA_BICO, Marca::PLA_MESA);
    }
  }, LV_EVENT_CLICKED, this);

  // Esfriar
  lv_obj_t *esfriar = lv_btn_create(aba);
  lv_obj_set_style_pad_all(esfriar, 0, 0);
  lv_obj_set_pos(esfriar, 287, 175);
  lv_obj_set_size(esfriar, 129, 44);
  lv_obj_set_flex_flow(esfriar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(esfriar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(esfriar, 6, 0);
  Marca::criar_icone(esfriar, &cooldown_img, 20, Marca::PAPEL);
  lv_obj_t *esfriar_t = lv_label_create(esfriar);
  lv_label_set_text(esfriar_t, "Esfriar");
  lv_obj_set_style_text_font(esfriar_t, Marca::f14(), 0);
  Marca::estilo_botao(esfriar, Marca::Tipo::NORMAL);
  lv_obj_add_event_cb(esfriar, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      ((MainPanel *)lv_event_get_user_data(e))->preaquecer(0, 0);
    }
  }, LV_EVENT_CLICKED, this);
}

void MainPanel::preaquecer(int bico, int mesa) {
  // roda dentro de evento do LVGL (lv_lock já preso): não prende de novo
  const char *aviso = (bico == 0 && mesa == 0)
    ? "A impressora está imprimindo.\nEsfriar agora desliga o bico e a mesa\ne a impressão é perdida. Esfriar?"
    : "A impressora está imprimindo.\nMudar as temperaturas mesmo assim?";
  KUtils::confirm_if_printing(aviso,
    [this, bico, mesa]() {
      ws.gcode_script(fmt::format("SET_HEATER_TEMPERATURE HEATER=extruder TARGET={}", bico));
      ws.gcode_script(fmt::format("SET_HEATER_TEMPERATURE HEATER=heater_bed TARGET={}", mesa));
    });
}

void MainPanel::agendar_ultima_impressao() {
  // O histórico do Moonraker fecha o trabalho logo depois do fim da
  // impressão: pergunta 2 s depois (timer de uma vez só, apaga sozinho).
  lv_timer_t *t = lv_timer_create([](lv_timer_t *tm) {
    ((MainPanel *)tm->user_data)->pedir_ultima_impressao();
  }, 2000, this);
  lv_timer_set_repeat_count(t, 1);
}

void MainPanel::pedir_ultima_impressao() {
  json params = {{"limit", 1}, {"order", "desc"}};
  ws.send_jsonrpc("server.history.list", params, [this](json &d) {
    // resposta chega na thread do websocket
    json job;
    bool tem = false;
    auto r = d.find("result");
    if (r != d.end() && r->is_object()) {
      auto jobs = r->find("jobs");
      if (jobs != r->end() && jobs->is_array() && !jobs->empty() && (*jobs)[0].is_object()) {
        job = (*jobs)[0];
        tem = true;
      }
    }

    std::string arquivo;
    {
      std::lock_guard<std::mutex> lock(lv_lock);
      mostrar_ultima_impressao(tem ? &job : nullptr);
      arquivo = ultima_arquivo;
    }

    if (tem && !arquivo.empty()) {
      carregar_miniatura(arquivo);
    }
  });
}

void MainPanel::mostrar_ultima_impressao(const json *job) {
  if (ultima_nome == NULL) {
    return;
  }

  std::string arquivo;
  if (job != nullptr) {
    auto f = job->find("filename");
    if (f != job->end() && f->is_string()) {
      arquivo = f->template get<std::string>();
    }
  }

  if (arquivo.empty()) {
    // sem histórico
    ultima_arquivo.clear();
    lv_obj_add_flag(ultima_thumb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ultima_sem_thumb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ultima_selo, LV_OBJ_FLAG_HIDDEN);
    Marca::texto(ultima_status, "");
    Marca::texto(ultima_nome, "Nenhuma impressão ainda");
    if (ultima_estado != 0) {
      ultima_estado = 0;
      Marca::estilo_botao(ultima_btn, Marca::Tipo::NORMAL);
      Marca::texto(lv_obj_get_child(ultima_btn, 0), "Ver arquivos");
    }
    return;
  }

  if (arquivo != ultima_arquivo) {
    // outro arquivo: a miniatura antiga sai até a nova chegar
    lv_obj_add_flag(ultima_thumb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ultima_sem_thumb, LV_OBJ_FLAG_HIDDEN);
  }
  ultima_arquivo = arquivo;
  Marca::texto(ultima_nome, nome_curto(arquivo).c_str());

  std::string status;
  auto st = job->find("status");
  if (st != job->end() && st->is_string()) {
    status = st->template get<std::string>();
  }

  double dur = 0;
  auto pd = job->find("print_duration");
  if (pd != job->end() && pd->is_number()) {
    dur = pd->template get<double>();
  }
  if (dur <= 0) {
    auto td = job->find("total_duration");
    if (td != job->end() && td->is_number()) {
      dur = td->template get<double>();
    }
  }
  std::string tempo = KUtils::tempo_curto((int64_t)dur);

  lv_obj_t *selo_txt = lv_obj_get_child(ultima_selo, 0);
  if (status == "completed") {
    lv_obj_set_style_bg_color(ultima_selo, lv_color_hex(Marca::OK), 0);
    lv_obj_set_style_text_color(ultima_selo, lv_color_hex(Marca::PAPEL), 0);
    Marca::texto(selo_txt, LV_SYMBOL_OK);
    Marca::texto(ultima_status, fmt::format("Concluída · {}", tempo).c_str());
  } else if (status == "in_progress") {
    lv_obj_set_style_bg_color(ultima_selo, lv_color_hex(Marca::LIMAO), 0);
    lv_obj_set_style_text_color(ultima_selo, lv_color_hex(Marca::PRETO), 0);
    Marca::texto(selo_txt, LV_SYMBOL_PLAY);
    Marca::texto(ultima_status, "Imprimindo");
  } else if (status == "cancelled") {
    lv_obj_set_style_bg_color(ultima_selo, lv_color_hex(Marca::ERRO), 0);
    lv_obj_set_style_text_color(ultima_selo, lv_color_hex(Marca::PAPEL), 0);
    Marca::texto(selo_txt, LV_SYMBOL_CLOSE);
    Marca::texto(ultima_status, fmt::format("Cancelada · {}", tempo).c_str());
  } else {
    // error, klippy_shutdown, klippy_disconnect, server_exit...
    lv_obj_set_style_bg_color(ultima_selo, lv_color_hex(Marca::ERRO), 0);
    lv_obj_set_style_text_color(ultima_selo, lv_color_hex(Marca::PAPEL), 0);
    Marca::texto(selo_txt, LV_SYMBOL_CLOSE);
    Marca::texto(ultima_status, fmt::format("Erro · {}", tempo).c_str());
  }
  lv_obj_clear_flag(ultima_selo, LV_OBJ_FLAG_HIDDEN);

  if (ultima_estado != 1) {
    ultima_estado = 1;
    Marca::estilo_botao(ultima_btn, Marca::Tipo::DESTAQUE);
    Marca::texto(lv_obj_get_child(ultima_btn, 0), LV_SYMBOL_PLAY " Imprimir de novo");
  }
}

void MainPanel::carregar_miniatura(const std::string &arquivo) {
  json params = {{"filename", arquivo}};
  ws.send_jsonrpc("server.files.metadata", params, [this, arquivo](json &d) {
    // thread do websocket: o download (se houver) acontece fora do lv_lock
    // miniatura com dados estranhos não pode derrubar a tela (exceção na
    // thread do websocket encerra o programa)
    std::pair<std::string, std::pair<size_t, size_t>> thumb;
    try {
      thumb = KUtils::get_thumbnail(arquivo, d, 64);
    } catch (const std::exception &e) {
      spdlog::warn("miniatura de {} ilegível: {}", arquivo, e.what());
    }
    std::string caminho = thumb.first;
    size_t w = thumb.second.first;
    size_t h = thumb.second.second;

    std::lock_guard<std::mutex> lock(lv_lock);
    if (arquivo != ultima_arquivo) {
      return;  // chegou atrasado: já é outro arquivo
    }
    if (caminho.empty() || w == 0 || h == 0) {
      lv_obj_add_flag(ultima_thumb, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ultima_sem_thumb, LV_OBJ_FLAG_HIDDEN);
      return;
    }

    const std::string src = "A:" + caminho;
    lv_img_set_src(ultima_thumb, src.c_str());
    size_t maior = std::max(w, h);
    uint32_t zoom = (uint32_t)(64 * 256 / maior);
    if (zoom == 0) zoom = 1;
    lv_img_set_zoom(ultima_thumb, (uint16_t)zoom);
    lv_obj_center(ultima_thumb);
    lv_obj_clear_flag(ultima_thumb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ultima_sem_thumb, LV_OBJ_FLAG_HIDDEN);
  });
}

void MainPanel::create_sensors(json &temp_sensors) {
  std::lock_guard<std::mutex> lock(lv_lock);
  // os cartões vão ser apagados: o teclado não pode ficar aberto guardando o
  // "this" de um cartão que deixou de existir
  numpad.fechar();
  numpad.set_callback([](double) {});
  sensors.clear();

  // Só os aquecedores controláveis (bico e mesa), bico primeiro.
  std::vector<std::string> chaves;
  for (auto &sensor : temp_sensors.items()) {
    std::string key = sensor.key();
    auto ctrl = sensor.value().find("controllable");
    bool controllable = ctrl != sensor.value().end() && ctrl->is_boolean() && ctrl->template get<bool>();
    bool aquecedor = key.rfind("temperature_fan ", 0) != 0 && key.rfind("temperature_sensor ", 0) != 0;
    if (controllable && aquecedor) {
      chaves.push_back(key);
    }
  }
  std::stable_sort(chaves.begin(), chaves.end(), [](const std::string &a, const std::string &b) {
    auto ordem = [](const std::string &k) { return k == "extruder" ? 0 : (k == "heater_bed" ? 1 : 2); };
    return ordem(a) < ordem(b);
  });
  if (chaves.size() > 2) {
    spdlog::warn("Temperatura: {} aquecedores controláveis; a tela mostra só os dois primeiros", chaves.size());
    chaves.resize(2);
  }

  lv_obj_t *aba = Navegacao::aba(Aba::TEMPERATURA);
  int i = 0;
  for (auto &key : chaves) {
    json &sensor = temp_sensors[key];
    std::string sensor_name = key;

    std::string display_name = sensor["display_name"].is_string()
      ? sensor["display_name"].template get<std::string>() : key;
    // pt-BR: o guppyconfig.json da impressora guarda os nomes padrão em inglês.
    // Traduz só o que aparece; o arquivo e as comparações ("Chamber") ficam iguais.
    if (display_name == "Extruder") display_name = "Bico";
    else if (display_name == "Bed") display_name = "Mesa";
    else if (display_name == "MCU Temp") display_name = "Placa-mãe";
    else if (display_name == "Chamber") display_name = "Câmara";

    auto space_idx = key.find(' ');
    if (space_idx != std::string::npos) {
      sensor_name = key.substr(space_idx + 1);
    }

    const void *sensor_img = &heater;
    if (key == "extruder") {
      sensor_img = &extruder;
    } else if (key == "heater_bed") {
      sensor_img = &bed;
    }

    auto sc = std::make_shared<SensorContainer>(ws, aba, sensor_img,
      display_name.c_str(), lv_color_hex(Marca::GRAFITE), true, false, numpad, sensor_name,
      (lv_obj_t *)NULL, (lv_chart_series_t *)NULL, SensorType::Heater);
    lv_obj_set_pos(sc->get_sensor(), 8, 7 + 84 * i);

    sensors.insert({key, sc});
    i++;
  }
}

void MainPanel::create_fans(json &fans) {
  fan_panel.create_fans(fans);
}

void MainPanel::create_leds(json &leds) {
  led_panel.init(leds);  // prende o lv_lock por dentro

  std::string id;
  for (auto &led : leds.items()) {
    id = led.key();
    break;
  }

  std::lock_guard<std::mutex> lock(lv_lock);
  luz_id = id;
  luz_ligada = -1;
  if (cartao_luz == NULL) {
    return;
  }
  if (luz_id.empty()) {
    lv_obj_clear_flag(cartao_luz, LV_OBJ_FLAG_CLICKABLE);
    Marca::texto(luz_valor, "--");
    Marca::cor_icone(luz_icone, Marca::GRAFITE);
    return;
  }
  lv_obj_add_flag(cartao_luz, LV_OBJ_FLAG_CLICKABLE);
  auto o = State::get_instance()->get_data(json::json_pointer("/printer_state/" + luz_id));
  if (luz_de(o) < 0) {
    Marca::texto(luz_valor, "Desligada");
  }
  atualizar_luz(o);
}

void MainPanel::enable_spoolman() {
  spoolman_panel.init();
  setting_panel.enable_spoolman();
  extruder_panel.enable_spoolman();
  print_panel.enable_spoolman();
}

void MainPanel::show_console() {
  // abre a subtela do Console no terminal, para ver a macro rodando
  Navegacao::abrir(console_cont);
  console_panel.reset_to_terminal();
}

#ifdef SIMULATOR
#include "config.h"

void MainPanel::sim_setup_mock_data() {
  Config *conf = Config::get_instance();
  json &user_sensors = conf->get_json(conf->df() + "monitored_sensors");
  if (user_sensors.is_null()) {
    spdlog::warn("sim_setup_mock_data: no monitored_sensors in config");
    return;
  }

  /* Build a display_sensors json keyed by id, matching State::get_display_sensors
   * shape - but bypass the Moonraker-dependent extruder/heater/sensor lookups. */
  json display_sensors;
  for (auto &s : user_sensors) {
    std::string id = s["id"].template get<std::string>();
    display_sensors[id] = s;
  }
  create_sensors(display_sensors);

  /* Set initial displayed values. */
  for (auto &kv : sensors) {
    const std::string &key = kv.first;
    auto &s = kv.second;
    if (key.find("extruder") != std::string::npos) {
      s->update_value(205); s->update_target(210);
    } else if (key.find("heater_bed") != std::string::npos) {
      s->update_value(62); s->update_target(65);
    } else {
      s->update_value(38);
    }
  }

  /* Seed fans so the Fans screen shows an editable slider (output_pin fan0)
   * plus read-only rows: a binary heater_fan (On/Off) and a PWM output_pin
   * (percentage). */
  {
    json objs;
    objs["result"]["objects"] = json::array({
      "output_pin fan0", "output_pin MainBoardFan", "heater_fan nozzle_fan"});
    State::get_instance()->set_data("printer_objs", objs, "/result");

    json ps;
    ps["params"][0]["output_pin fan0"]["value"] = 0.5;
    ps["params"][0]["output_pin MainBoardFan"]["value"] = 0.4;
    ps["params"][0]["heater_fan nozzle_fan"]["speed"] = 1.0;
    auto &set = ps["params"][0]["configfile"]["settings"];
    set["output_pin fan0"]["pwm"] = true;
    // MainBoardFan has no pwm key on KE (digital-only pin) -> shows On/Off
    State::get_instance()->set_data("printer_state", ps, "/params/0");

    json editable;
    editable["output_pin fan0"] = {{"id", "output_pin fan0"}, {"display_name", "Part Cooling Fan"}};
    create_fans(editable);
  }

  /* Seed a handful of gcode_macros into printer_state so the Macros panel
   * has realistic data to render in the sim: a mix of param/no-param macros,
   * a couple of `_`-private ones (which parse_macros filters out), and enough
   * entries to require scrolling. One is pre-favorited via the guppyscreen DB
   * namespace so the Favorites view isn't empty. */
  {
    auto mock_macro = [](const std::string &gcode) {
      json m;
      m["gcode"] = gcode;
      return m;
    };

    json cfg;
    cfg["gcode_macro LOAD_FILAMENT"] =
      mock_macro("{% set t = params.TEMP|default(220) %}\nM109 S{t}\nG1 E50 F300");
    cfg["gcode_macro UNLOAD_FILAMENT"] =
      mock_macro("{% set t = params.TEMP|default(220) %}\nM109 S{t}\nG1 E-50 F300");
    cfg["gcode_macro HEAT_SOAK"] =
      mock_macro("{% set t = params.TEMP|default(60) %}{% set d = params.DURATION|default(10) %}\nM140 S{t}");
    cfg["gcode_macro START_PRINT"] =
      mock_macro("{% set bed = params.BED|default(60) %}{% set hot = params.EXTRUDER|default(200) %}\nM140 S{bed}");
    cfg["gcode_macro END_PRINT"]    = mock_macro("M104 S0\nM140 S0\nG28 X Y");
    cfg["gcode_macro PURGE"]        = mock_macro("G1 E20 F200");
    cfg["gcode_macro CLEAN_NOZZLE"] = mock_macro("G28\nG1 X10 Y10 F3000");
    cfg["gcode_macro PARK"]         = mock_macro("G1 X0 Y0 F6000");
    cfg["gcode_macro BEEP"]         = mock_macro("M300 S1000 P200");
    cfg["gcode_macro LED_ON"]       = mock_macro("SET_PIN PIN=caselight VALUE=1");
    cfg["gcode_macro LED_OFF"]      = mock_macro("SET_PIN PIN=caselight VALUE=0");
    cfg["gcode_macro PROBE_CALIBRATE"] =
      mock_macro("{% set sp = params.SPEED|default(5) %}\nPROBE_CALIBRATE");
    cfg["gcode_macro CALIBRATE_PRESSURE_ADVANCE_LONG_NAME_TEST"] =
      mock_macro("TUNING_TOWER COMMAND=SET_PRESSURE_ADVANCE");
    cfg["gcode_macro _PRIVATE_HELPER"] = mock_macro("G4 P100");
    cfg["gcode_macro _CG28"]           = mock_macro("G28");

    json ps;
    ps["params"][0]["configfile"]["config"] = cfg;
    State::get_instance()->set_data("printer_state", ps, "/params/0");

    json fav;
    fav["params"][0]["macros"]["settings"]["START_PRINT"]["favorite"] = true;
    State::get_instance()->set_data("guppysettings", fav, "/params/0");

    macros_panel.populate();
  }

  /* Seed a firmware_retraction object so the Retraction panel renders its live
   * controls in the sim. (On a real printer without [firmware_retraction] the
   * object is absent and the panel shows its empty-state instead.) */
  {
    json fr;
    auto &o = fr["params"][0]["firmware_retraction"];
    o["retract_length"] = 0.5;
    o["retract_speed"] = 40;
    o["unretract_extra_length"] = 0.0;
    o["unretract_speed"] = 30;
    auto &d = fr["params"][0]["configfile"]["settings"]["firmware_retraction"];
    d["retract_length"] = 0.5;
    d["retract_speed"] = 40;
    d["unretract_extra_length"] = 0.0;
    d["unretract_speed"] = 30;
    State::get_instance()->set_data("printer_state", fr, "/params/0");
  }

  /* Seed gcode_move so the FineTune / Z-Offset panels and the print-status
   * Z-offset cell render a live value in the sim (no Moonraker to provide it). */
  {
    json gm;
    auto &o = gm["params"][0]["gcode_move"];
    o["homing_origin"] = json::array({0.0, 0.0, -0.040});
    o["speed_factor"] = 1.0;
    o["extrude_factor"] = 1.0;
    State::get_instance()->set_data("printer_state", gm, "/params/0");
  }

  /* Seed the console: recent history, favorites, a gcode.help list spanning the
   * quick-filter chip groups, and some output lines. */
  {
    json hist;
    hist["params"][0]["commandHistory"] = json::array({
      "SAVE_CONFIG", "SET_PRESSURE_ADVANCE ADVANCE=0.04", "QUERY_PROBE",
      "M117 hello", "BED_MESH_CALIBRATE", "G28"});
    State::get_instance()->set_data("console", hist, "/params/0");

    json cfav;
    cfav["params"][0]["console"]["favorites"] = json::array({"G28", "BED_MESH_CALIBRATE"});
    State::get_instance()->set_data("guppysettings", cfav, "/params/0");

    static const char *CMDS[] = {
      "G28","G29","M204","M205","M106","M107","M117","M600",
      "SET_VELOCITY_LIMIT","FORCE_MOVE","SET_KINEMATIC_POSITION","SET_GCODE_OFFSET",
      "SET_HEATER_TEMPERATURE","TEMPERATURE_WAIT","PID_CALIBRATE",
      "BED_MESH_CALIBRATE","BED_MESH_CLEAR","BED_MESH_MAP","BED_MESH_PROFILE","BED_MESH_OFFSET",
      "PROBE","PROBE_ACCURACY","PROBE_CALIBRATE","QUERY_PROBE","Z_OFFSET_APPLY_PROBE",
      "INPUTSHAPER","TEST_RESONANCES","ACCELEROMETER_QUERY","SET_PRESSURE_ADVANCE","SHAPER_CALIBRATE",
      "SET_TMC_CURRENT","DUMP_TMC","INIT_TMC","SET_TMC_FIELD",
      "SET_PIN","SET_FILAMENT_SENSOR","SET_DISPLAY_TEXT","SET_IDLE_TIMEOUT",
      "QUERY_ENDSTOPS","QUERY_ADC","QUERY_FILAMENT_SENSOR",
      "CAM_BRIGHTNESS","CAM_CONTRAST","CAM_HUE","CAM_SATURATION",
      "SAVE_CONFIG","SAVE_GCODE_STATE","SAVE_VARIABLE",
      "RESPOND","STATUS","EXCLUDE_OBJECT","RESTART","FIRMWARE_RESTART",
      "_HOME_Z","_CG28", NULL};
    json help;
    for (int i = 0; CMDS[i] != NULL; i++) {
      help["result"][CMDS[i]] = "mock help";
    }
    console_panel.handle_macros(help);
    json resp;
    resp["params"] = json::array({"Klipper state: Ready", "// probe: open"});
    console_panel.handle_macro_response(resp);
  }

  /* Drive the print status panel so its widgets render with sensible values.
   * GS_PARADA=1: impressora parada (sem impressão falsa), para ver as telas
   * que só funcionam sem impressão. */
  if (std::getenv("GS_PARADA") == nullptr) {
    print_status_panel.sim_setup_mock_data();
  }

  /* Mock temperatures: 300 ms timer (simulator only) moving the card values. */
  lv_timer_t *t = lv_timer_create([](lv_timer_t *timer) {
    auto *self = static_cast<MainPanel *>(timer->user_data);
    static int n = 0;
    n++;
    for (auto &kv : self->sensors) {
      const std::string &key = kv.first;
      auto &s = kv.second;
      int v;
      if (key.find("extruder") != std::string::npos) {
        v = 205 + (n % 12) - 6;
      } else if (key.find("heater_bed") != std::string::npos) {
        v = 62 + (n % 6) - 3;
      } else {
        v = 38 + (n % 8) - 4;
      }
      s->update_value(v);
    }
  }, 300, this);
  (void)t;
}
#endif
