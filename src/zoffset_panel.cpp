#include "zoffset_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "utils.h"

// Altura do bico: subtela 424 x 272 com cabeçalho (lógica e comandos iguais).
//   y =  52  cartão 408 x 48: valor f20 à esquerda, aviso de salvamento à direita
//   y = 108  "Subir  (mais longe da mesa)"   408 x 44
//   y = 160  "Descer  (mais perto da mesa)"  408 x 44
//   y = 212  "Passo (mm)" + segmentado 220 x 44 + "Zerar" 104 x 44

namespace {
  static const char *MAPA_PASSO[] = {"0.001", "0.005", "0.01", "0.025", "0.05", ""};
}

ZOffsetPanel::ZOffsetPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , step_idx(2)   // 0.01 mm
{
  Navegacao::subtela(panel_cont);
  Marca::criar_cabecalho(panel_cont, "Altura do bico");

  // valor ao vivo + aviso de que salva sozinho
  lv_obj_t *card = Marca::criar_cartao(panel_cont, 8, 52, 408, 48);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
  value_label = Marca::criar_rotulo(card, "0.000 mm", Marca::f20(), Marca::PAPEL);
  lv_obj_set_pos(value_label, 12, 13);
  lv_obj_set_width(value_label, 120);
  lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);

  // the Helper Script's Save Z-Offset macros persist every adjustment to
  // variables.cfg automatically, so there is no Save button - just say so.
  auto_save_hint = Marca::criar_rotulo(card, LV_SYMBOL_OK "  Os ajustes são salvos automaticamente",
                                       Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(auto_save_hint, 128, 17);
  lv_obj_set_width(auto_save_hint, 268);
  lv_label_set_long_mode(auto_save_hint, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(auto_save_hint, LV_TEXT_ALIGN_RIGHT, 0);

  // raise / lower buttons: full-width, clear physical direction
  up_btn = Marca::criar_botao(panel_cont, LV_SYMBOL_UP "  Subir  (mais longe da mesa)", Marca::Tipo::NORMAL);
  lv_obj_set_pos(up_btn, 8, 108);
  lv_obj_set_size(up_btn, 408, Marca::TOQUE);
  lv_obj_add_event_cb(up_btn, &ZOffsetPanel::_handle_callback, LV_EVENT_CLICKED, this);

  down_btn = Marca::criar_botao(panel_cont, LV_SYMBOL_DOWN "  Descer  (mais perto da mesa)", Marca::Tipo::NORMAL);
  lv_obj_set_pos(down_btn, 8, 160);
  lv_obj_set_size(down_btn, 408, Marca::TOQUE);
  lv_obj_add_event_cb(down_btn, &ZOffsetPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // passo
  lv_obj_t *passo = Marca::criar_rotulo(panel_cont, "Passo (mm)", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(passo, 8, 226);
  lv_obj_set_width(passo, 74);

  step_selector = lv_btnmatrix_create(panel_cont);
  lv_btnmatrix_set_map(step_selector, MAPA_PASSO);
  lv_obj_set_pos(step_selector, 84, 212);
  lv_obj_set_size(step_selector, 220, Marca::TOQUE);
  Marca::estilo_segmentado(step_selector);
  lv_btnmatrix_set_btn_ctrl_all(step_selector, LV_BTNMATRIX_CTRL_CHECKABLE | LV_BTNMATRIX_CTRL_NO_REPEAT);
  lv_btnmatrix_set_one_checked(step_selector, true);
  lv_btnmatrix_set_btn_ctrl(step_selector, step_idx, LV_BTNMATRIX_CTRL_CHECKED);
  lv_obj_add_event_cb(step_selector, &ZOffsetPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  reset_btn = Marca::criar_botao(panel_cont, LV_SYMBOL_REFRESH "  Zerar", Marca::Tipo::NORMAL);
  lv_obj_set_pos(reset_btn, 312, 212);
  lv_obj_set_size(reset_btn, 104, Marca::TOQUE);
  lv_obj_add_event_cb(reset_btn, &ZOffsetPanel::_handle_callback, LV_EVENT_CLICKED, this);

  Navegacao::registrar_acao("altura_do_bico", [this](const std::string &) { foreground(); });

  ws.register_notify_update(this);
}

ZOffsetPanel::~ZOffsetPanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

double ZOffsetPanel::cur_offset() {
  auto v = State::get_instance()->get_data(
    "/printer_state/gcode_move/homing_origin/2"_json_pointer);
  return v.is_number() ? v.template get<double>() : 0.0;
}

void ZOffsetPanel::update_value(double z) {
  Marca::texto(value_label, fmt::format("{:.3f} mm", z).c_str());
}

void ZOffsetPanel::apply_step(bool raise) {
  // SET_GCODE_OFFSET ... MOVE=1 needs a homed axis: unhomed, Klipper still sets
  // the offset (the readout would change) but the move fails and the Helper
  // Script's save macro aborts before SAVE_VARIABLE - so it'd be applied live,
  // not moved, and never saved. Guard before sending so the three never drift.
  if (!KUtils::is_homed()) {
    KUtils::show_homing_prompt(ws);
    return;
  }

  const char *step = lv_btnmatrix_get_btn_text(step_selector, step_idx);
  if (step == NULL) {
    return;
  }

  // SET_GCODE_OFFSET operates in Klipper's normalized gcode frame, where
  // negative Z_ADJUST always means closer to the bed - independent of the
  // printer's physical stepper wiring. invert_z_direction only corrects the
  // on-screen jog arrows for manual G0 moves (see homing_panel.cpp) and must
  // not be applied here.
  const char *sign = raise ? "+" : "-";
  ws.gcode_script(fmt::format("SET_GCODE_OFFSET Z_ADJUST={}{} MOVE=1", sign, step));
}

void ZOffsetPanel::foreground() {
  update_value(cur_offset());
  Navegacao::abrir(panel_cont);
}

void ZOffsetPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto v = j["/params/0/gcode_move/homing_origin/2"_json_pointer];
  if (v.is_number()) update_value(v.template get<double>());
}

void ZOffsetPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *sel = lv_event_get_target(e);
    if (sel == step_selector) {
      uint16_t idx = lv_btnmatrix_get_selected_btn(sel);
      if (idx != LV_BTNMATRIX_BTN_NONE) {
        step_idx = idx;
      }
    }
    return;
  }

  if (code != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (btn == up_btn) {
    apply_step(true);
  } else if (btn == down_btn) {
    apply_step(false);
  } else if (btn == reset_btn) {
    if (!KUtils::is_homed()) {
      KUtils::show_homing_prompt(ws);
      return;
    }
    ws.gcode_script("SET_GCODE_OFFSET Z=0 MOVE=1");
  }
}
