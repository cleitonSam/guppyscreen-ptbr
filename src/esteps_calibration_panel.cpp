#include "esteps_calibration_panel.h"
#include "state.h"
#include "utils.h"
#include "marca.h"
#include "navegacao.h"
#include "spdlog/spdlog.h"

#include <cstdlib>
#include <string>

// Klipper's idle_timeout is effectively disabled on this printer (see the
// esteps_calibration.cfg header), so nothing else stops a forgotten heater -
// same 2-minute backstop the underlying macro already enforces on its own via
// _CALIBRATE_ESTEPS_TIMEOUT; this is just the UI side of that same guarantee.

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

EstepsCalibrationPanel::EstepsCalibrationPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , intro_cont(lv_obj_create(panel_cont))
  , heating_cont(lv_obj_create(panel_cont))
  , mark_cont(lv_obj_create(panel_cont))
  , extruding_cont(lv_obj_create(panel_cont))
  , measure_cont(lv_obj_create(panel_cont))
  , result_cont(lv_obj_create(panel_cont))
  , stage(INTRO)
  , active(false)
  , mark_mm(120.0)
  , length_mm(100.0)
  , speed_mm_min(60.0)
  , extruder_temp(0.0)
  , have_extruder_temp(false)
  , target_temp(0.0)
  , have_target_temp(false)
  , old_rotation_distance(0.0)
  , new_rotation_distance(0.0)
  , commanded_mm(0.0)
  , actual_mm(0.0)
  , have_original_rd(false)
  , original_rotation_distance(0.0)
{
  // Subtela (56,0 424x272) com cabeçalho. O voltar do cabeçalho é o antigo
  // "Voltar" da introdução e só aparece nela (show_stage).
  Navegacao::subtela(panel_cont);
  Marca::Cabecalho cab = Marca::criar_cabecalho(panel_cont, "Calibração da extrusora (E-steps)",
                                                &EstepsCalibrationPanel::_handle_callback, this);
  lv_obj_set_width(cab.titulo, 366);   // nada à direita: o título usa a largura toda
  intro_back_btn = cab.voltar;

  lv_event_cb_t cb = &EstepsCalibrationPanel::_handle_callback;

  // ---------- INTRO ----------
  montar_etapa(intro_cont);
  texto_etapa(intro_cont,
    "Confere se a extrusora empurra a quantidade certa de filamento. Corrige "
    "impressões com excesso ou falta de filamento em toda a peça.\n\n"
    "Separe uma régua e um pedacinho de fita crepe. Primeiro o bico aquece: "
    "a extrusora fica junto do bico, então não dá para testar a frio.",
    Marca::f14(), true);
  start_btn = botao(intro_cont, LV_SYMBOL_OK "  Iniciar", Marca::Tipo::DESTAQUE, cb, this,
                    LV_PCT(100), Marca::f16());

  // ---------- HEATING ----------
  // Sem girador: a linha de estado mostra a temperatura (mais leve).
  montar_etapa(heating_cont);
  titulo_etapa(heating_cont, "Aquecimento");
  heating_status_label = texto_etapa(heating_cont, "Aquecendo...", Marca::f14(), true);
  heating_cancel_btn = botao(heating_cont, LV_SYMBOL_CLOSE "  Cancelar", Marca::Tipo::NORMAL, cb, this,
                             LV_PCT(100));

  // ---------- MARK ----------
  montar_etapa(mark_cont);
  titulo_etapa(mark_cont, "Marque o filamento");
  mark_instr_label = texto_etapa(mark_cont, "", Marca::f14(), true);

  lv_obj_t *mark_act = linha(mark_cont, 44);
  mark_cancel_btn = botao(mark_act, LV_SYMBOL_CLOSE "  Cancelar", Marca::Tipo::NORMAL, cb, this, 136);
  extrude_btn = botao(mark_act, LV_SYMBOL_OK "  Extrudar", Marca::Tipo::DESTAQUE, cb, this);

  // ---------- EXTRUDING ----------
  // Sem girador: a linha de estado mostra quantos mm já saíram.
  montar_etapa(extruding_cont);
  titulo_etapa(extruding_cont, "Extrusão");
  extruding_status_label = texto_etapa(extruding_cont,
    "Extrudando... Aguarde e não mexa na impressora.", Marca::f14(), true);
  extruding_cancel_btn = botao(extruding_cont, LV_SYMBOL_CLOSE "  Cancelar", Marca::Tipo::NORMAL, cb, this,
                               LV_PCT(100));

  // ---------- MEASURE ----------
  // O campo fica na primeira linha (y 48..92): o teclado próprio abre embaixo
  // (y 92..272) e não cobre o que está sendo digitado.
  montar_etapa(measure_cont);
  lv_obj_t *measure_top = linha(measure_cont, Marca::TOQUE);
  lv_obj_set_flex_align(measure_top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  Marca::criar_rotulo(measure_top, "Digite a medida", Marca::f16(), Marca::PAPEL);

  measure_ta = lv_textarea_create(measure_top);
  lv_textarea_set_one_line(measure_ta, true);
  lv_textarea_set_placeholder_text(measure_ta, "distância em mm");
  lv_obj_set_style_text_font(measure_ta, Marca::f16(), 0);
  lv_obj_set_style_pad_hor(measure_ta, 10, 0);
  lv_obj_set_style_pad_ver(measure_ta, 12, 0);
  lv_obj_set_size(measure_ta, 200, Marca::TOQUE);
  lv_obj_add_event_cb(measure_ta, &EstepsCalibrationPanel::_handle_ta, LV_EVENT_ALL, this);

  measure_instr_label = texto_etapa(measure_cont, "", Marca::f14(), true);

  lv_obj_t *measure_act = linha(measure_cont, 44);
  measure_cancel_btn = botao(measure_act, LV_SYMBOL_CLOSE "  Cancelar", Marca::Tipo::NORMAL, cb, this, 136);
  measure_apply_btn = botao(measure_act, LV_SYMBOL_OK "  Aplicar", Marca::Tipo::DESTAQUE, cb, this);

  // Own decimal numeric keyboard (the shared Numpad is integer-only), same
  // construction as skew_correction_panel.cpp's. Fica dentro da subtela,
  // embaixo do campo (424 x 180 em y = 92); o OK em limão.
  kb = lv_keyboard_create(panel_cont);
  static const char *kb_map[] = {"1", "2", "3", "\n",
                                 "4", "5", "6", "\n",
                                 "7", "8", "9", "\n",
                                 LV_SYMBOL_BACKSPACE, "0", ".", LV_SYMBOL_OK, NULL};
  static const lv_btnmatrix_ctrl_t kb_ctrl[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                                (lv_btnmatrix_ctrl_t)(LV_BTNMATRIX_CTRL_CHECKED | 1)};
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(kb, Marca::CONT_L, 180);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_text_font(kb, Marca::f20(), LV_PART_ITEMS);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(kb, &EstepsCalibrationPanel::_handle_kb, LV_EVENT_ALL, this);

  // ---------- RESULT ----------
  montar_etapa(result_cont);
  titulo_etapa(result_cont, "Resultado", Marca::OK, LV_SYMBOL_OK);
  result_label = texto_etapa(result_cont, "", Marca::f14(), true);

  lv_obj_t *result_act = linha(result_cont, 44);
  done_btn = botao(result_act, LV_SYMBOL_OK "  Concluir", Marca::Tipo::NORMAL, cb, this);
  verify_again_btn = botao(result_act, LV_SYMBOL_REFRESH "  Conferir de novo", Marca::Tipo::DESTAQUE, cb, this);
  discard_btn = botao(result_cont, LV_SYMBOL_LEFT "  Descartar e manter o anterior", Marca::Tipo::NORMAL,
                      cb, this, LV_PCT(100));

  show_stage(INTRO);

  ws.register_notify_update(this);
  ws.register_method_callback("notify_gcode_response", "EstepsCalibrationPanel",
    [this](json &d) { this->handle_gcode_response(d); });
}

EstepsCalibrationPanel::~EstepsCalibrationPanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

void EstepsCalibrationPanel::show_stage(Stage s) {
  stage = s;
  lv_obj_t *all[6] = { intro_cont, heating_cont, mark_cont, extruding_cont, measure_cont, result_cont };
  for (auto *c : all) lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
  switch (s) {
    case INTRO:     lv_obj_clear_flag(intro_cont, LV_OBJ_FLAG_HIDDEN); break;
    case HEATING:   lv_obj_clear_flag(heating_cont, LV_OBJ_FLAG_HIDDEN); break;
    case MARK:      lv_obj_clear_flag(mark_cont, LV_OBJ_FLAG_HIDDEN); break;
    case EXTRUDING: lv_obj_clear_flag(extruding_cont, LV_OBJ_FLAG_HIDDEN); break;
    case MEASURE:   lv_obj_clear_flag(measure_cont, LV_OBJ_FLAG_HIDDEN); break;
    case RESULT:    lv_obj_clear_flag(result_cont, LV_OBJ_FLAG_HIDDEN); break;
  }
  // O voltar do cabeçalho substitui o antigo "Voltar" da introdução: só
  // aparece nela. Nas outras etapas saem pelos botões delas (Cancelar,
  // Concluir, Descartar), como antes.
  if (s == INTRO) lv_obj_clear_flag(intro_back_btn, LV_OBJ_FLAG_HIDDEN);
  else            lv_obj_add_flag(intro_back_btn, LV_OBJ_FLAG_HIDDEN);
}

void EstepsCalibrationPanel::encerrar() {
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  show_stage(INTRO);
  Navegacao::fechar(panel_cont);
}

void EstepsCalibrationPanel::foreground() {
  // Agora a coluna da esquerda deixa sair da tela no meio da calibração.
  // Com o bico quente ou uma medida pendente, voltar para a mesma etapa
  // (com o Cancelar / Concluir / Descartar à mão) em vez de recomeçar.
  if (stage != INTRO) {
    Navegacao::abrir(panel_cont);
    return;
  }
  active = false;
  have_extruder_temp = false;
  have_target_temp = false;
  have_original_rd = false;
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_textarea_set_text(measure_ta, "");
  show_stage(INTRO);
  Navegacao::abrir(panel_cont);
}

void EstepsCalibrationPanel::start_calibration() {
  if (KUtils::is_printing()) {
    KUtils::notify_toast("Não é possível calibrar a extrusora durante a impressão.", 3000);
    return;
  }
  active = true;
  have_extruder_temp = false;
  have_target_temp = false;
  show_stage(HEATING);
  update_heating_status();
  ws.gcode_script("CALIBRATE_ESTEPS");
}

void EstepsCalibrationPanel::update_heating_status() {
  if (!have_extruder_temp) {
    lv_label_set_text(heating_status_label, "Aquecendo o bico...");
    return;
  }
  // The hotend may already be hotter than this calibration's target (e.g.
  // left over from printing), in which case M109 is correctly *cooling*
  // toward it, not heating - say so, rather than always claiming "heating"
  // regardless of which direction the temperature is actually moving.
  if (have_target_temp) {
    const double settle_band = 2.0;
    if (extruder_temp < target_temp - settle_band) {
      lv_label_set_text(heating_status_label,
        fmt::format("Aquecendo até {:.0f}°C...\nBico: {:.0f}°C", target_temp, extruder_temp).c_str());
    } else if (extruder_temp > target_temp + settle_band) {
      lv_label_set_text(heating_status_label,
        fmt::format("Esfriando até {:.0f}°C (já estava mais quente)...\nBico: {:.0f}°C", target_temp, extruder_temp).c_str());
    } else {
      lv_label_set_text(heating_status_label,
        fmt::format("Chegando a {:.0f}°C, quase pronto...\nBico: {:.0f}°C", target_temp, extruder_temp).c_str());
    }
  } else {
    lv_label_set_text(heating_status_label,
      fmt::format("Aquecendo o bico...\nBico: {:.0f}°C", extruder_temp).c_str());
  }
}

void EstepsCalibrationPanel::cancel_active_run() {
  active = false;
  ws.gcode_script("_CALIBRATE_ESTEPS_CANCEL");
  show_stage(INTRO);
  KUtils::notify_toast("Calibração da extrusora cancelada.", 3000);
}

void EstepsCalibrationPanel::apply_measurement() {
  const char *txt = lv_textarea_get_text(measure_ta);
  std::string s = txt ? std::string(txt) : std::string();
  if (s.empty() || std::atof(s.c_str()) <= 0.0) {
    KUtils::notify_toast("Digite a distância medida (mm) primeiro.", 3000);
    return;
  }
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  ws.gcode_script(fmt::format("CALIBRATE_ESTEPS_APPLY MEASURED={} MARK={:.1f} LENGTH={:.1f}",
                              s, mark_mm, length_mm));
}

void EstepsCalibrationPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  if (!active || stage != HEATING) return;

  auto et = j["/params/0/extruder/temperature"_json_pointer];
  if (et.is_number()) {
    extruder_temp = et.template get<double>();
    have_extruder_temp = true;
    update_heating_status();
  }
}

