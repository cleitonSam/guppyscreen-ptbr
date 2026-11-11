#include "axis_twist_panel.h"
#include "state.h"
#include "utils.h"
#include "marca.h"
#include "navegacao.h"
#include "spdlog/spdlog.h"

#include <cmath>
#include <string>

// jog step labels (also valid TESTZ Z= arguments)
static const char *JOG_LABELS[8] = {
  "-1", "-0.1", "-0.05", "-0.01",
  "+0.01", "+0.05", "+0.1", "+1",
};

bool AxisTwistPanel::is_enabled() {
  // The module has no get_status, so it never appears as a live object. Detect
  // it through the parsed config section instead.
  auto cfg = State::get_instance()->get_data(
    "/printer_state/configfile/settings/axis_twist_compensation"_json_pointer);
  return !cfg.is_null();
}

// ---- Peças visuais das etapas (só aparência, nenhum comando) ---------------
// Cada etapa ocupa a área abaixo do cabeçalho: 424 x 228 em (0,44), em
// coluna, com 8 px de margem. Texto à esquerda, botões de 44 px embaixo.
static void montar_etapa(lv_obj_t *c) {
  lv_obj_set_pos(c, 0, 44);
  lv_obj_set_size(c, Marca::CONT_L, Marca::TELA_A - 44);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_hor(c, Marca::MARGEM, 0);
  lv_obj_set_style_pad_top(c, 4, 0);
  lv_obj_set_style_pad_bottom(c, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(c, 8, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
}

// Contêiner sem desenho (transparente), largura toda, em linha.
static lv_obj_t *linha(lv_obj_t *pai, lv_coord_t altura) {
  lv_obj_t *r = lv_obj_create(pai);
  lv_obj_set_size(r, LV_PCT(100), altura);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(r, 0, 0);
  lv_obj_set_style_pad_column(r, 8, 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return r;
}

// Título da etapa (f16 PAPEL). Com selo_txt, vem antes um selo de 24 px
// (fundo OK ou ALERTA, símbolo PAPEL): a palavra do estado fica ao lado.
static lv_obj_t *titulo_etapa(lv_obj_t *pai, const char *txt,
                              uint32_t selo_fundo = Marca::PRETO_3, const char *selo_txt = nullptr) {
  lv_obj_t *r = linha(pai, LV_SIZE_CONTENT);
  if (selo_txt != nullptr) {
    Marca::criar_selo(r, 24, selo_fundo, Marca::PAPEL, selo_txt);
  }
  return Marca::criar_rotulo(r, txt, Marca::f16(), Marca::PAPEL);
}

// Texto da etapa (quebra linha). cresce = ocupa o espaço que sobra e empurra
// os botões para baixo.
static lv_obj_t *texto_etapa(lv_obj_t *pai, const char *txt, const lv_font_t *f, bool cresce) {
  lv_obj_t *l = Marca::criar_rotulo(pai, txt, f, Marca::PAPEL);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, LV_PCT(100));
  if (cresce) lv_obj_set_flex_grow(l, 1);
  return l;
}

// Botão de 44 px. largura 0 = divide a linha com os outros botões.
static lv_obj_t *botao(lv_obj_t *pai, const char *txt, Marca::Tipo t,
                       lv_event_cb_t cb, void *ud, lv_coord_t largura = 0,
                       const lv_font_t *f = nullptr) {
  lv_obj_t *b = Marca::criar_botao(pai, txt, t, f);
  if (largura != 0) {
    lv_obj_set_size(b, largura, 44);
  } else {
    lv_obj_set_height(b, 44);
    lv_obj_set_flex_grow(b, 1);
  }
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

AxisTwistPanel::AxisTwistPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , intro_cont(lv_obj_create(panel_cont))
  , probe_cont(lv_obj_create(panel_cont))
  , done_cont(lv_obj_create(panel_cont))
  , empty_cont(lv_obj_create(panel_cont))
  , stage(INTRO)
  , calibrating(false)
  , last_active(false)
  , last_z(0.0)
  , have_z(false)
  , accepted(0)
  , total(5)
{
  // Subtela (56,0 424x272) com cabeçalho. O voltar do cabeçalho faz o papel
  // dos três "Voltar" antigos (introdução, concluída e "não ativada"); some
  // só durante a medição, que sai pelo Abortar (show_stage).
  Navegacao::subtela(panel_cont);
  Marca::Cabecalho cab = Marca::criar_cabecalho(panel_cont, "Compensação da torção do eixo X",
                                                &AxisTwistPanel::_handle_callback, this);
  lv_obj_set_width(cab.titulo, 366);   // nada à direita: o título usa a largura toda
  intro_back_btn = cab.voltar;
  done_back_btn = cab.voltar;
  empty_back_btn = cab.voltar;

  lv_event_cb_t cb = &AxisTwistPanel::_handle_callback;

  // ---------- INTRO ----------
  montar_etapa(intro_cont);
  texto_etapa(intro_cont,
    "Corrige a primeira camada esmagada de um lado e alta do outro, que a malha da mesa não resolve.\n\n"
    "São 5 pontos ao longo do eixo X. Em cada um, abaixe o bico até a folha de papel "
    "arrastar de leve e toque em Aceitar. O bico deve estar limpo; não precisa aquecer. "
    "A impressora vai à origem e limpa a malha sozinha.",
    Marca::f14(), true);
  start_btn = botao(intro_cont, LV_SYMBOL_OK "  Iniciar calibração", Marca::Tipo::DESTAQUE, cb, this,
                    LV_PCT(100), Marca::f16());

  // ---------- PROBING ----------
  montar_etapa(probe_cont);
  lv_obj_set_style_pad_row(probe_cont, 6, 0);

  // header row: point (left) + Z (right)
  lv_obj_t *hdr = linha(probe_cont, LV_SIZE_CONTENT);
  lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  point_label = Marca::criar_rotulo(hdr, "Ponto 1 de 5", Marca::f16(), Marca::PAPEL);
  z_label = Marca::criar_rotulo(hdr, "Z -", Marca::f20(), Marca::PAPEL);

  instr_label = texto_etapa(probe_cont, "Abaixe até o papel arrastar de leve e toque em Aceitar.",
                            Marca::f14(), true);

  // two jog rows of 4 buttons (down steps, then up steps)
  for (int start_idx = 0; start_idx < 8; start_idx += 4) {
    lv_obj_t *r = linha(probe_cont, 44);
    for (int i = start_idx; i < start_idx + 4; i++) {
      lv_obj_t *b = botao(r, JOG_LABELS[i], Marca::Tipo::NORMAL, cb, this, 0, Marca::f16());
      jogs[i] = { JOG_LABELS[i], b };
    }
  }

  // action row: Abort | Accept
  lv_obj_t *act = linha(probe_cont, 44);
  abort_btn  = botao(act, LV_SYMBOL_CLOSE "  Abortar", Marca::Tipo::NORMAL, cb, this, 0, Marca::f16());
  accept_btn = botao(act, LV_SYMBOL_OK "  Aceitar", Marca::Tipo::DESTAQUE, cb, this, 0, Marca::f16());

  // ---------- DONE ----------
  montar_etapa(done_cont);
  titulo_etapa(done_cont, "Calibração concluída", Marca::OK, LV_SYMBOL_OK);
  done_offsets_label = texto_etapa(done_cont,
    "Confira os valores e toque em Salvar e reiniciar para gravar no printer.cfg.", Marca::f14(), true);
  save_btn = botao(done_cont, LV_SYMBOL_SAVE "  Salvar e reiniciar", Marca::Tipo::DESTAQUE, cb, this,
                   LV_PCT(100), Marca::f16());

  // ---------- EMPTY (not configured) ----------
  montar_etapa(empty_cont);
  texto_etapa(empty_cont,
    "A correção da torção do eixo X não está\n"
    "instalada nesta impressora.",
    Marca::f14(), true);

  show_stage(INTRO);

  ws.register_notify_update(this);
  ws.register_method_callback("notify_gcode_response", "AxisTwistPanel",
    [this](json &d) { this->handle_gcode_response(d); });
}

AxisTwistPanel::~AxisTwistPanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

void AxisTwistPanel::show_stage(Stage s) {
  stage = s;
  lv_obj_t *all[4] = { intro_cont, probe_cont, done_cont, empty_cont };
  for (auto *c : all) lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
  switch (s) {
    case INTRO:   lv_obj_clear_flag(intro_cont, LV_OBJ_FLAG_HIDDEN); break;
    case PROBING: lv_obj_clear_flag(probe_cont, LV_OBJ_FLAG_HIDDEN); break;
    case DONE:    lv_obj_clear_flag(done_cont, LV_OBJ_FLAG_HIDDEN); break;
  }
  // O voltar do cabeçalho some só durante a medição (sai pelo Abortar).
  if (s == PROBING) lv_obj_add_flag(intro_back_btn, LV_OBJ_FLAG_HIDDEN);
  else              lv_obj_clear_flag(intro_back_btn, LV_OBJ_FLAG_HIDDEN);
}

void AxisTwistPanel::encerrar() {
  show_stage(INTRO);
  Navegacao::fechar(panel_cont);
}

void AxisTwistPanel::foreground() {
  if (!is_enabled()) {
    lv_obj_t *all[4] = { intro_cont, probe_cont, done_cont, empty_cont };
    for (auto *c : all) lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(empty_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(intro_back_btn, LV_OBJ_FLAG_HIDDEN);
    Navegacao::abrir(panel_cont);
    return;
  }
  // Agora a coluna da esquerda deixa sair da tela no meio da medição.
  // Medindo (Aceitar/Abortar pendentes) ou com o resultado ainda não salvo:
  // volta para a mesma etapa em vez de recomeçar.
  if (stage == PROBING || stage == DONE) {
    Navegacao::abrir(panel_cont);
    return;
  }
  // fresh entry -> intro
  calibrating = false;
  accepted = 0;
  have_z = false;
  show_stage(INTRO);
  Navegacao::abrir(panel_cont);
}

void AxisTwistPanel::start_calibration() {
  if (KUtils::is_printing()) {
    KUtils::notify_toast("Não é possível calibrar durante a impressão.", 3000);
    return;
  }
  calibrating = true;
  accepted = 0;
  have_z = false;
  last_active = false;
  lv_label_set_text(point_label, "Ponto 1 de 5");
  lv_label_set_text(z_label, "Z -");
  lv_label_set_text(instr_label, "Indo à origem e medindo o primeiro ponto...");
  show_stage(PROBING);
  update_probe_ui();

  if (!KUtils::is_homed()) {
    ws.gcode_script("G28");
  }
  ws.gcode_script("BED_MESH_CLEAR");
  ws.gcode_script("AXIS_TWIST_COMPENSATION_CALIBRATE SAMPLE_COUNT=5");
}

void AxisTwistPanel::update_probe_ui() {
  // jog + accept only usable while the manual probe is actually active
  bool active = last_active;
  for (auto &jg : jogs) {
    if (active) lv_obj_clear_state(jg.btn, LV_STATE_DISABLED);
    else        lv_obj_add_state(jg.btn, LV_STATE_DISABLED);
  }
  if (active) lv_obj_clear_state(accept_btn, LV_STATE_DISABLED);
  else        lv_obj_add_state(accept_btn, LV_STATE_DISABLED);

  lv_label_set_text(point_label,
    fmt::format("Ponto {} de {}", std::min(accepted + 1, total), total).c_str());

  if (active) {
    if (have_z) lv_label_set_text(z_label, fmt::format("Z {:.3f}", last_z).c_str());
    else        lv_label_set_text(z_label, "Z -");
    lv_label_set_text(instr_label, "Abaixe até o papel arrastar de leve e toque em Aceitar.");
  } else {
    lv_label_set_text(instr_label, "Indo ao próximo ponto e medindo...");
  }
}

void AxisTwistPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  if (!calibrating) return;

  auto mp = j["/params/0/manual_probe"_json_pointer];
  if (mp.is_null()) return;

  if (mp.contains("is_active") && mp["is_active"].is_boolean()) {
    last_active = mp["is_active"].template get<bool>();
  }
  if (mp.contains("z_position")) {
    auto zp = mp["z_position"];
    if (zp.is_number()) { last_z = zp.template get<double>(); have_z = true; }
    else { have_z = false; }
  }

  if (stage == PROBING) update_probe_ui();
}

void AxisTwistPanel::handle_gcode_response(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  if (!calibrating) return;
  if (!j.contains("params")) return;

  for (auto &el : j["params"]) {
    if (!el.is_string()) continue;
    std::string line = el.template get<std::string>();

    if (line.find("Calibration complete") != std::string::npos) {
      calibrating = false;
      // strip the leading "// " action prefix if present
      std::string body = line;
      auto p = body.find("offsets:");
      std::string shown = (p != std::string::npos) ? body.substr(p) : body;
      lv_label_set_text(done_offsets_label,
        fmt::format("{}\n\nToque em Salvar e reiniciar para gravar no printer.cfg.", shown).c_str());
      show_stage(DONE);
      return;
    }
    if (line.find("Calibration aborted") != std::string::npos) {
      calibrating = false;
      show_stage(INTRO);
      KUtils::notify_toast("Calibração da torção do eixo X abortada.", 3000);
      return;
    }
  }
}

void AxisTwistPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (btn == intro_back_btn || btn == empty_back_btn || btn == done_back_btn) {
    // Mesmo efeito dos "Voltar" antigos (sai sem salvar); a próxima entrada
    // começa na introdução, como antes.
    encerrar();
    return;
  }

  if (btn == start_btn) {
    start_calibration();
    return;
  }

  if (btn == accept_btn) {
    accepted++;
    ws.gcode_script("ACCEPT");
    update_probe_ui();
    return;
  }

  if (btn == abort_btn) {
    ws.gcode_script("ABORT");
    calibrating = false;
    show_stage(INTRO);
    return;
  }

  if (btn == save_btn) {
    ws.gcode_script("SAVE_CONFIG");
    KUtils::notify_toast("Salvando... O Klipper vai reiniciar.", 3000);
    encerrar();
    return;
  }

  for (auto &jg : jogs) {
    if (btn == jg.btn) {
      ws.gcode_script(std::string("TESTZ Z=") + jg.label);
      return;
    }
  }
}
