#include "recalibration_wizard_panel.h"
#include "state.h"
#include "utils.h"
#include "marca.h"
#include "navegacao.h"
#include "spdlog/spdlog.h"

#include <cmath>
#include <string>
#include <fstream>
#include <iterator>

// jog step labels (also valid TESTZ Z= arguments) - same set as Axis Twist's
// probing UI, since PROBE_CALIBRATE and AXIS_TWIST_COMPENSATION_CALIBRATE
// share the same underlying Klipper manual_probe helper and TESTZ/ACCEPT/
// ABORT commands.
static const char *JOG_LABELS[8] = {
  "-1", "-0.1", "-0.05", "-0.01",
  "+0.01", "+0.05", "+0.1", "+1",
};

// Load-sensor refine step (optional, post-baseline only - see header comment
// and project_prtouch_mechanism_research memory). GuppyScreen runs on the
// same device as Klipper, so backup/restore is plain local file I/O, no
// SSH/network round-trip needed.
static const char *CONFIG_PATH = "/usr/data/printer_data/config/printer.cfg";
static const char *CONFIG_BACKUP_PATH = "/usr/data/printer_data/config/printer.cfg.bak-wizard-sensor";
// Wipe+heat+probe+multi-point bed-tilt cross-check usually takes 1-3min, but
// confirmed live (2026-07-08) to sometimes take 4.5+min - a 4-minute timeout
// fired while the macro was still genuinely running, and because Klipper
// processes gcode serially, the resulting FIRMWARE_RESTART queued behind the
// still-active macro instead of running immediately: the macro's own self-save
// then landed *after* the safety restore, clobbering it. Set well above the
// observed worst case so this failure path stays a true "actually hung" signal.
static const uint32_t SENSOR_TIMEOUT_MS = 480000;
// Random-noise band from repeated same-session runs (see Test D): a gap this
// size or smaller just guides the recommendation shown to the user - it does
// NOT auto-apply either value, the user always picks (2026-07-08 decision).
static const double SENSOR_AGREEMENT_THRESHOLD_MM = 0.1;

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