void EstepsCalibrationPanel::handle_gcode_response(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  if (!active) return;
  if (!j.contains("params")) return;

  for (auto &el : j["params"]) {
    if (!el.is_string()) continue;
    std::string line = el.template get<std::string>();

    // RESPOND TYPE=command prefixes every line with "// " before it reaches
    // us, so these must be substring searches (matching how the wizard's own
    // z_offset: parsing already works), not anchored-at-0 prefix checks.
    if (line.find("ESTEPS_HEATING") != std::string::npos) {
      // "ESTEPS_HEATING TARGET=200"
      try {
        auto p = line.find("TARGET=");
        target_temp = std::stod(line.substr(p + 7));
        have_target_temp = true;
        update_heating_status();
      } catch (const std::exception &ex) {
        spdlog::warn("EstepsCalibrationPanel: failed to parse ESTEPS_HEATING from '{}': {}", line, ex.what());
      }
      return;
    }

    if (line.find("ESTEPS_READY") != std::string::npos) {
      // "ESTEPS_READY MARK=120 LENGTH=100 SPEED=60"
      try {
        auto get_val = [&line](const std::string &key) {
          auto p = line.find(key + "=");
          return std::stod(line.substr(p + key.size() + 1));
        };
        mark_mm = get_val("MARK");
        length_mm = get_val("LENGTH");
        speed_mm_min = get_val("SPEED");
      } catch (const std::exception &ex) {
        spdlog::warn("EstepsCalibrationPanel: failed to parse ESTEPS_READY from '{}': {}", line, ex.what());
      }
      lv_label_set_text(mark_instr_label,
        fmt::format(
          "Temperatura atingida. A partir de onde o filamento entra no topo "
          "da extrusora, meça {:.0f} mm para cima e enrole ali um pedacinho de "
          "fita crepe no filamento. Com a marca feita, toque em Extrudar. Há "
          "cerca de 2 minutos para isso. Depois, a calibração é cancelada "
          "sozinha para o bico não ficar quente por muito tempo.",
          mark_mm).c_str());
      show_stage(MARK);
      return;
    }

    if (line.find("ESTEPS_PROGRESS") != std::string::npos) {
      // "ESTEPS_PROGRESS FED=40 TOTAL=100"
      try {
        auto fed_p = line.find("FED=");
        auto total_p = line.find("TOTAL=");
        double fed = std::stod(line.substr(fed_p + 4));
        double total = std::stod(line.substr(total_p + 6));
        lv_label_set_text(extruding_status_label,
          fmt::format("Extrudando... {:.0f}/{:.0f} mm\nAguarde e não mexa na impressora.", fed, total).c_str());
      } catch (const std::exception &ex) {
        spdlog::warn("EstepsCalibrationPanel: failed to parse ESTEPS_PROGRESS from '{}': {}", line, ex.what());
      }
      if (stage != EXTRUDING) show_stage(EXTRUDING);
      return;
    }

    if (line.find("ESTEPS_DONE") != std::string::npos) {
      lv_label_set_text(measure_instr_label,
        fmt::format(
          "Pegue a régua. Meça a distância entre a entrada da extrusora e a "
          "marca de fita. Com {:.0f} mm extrudados e a marca a {:.0f} mm, o ideal "
          "é medir cerca de {:.0f} mm. Digite abaixo o valor que mediu.",
          length_mm, mark_mm, mark_mm - length_mm).c_str());
      lv_textarea_set_text(measure_ta, "");
      show_stage(MEASURE);
      return;
    }

    if (line.find("ESTEPS_CANCELLED") != std::string::npos) {
      active = false;
      show_stage(INTRO);
      return;
    }

    if (line.find("ESTEPS_APPLIED") != std::string::npos) {
      // "ESTEPS_APPLIED OLD=1.2345 NEW=1.2400 COMMANDED=100.0 ACTUAL=99.5"
      try {
        auto get_val = [&line](const std::string &key) {
          auto p = line.find(key + "=");
          return std::stod(line.substr(p + key.size() + 1));
        };
        old_rotation_distance = get_val("OLD");
        new_rotation_distance = get_val("NEW");
        commanded_mm = get_val("COMMANDED");
        actual_mm = get_val("ACTUAL");
      } catch (const std::exception &ex) {
        spdlog::warn("EstepsCalibrationPanel: failed to parse ESTEPS_APPLIED from '{}': {}", line, ex.what());
        return;
      }
      if (!have_original_rd) {
        original_rotation_distance = old_rotation_distance;
        have_original_rd = true;
      }
      active = false;
      bool within_tolerance = std::fabs(actual_mm - commanded_mm) < 0.5;
      lv_label_set_text(result_label,
        fmt::format(
          "Foram pedidos {:.1f} mm e saíram {:.1f} mm.\nrotation_distance: {:.4f} -> {:.4f}\n{}",
          commanded_mm, actual_mm, old_rotation_distance, new_rotation_distance,
          within_tolerance
            ? "Diferença de até 0.5 mm: está bom, pode parar aqui."
            : "Diferença maior que 0.5 mm: toque em Conferir de novo para testar o valor novo.").c_str());
      show_stage(RESULT);
      return;
    }
  }
}