RecalibrationWizardPanel::RecalibrationWizardPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , intro_cont(lv_obj_create(panel_cont))
  , probe_cont(lv_obj_create(panel_cont))
  , mesh_cont(lv_obj_create(panel_cont))
  , done_cont(lv_obj_create(panel_cont))
  , sensor_cont(lv_obj_create(panel_cont))
  , sensor_choice_cont(lv_obj_create(panel_cont))
  , stage(INTRO)
  , active(false)
  , probe_active(false)
  , last_z(0.0)
  , have_z(false)
  , old_z_offset(0.0)
  , new_z_offset(0.0)
  , have_new_z_offset(false)
  , sensor_reading(0.0)
  , have_sensor_reading(false)
  , sensor_step_active(false)
  , config_backed_up(false)
  , sensor_timeout_timer(NULL)
  , sensor_extruder_temp(0.0)
  , sensor_bed_temp(0.0)
  , have_sensor_temps(false)
{
  // Subtela (56,0 424x272) com cabeçalho. O voltar do cabeçalho é o antigo
  // "Voltar" da introdução e só aparece nela (show_stage).
  Navegacao::subtela(panel_cont);
  Marca::Cabecalho cab = Marca::criar_cabecalho(panel_cont, "Assistente de recalibração",
                                                &RecalibrationWizardPanel::_handle_callback, this);
  lv_obj_set_width(cab.titulo, 366);   // nada à direita: o título usa a largura toda
  intro_back_btn = cab.voltar;

  lv_event_cb_t cb = &RecalibrationWizardPanel::_handle_callback;

  // ---------- INTRO ----------
  montar_etapa(intro_cont);
  texto_etapa(intro_cont,
    "Trocou a mesa, o bico ou o suporte do CR Touch? Refaça a calibração "
    "básica antes de imprimir de novo.\n\n"
    "Primeiro, o teste do papel acerta a altura do bico. Depois, a impressora "
    "mede a malha da mesa sozinha. Nada é salvo antes de revisar e confirmar no fim.",
    Marca::f14(), true);
  start_btn = botao(intro_cont, LV_SYMBOL_OK "  Iniciar", Marca::Tipo::DESTAQUE, cb, this,
                    LV_PCT(100), Marca::f16());

  // ---------- ZOFFSET (PROBE_CALIBRATE) ----------
  montar_etapa(probe_cont);
  lv_obj_set_style_pad_row(probe_cont, 6, 0);

  lv_obj_t *hdr = linha(probe_cont, LV_SIZE_CONTENT);
  lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  Marca::criar_rotulo(hdr, "Etapa 1 de 2: altura do bico", Marca::f16(), Marca::PAPEL);
  z_label = Marca::criar_rotulo(hdr, "Z -", Marca::f20(), Marca::PAPEL);

  instr_label = texto_etapa(probe_cont, "Indo à origem e medindo...", Marca::f14(), true);

  // two jog rows of 4 buttons (down steps, then up steps)
  for (int start_idx = 0; start_idx < 8; start_idx += 4) {
    lv_obj_t *r = linha(probe_cont, 44);
    for (int i = start_idx; i < start_idx + 4; i++) {
      lv_obj_t *b = botao(r, JOG_LABELS[i], Marca::Tipo::NORMAL, cb, this, 0, Marca::f16());
      jogs[i] = { JOG_LABELS[i], b };
    }
  }

  lv_obj_t *act = linha(probe_cont, 44);
  abort_btn  = botao(act, LV_SYMBOL_CLOSE "  Abortar", Marca::Tipo::NORMAL, cb, this, 0, Marca::f16());
  accept_btn = botao(act, LV_SYMBOL_OK "  Aceitar", Marca::Tipo::DESTAQUE, cb, this, 0, Marca::f16());

  // ---------- MESH (automatic) ----------
  // Sem girador: o texto já diz o que está acontecendo (mais leve).
  montar_etapa(mesh_cont);
  titulo_etapa(mesh_cont, "Etapa 2 de 2: malha da mesa");
  mesh_status_label = texto_etapa(mesh_cont,
    "Medindo a mesa... leva cerca de 1 minuto. Não precisa tocar em nada.", Marca::f14(), true);

  // ---------- DONE ----------
  montar_etapa(done_cont);
  titulo_etapa(done_cont, "Pronto para revisar", Marca::OK, LV_SYMBOL_OK);
  done_summary_label = texto_etapa(done_cont, "", Marca::f14(), true);

  sensor_refine_btn = botao(done_cont, "Refinar com o sensor de pressão", Marca::Tipo::NORMAL,
                            cb, this, LV_PCT(100));
  lv_obj_t *done_act = linha(done_cont, 44);
  done_back_btn = botao(done_act, LV_SYMBOL_LEFT "  Descartar", Marca::Tipo::NORMAL, cb, this, 136);
  save_btn = botao(done_act, LV_SYMBOL_SAVE "  Salvar e reiniciar", Marca::Tipo::DESTAQUE, cb, this);

  // ---------- SENSOR_RUNNING (Z_OFFSET_CALIBRATION, single reading, automatic) ----------
  // Sem girador: a linha de estado mostra o que acontece e as temperaturas.
  montar_etapa(sensor_cont);
  titulo_etapa(sensor_cont, "Refinando com o sensor de pressão");
  sensor_status_label = texto_etapa(sensor_cont,
    "Indo à origem, limpando o bico e medindo...\nNão mexa na impressora.", Marca::f14(), true);

  // ---------- SENSOR_CHOICE (paper vs. sensor average, user always picks) ----------
  montar_etapa(sensor_choice_cont);
  titulo_etapa(sensor_choice_cont, "Escolha qual valor manter", Marca::ALERTA, LV_SYMBOL_WARNING);
  sensor_choice_label = texto_etapa(sensor_choice_cont, "", Marca::f14(), true);

  lv_obj_t *choice_act = linha(sensor_choice_cont, 44);
  keep_paper_btn = botao(choice_act, "Manter valor do papel", Marca::Tipo::NORMAL, cb, this);
  use_sensor_btn = botao(choice_act, "Usar valor do sensor", Marca::Tipo::NORMAL, cb, this);
  discard_all_btn = botao(sensor_choice_cont, "Descartar os dois e manter o original",
                          Marca::Tipo::NORMAL, cb, this, LV_PCT(100));

  show_stage(INTRO);

  ws.register_notify_update(this);
  ws.register_method_callback("notify_gcode_response", "RecalibrationWizardPanel",
    [this](json &d) { this->handle_gcode_response(d); });
}

RecalibrationWizardPanel::~RecalibrationWizardPanel() {
  disarm_sensor_timeout();
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

void RecalibrationWizardPanel::show_stage(Stage s) {
  spdlog::info("RecalibrationWizardPanel: show_stage {} -> {}", (int)stage, (int)s);
  stage = s;
  lv_obj_t *all[6] = { intro_cont, probe_cont, mesh_cont, done_cont, sensor_cont, sensor_choice_cont };
  for (auto *c : all) lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
  switch (s) {
    case INTRO:  lv_obj_clear_flag(intro_cont, LV_OBJ_FLAG_HIDDEN); break;
    case ZOFFSET: lv_obj_clear_flag(probe_cont, LV_OBJ_FLAG_HIDDEN); break;
    case MESH:   lv_obj_clear_flag(mesh_cont, LV_OBJ_FLAG_HIDDEN); break;
    case DONE:   lv_obj_clear_flag(done_cont, LV_OBJ_FLAG_HIDDEN); break;
    case SENSOR_RUNNING: lv_obj_clear_flag(sensor_cont, LV_OBJ_FLAG_HIDDEN); break;
    case SENSOR_CHOICE:  lv_obj_clear_flag(sensor_choice_cont, LV_OBJ_FLAG_HIDDEN); break;
  }
  // O voltar do cabeçalho substitui o antigo "Voltar" da introdução: só
  // aparece nela. Nas outras etapas saem pelos botões delas (Abortar,
  // Descartar, Salvar...), como antes.
  if (s == INTRO) lv_obj_clear_flag(intro_back_btn, LV_OBJ_FLAG_HIDDEN);
  else            lv_obj_add_flag(intro_back_btn, LV_OBJ_FLAG_HIDDEN);
}

void RecalibrationWizardPanel::encerrar() {
  show_stage(INTRO);
  Navegacao::fechar(panel_cont);
}

void RecalibrationWizardPanel::foreground() {
  // Agora a coluna da esquerda deixa sair da tela no meio do assistente.
  // Se ele estava esperando o dono (papel, revisão, escolha) ou o sensor,
  // volta para a mesma etapa em vez de recomeçar: assim os botões Aceitar /
  // Abortar / Salvar / Descartar continuam ao alcance. Na malha (automática,
  // sem botão) recomeça como antes.
  if (stage == ZOFFSET || stage == DONE || stage == SENSOR_RUNNING || stage == SENSOR_CHOICE) {
    Navegacao::abrir(panel_cont);
    return;
  }
  active = false;
  probe_active = false;
  have_z = false;
  have_new_z_offset = false;
  sensor_step_active = false;
  config_backed_up = false;
  have_sensor_reading = false;
  disarm_sensor_timeout();
  show_stage(INTRO);
  Navegacao::abrir(panel_cont);
}

void RecalibrationWizardPanel::start_wizard() {
  if (KUtils::is_printing()) {
    KUtils::notify_toast("Não é possível recalibrar durante a impressão.", 3000);
    return;
  }

  auto z = State::get_instance()->get_data(
    "/printer_state/configfile/settings/bltouch/z_offset"_json_pointer);
  old_z_offset = z.is_number() ? z.template get<double>() : 0.0;
  have_new_z_offset = false;

  active = true;
  probe_active = false;
  have_z = false;
  lv_label_set_text(z_label, "Z -");
  lv_label_set_text(instr_label, "Indo à origem e medindo...");
  show_stage(ZOFFSET);
  update_probe_ui();
  // No auto-cancel timer here (2026-07-08 decision) - this step is manual and
  // already has its own Abort button, unlike the automated sensor-refine
  // step below which does need a watchdog since nothing prompts the user.

  // Always home unconditionally, even if Klipper currently reports already
  // homed - this wizard's whole purpose is "hardware just changed," so a
  // cached homed_axes from before that change is exactly what shouldn't be
  // trusted here (2026-07-08 fix: a stale "already homed" skipped G28 on a
  // real run, which is the one case this wizard exists to protect against).
  ws.gcode_script("G28");
  ws.gcode_script("PROBE_CALIBRATE");
}

void RecalibrationWizardPanel::update_probe_ui() {
  bool en = probe_active;
  for (auto &jg : jogs) {
    if (en) lv_obj_clear_state(jg.btn, LV_STATE_DISABLED);
    else    lv_obj_add_state(jg.btn, LV_STATE_DISABLED);
  }
  if (en) lv_obj_clear_state(accept_btn, LV_STATE_DISABLED);
  else    lv_obj_add_state(accept_btn, LV_STATE_DISABLED);

  if (en) {
    if (have_z) lv_label_set_text(z_label, fmt::format("Z {:.3f}", last_z).c_str());
    else        lv_label_set_text(z_label, "Z -");
    lv_label_set_text(instr_label, "Coloque uma folha de papel embaixo do bico. Abaixe até o papel arrastar de leve e toque em Aceitar.");
  } else {
    lv_label_set_text(instr_label, "Indo à origem e medindo...");
  }
}

void RecalibrationWizardPanel::start_mesh() {
  show_stage(MESH);
  lv_label_set_text(mesh_status_label, "Medindo a mesa... leva cerca de 1 minuto. Não precisa tocar em nada.");
  ws.gcode_script("BED_MESH_CALIBRATE");
}

bool RecalibrationWizardPanel::backup_printer_cfg() {
  std::ifstream src(CONFIG_PATH, std::ios::binary);
  if (!src) {
    spdlog::warn("RecalibrationWizardPanel: could not open {} for backup", CONFIG_PATH);
    config_backed_up = false;
    return false;
  }
  std::ofstream dst(CONFIG_BACKUP_PATH, std::ios::binary | std::ios::trunc);
  if (!dst) {
    spdlog::warn("RecalibrationWizardPanel: could not open {} for backup write", CONFIG_BACKUP_PATH);
    config_backed_up = false;
    return false;
  }
  dst << src.rdbuf();
  dst.flush();
  config_backed_up = dst.good();
  return config_backed_up;
}

bool RecalibrationWizardPanel::restore_printer_cfg_backup() {
  if (!config_backed_up) return false;
  std::ifstream src(CONFIG_BACKUP_PATH, std::ios::binary);
  if (!src) {
    spdlog::warn("RecalibrationWizardPanel: could not open {} to restore", CONFIG_BACKUP_PATH);
    return false;
  }
  std::ofstream dst(CONFIG_PATH, std::ios::binary | std::ios::trunc);
  if (!dst) {
    spdlog::warn("RecalibrationWizardPanel: could not open {} to restore into", CONFIG_PATH);
    return false;
  }
  dst << src.rdbuf();
  dst.flush();
  return dst.good();
}

bool RecalibrationWizardPanel::patch_z_offset_value(double value) {
  std::ifstream in(CONFIG_PATH, std::ios::binary);
  if (!in) {
    spdlog::warn("RecalibrationWizardPanel: could not open {} to patch z_offset", CONFIG_PATH);
    return false;
  }
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();

  static const std::string marker = "#*# z_offset = ";
  auto pos = content.find(marker);
  if (pos == std::string::npos) {
    spdlog::warn("RecalibrationWizardPanel: could not find '{}' in {}", marker, CONFIG_PATH);
    return false;
  }
  auto value_start = pos + marker.size();
  auto line_end = content.find('\n', value_start);
  if (line_end == std::string::npos) line_end = content.size();
  content.replace(value_start, line_end - value_start, fmt::format("{:.3f}", value));

  std::ofstream out(CONFIG_PATH, std::ios::binary | std::ios::trunc);
  if (!out) {
    spdlog::warn("RecalibrationWizardPanel: could not open {} to write patched z_offset", CONFIG_PATH);
    return false;
  }
  out << content;
  out.flush();
  return out.good();
}

void RecalibrationWizardPanel::arm_sensor_timeout() {
  disarm_sensor_timeout();
  sensor_timeout_timer = lv_timer_create(&RecalibrationWizardPanel::sensor_timeout_cb, SENSOR_TIMEOUT_MS, this);
  lv_timer_set_repeat_count(sensor_timeout_timer, 1);
}

void RecalibrationWizardPanel::disarm_sensor_timeout() {
  if (sensor_timeout_timer != NULL) {
    lv_timer_del(sensor_timeout_timer);
    sensor_timeout_timer = NULL;
  }
}

void RecalibrationWizardPanel::sensor_timeout_cb(lv_timer_t *t) {
  auto *self = (RecalibrationWizardPanel *)t->user_data;
  self->sensor_timeout_timer = NULL;
  if (!self->sensor_step_active) return;
  self->sensor_refine_failed("a impressora não respondeu em 8 minutos");
}

void RecalibrationWizardPanel::sensor_refine_failed(const char *reason) {
  disarm_sensor_timeout();
  active = false;
  sensor_step_active = false;
  if (config_backed_up) {
    restore_printer_cfg_backup();
    ws.gcode_script("FIRMWARE_RESTART");
  }
  show_stage(INTRO);
  KUtils::notify_toast(
    fmt::format("Falha ao refinar com o sensor ({}). As configurações anteriores foram restauradas. Toque em Iniciar para refazer.", reason).c_str(),
    6000);
}

void RecalibrationWizardPanel::update_sensor_status_text() {
  std::string temp_str = have_sensor_temps
    ? fmt::format("\nBico: {:.0f}°C   Mesa: {:.0f}°C", sensor_extruder_temp, sensor_bed_temp)
    : "";
  lv_label_set_text(sensor_status_label,
    fmt::format("Indo à origem, limpando o bico e medindo...{}\nNão mexa na impressora.", temp_str).c_str());
}

void RecalibrationWizardPanel::start_sensor_refine() {
  if (KUtils::is_printing()) {
    KUtils::notify_toast("Não é possível recalibrar durante a impressão.", 3000);
    return;
  }
  if (!backup_printer_cfg()) {
    KUtils::notify_toast("Não foi possível guardar uma cópia do printer.cfg. Por segurança, o refino com o sensor não foi feito.", 4000);
    return;
  }

  have_sensor_reading = false;
  have_sensor_temps = false;
  sensor_step_active = true;
  active = true;
  show_stage(SENSOR_RUNNING);
  update_sensor_status_text();
  arm_sensor_timeout();

  // Always home unconditionally too (see start_wizard() - same "don't trust
  // a cached homed flag in this wizard" reasoning).
  ws.gcode_script("G28");
  ws.gcode_script("CRTENSE_NOZZLE_CLEAR");
  ws.gcode_script("Z_OFFSET_CALIBRATION");
}

void RecalibrationWizardPanel::finish_sensor_reading() {
  disarm_sensor_timeout();
  active = false;
  sensor_step_active = false;

  double diff = std::fabs(sensor_reading - new_z_offset);
  bool sensor_recommended = diff <= SENSOR_AGREEMENT_THRESHOLD_MM;

  lv_label_set_text(sensor_choice_label,
    fmt::format(
      "Teste do papel: {:.3f} mm  |  Sensor: {:.3f} mm  |  Diferença: {:.3f} mm\n{}",
      new_z_offset, sensor_reading, diff,
      sensor_recommended
        ? "Valores parecidos: a leitura do sensor (verde) costuma ser a mais precisa."
        : "Diferença maior que o normal (0.1 mm): o teste do papel (verde) é a escolha mais segura.").c_str());

  // The text calls the recommended button "verde": it gets the brand's OK
  // green (background, PAPEL text); the other one stays a plain NORMAL button.
  auto recomendar = [](lv_obj_t *b, bool sim) {
    Marca::estilo_botao(b, Marca::Tipo::NORMAL);
    if (sim) {
      lv_obj_set_style_bg_color(b, lv_color_hex(Marca::OK), 0);
      lv_obj_set_style_bg_color(b, lv_color_darken(lv_color_hex(Marca::OK), LV_OPA_20), LV_STATE_PRESSED);
    }
  };
  recomendar(keep_paper_btn, !sensor_recommended);
  recomendar(use_sensor_btn, sensor_recommended);

  show_stage(SENSOR_CHOICE);
}

void RecalibrationWizardPanel::apply_sensor_choice(bool use_sensor) {
  double chosen = use_sensor ? sensor_reading : new_z_offset;
  if (!patch_z_offset_value(chosen)) {
    KUtils::notify_toast("Falha ao gravar a altura do bico (z_offset) no printer.cfg. Confira o arquivo antes de imprimir.", 6000);
    encerrar();
    return;
  }
  ws.gcode_script("FIRMWARE_RESTART");
  KUtils::notify_toast(fmt::format("Salvando {:.3f} mm... O Klipper vai reiniciar.", chosen).c_str(), 3000);
  encerrar();
}

void RecalibrationWizardPanel::apply_discard_all() {
  // Reverts to whatever printer.cfg had before this whole wizard run started -
  // both the paper-test z_offset/mesh and the sensor's readings are abandoned.
  if (!restore_printer_cfg_backup()) {
    KUtils::notify_toast("Não há cópia para restaurar. O printer.cfg ficou como o sensor salvou por último.", 6000);
    encerrar();
    return;
  }
  ws.gcode_script("FIRMWARE_RESTART");
  KUtils::notify_toast("Descartando esta recalibração... O Klipper vai reiniciar com as configurações originais.", 4000);
  encerrar();
}

void RecalibrationWizardPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  if (!active) return;

  auto mp = j["/params/0/manual_probe"_json_pointer];
  if (!mp.is_null()) {
    if (mp.contains("is_active") && mp["is_active"].is_boolean()) {
      probe_active = mp["is_active"].template get<bool>();
    }
    if (mp.contains("z_position")) {
      auto zp = mp["z_position"];
      if (zp.is_number()) { last_z = zp.template get<double>(); have_z = true; }
      else { have_z = false; }
    }
    if (stage == ZOFFSET) update_probe_ui();
  }

  if (stage == SENSOR_RUNNING) {
    bool temps_changed = false;
    auto et = j["/params/0/extruder/temperature"_json_pointer];
    if (et.is_number()) { sensor_extruder_temp = et.template get<double>(); temps_changed = true; }
    auto bt = j["/params/0/heater_bed/temperature"_json_pointer];
    if (bt.is_number()) { sensor_bed_temp = bt.template get<double>(); temps_changed = true; }
    if (temps_changed) {
      have_sensor_temps = true;
      update_sensor_status_text();
    }
  }
}