void EstepsCalibrationPanel::handle_ta(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, lv_event_get_target(e));
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
  }
}

void EstepsCalibrationPanel::handle_kb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}

void EstepsCalibrationPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (btn == intro_back_btn) {
    Navegacao::fechar(panel_cont);
    return;
  }

  if (btn == start_btn) {
    start_calibration();
    return;
  }

  if (btn == heating_cancel_btn || btn == mark_cancel_btn ||
      btn == extruding_cancel_btn || btn == measure_cancel_btn) {
    cancel_active_run();
    return;
  }

  if (btn == extrude_btn) {
    active = true;
    show_stage(EXTRUDING);
    lv_label_set_text(extruding_status_label, "Extrudando... 0 mm\nAguarde e não mexa na impressora.");
    ws.gcode_script(fmt::format("_CALIBRATE_ESTEPS_EXTRUDE LENGTH={:.1f} SPEED={:.1f} MARK={:.1f}",
                                length_mm, speed_mm_min, mark_mm));
    return;
  }

  if (btn == measure_apply_btn) {
    active = true;
    apply_measurement();
    return;
  }

  if (btn == verify_again_btn) {
    start_calibration();
    return;
  }

  if (btn == done_btn) {
    ws.gcode_script("M104 S0");
    KUtils::notify_toast("Calibração da extrusora salva.", 3000);
    encerrar();
    return;
  }

  if (btn == discard_btn) {
    if (have_original_rd) {
      ws.gcode_script(fmt::format("SET_EXTRUDER_ROTATION_DISTANCE DISTANCE={:.4f}", original_rotation_distance));
    }
    ws.gcode_script("M104 S0");
    KUtils::notify_toast("Descartado. O rotation_distance anterior foi restaurado.", 4000);
    encerrar();
    return;
  }
}