void RecalibrationWizardPanel::handle_gcode_response(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  if (!active) return;
  if (!j.contains("params")) return;

  for (auto &el : j["params"]) {
    if (!el.is_string()) continue;
    std::string line = el.template get<std::string>();
    spdlog::info("RecalibrationWizardPanel: [stage={}] gcode response: {}", (int)stage, line);

    if (stage == ZOFFSET) {
      auto p = line.find("z_offset:");
      if (p != std::string::npos) {
        try {
          new_z_offset = std::stod(line.substr(p + std::string("z_offset:").size()));
          have_new_z_offset = true;
        } catch (const std::exception &ex) {
          spdlog::warn("RecalibrationWizardPanel: failed to parse z_offset from '{}': {}", line, ex.what());
        }
        probe_active = false;
        start_mesh();
        return;
      }
      if (line.find("Calibration aborted") != std::string::npos) {
        active = false;
        show_stage(INTRO);
        KUtils::notify_toast("Recalibração abortada.", 3000);
        return;
      }
    }

    if (stage == MESH && line.find("Mesh Bed Leveling Complete") != std::string::npos) {
      active = false;
      lv_label_set_text(done_summary_label,
        fmt::format(
          "Altura do bico: {:.3f} mm -> {:.3f} mm\nMalha da mesa: atualizada. Nada foi salvo ainda.\n"
          "Escolha uma opção: Salvar e reiniciar agora ou refinar antes (opcional).",
          old_z_offset, new_z_offset).c_str());
      show_stage(DONE);
      return;
    }

    if (stage == SENSOR_RUNNING) {
      // Any PR_ERR_CODE (e.g. PRES_LOST_RUN_DATA - see
      // project_prtouch_mechanism_research memory) means the sensor's own
      // firmware hit its zero-retry failure path. Bail safely rather than
      // trying to continue or interpret partial data.
      if (line.find("PR_ERR_CODE") != std::string::npos) {
        sensor_refine_failed(line.c_str());
        return;
      }
      auto p = line.find("z_offset:");
      if (p != std::string::npos) {
        try {
          sensor_reading = std::stod(line.substr(p + std::string("z_offset:").size()));
          have_sensor_reading = true;
        } catch (const std::exception &ex) {
          spdlog::warn("RecalibrationWizardPanel: failed to parse sensor z_offset from '{}': {}", line, ex.what());
        }
        finish_sensor_reading();
        return;
      }
    }
  }
}

void RecalibrationWizardPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(e);
  spdlog::info("RecalibrationWizardPanel: button pressed, stage={}, btn={}"
    " (intro_back={} start={} accept={} abort={} save={} sensor_refine={} done_back={}"
    " keep_paper={} use_sensor={} discard_all={})",
    (int)stage, (void*)btn,
    btn == intro_back_btn, btn == start_btn, btn == accept_btn, btn == abort_btn,
    btn == save_btn, btn == sensor_refine_btn, btn == done_back_btn,
    btn == keep_paper_btn, btn == use_sensor_btn, btn == discard_all_btn);

  if (btn == intro_back_btn) {
    Navegacao::fechar(panel_cont);
    return;
  }

  if (btn == done_back_btn) {
    // Discard: nothing was ever saved (SAVE_CONFIG only fires from the Save
    // button), so leaving just needs a restart-free bail - re-home clears any
    // live-only offset the manual probe applied so the printer isn't left
    // trusting an unreviewed number.
    ws.gcode_script("G28");
    encerrar();
    return;
  }

  if (btn == start_btn) {
    start_wizard();
    return;
  }

  if (btn == accept_btn) {
    ws.gcode_script("ACCEPT");
    return;
  }

  if (btn == abort_btn) {
    ws.gcode_script("ABORT");
    active = false;
    show_stage(INTRO);
    return;
  }

  if (btn == save_btn) {
    ws.gcode_script("SAVE_CONFIG");
    KUtils::notify_toast("Salvando... O Klipper vai reiniciar.", 3000);
    encerrar();
    return;
  }

  if (btn == sensor_refine_btn) {
    start_sensor_refine();
    return;
  }

  if (btn == keep_paper_btn) {
    apply_sensor_choice(false);
    return;
  }

  if (btn == use_sensor_btn) {
    apply_sensor_choice(true);
    return;
  }

  if (btn == discard_all_btn) {
    apply_discard_all();
    return;
  }

  for (auto &jg : jogs) {
    if (btn == jg.btn) {
      ws.gcode_script(std::string("TESTZ Z=") + jg.label);
      return;
    }
  }
}
