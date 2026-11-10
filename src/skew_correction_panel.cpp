#include "skew_correction_panel.h"
#include "state.h"
#include "utils.h"
#include "marca.h"
#include "navegacao.h"
#include "spdlog/spdlog.h"

#include <string>
#include <cstdlib>

// XY-plane measurement labels (A-C, B-D are the diagonals; A-D the reference).
static const char *FIELD_LABELS[3] = {"A-C", "B-D", "A-D"};

bool SkewCorrectionPanel::is_enabled() {
  // Detect via the RAW config sections, not settings: [skew_correction] is a
  // bare section whose module reads no config options, so Klipper never lists
  // it under configfile.settings (unlike axis_twist_compensation, which has
  // options). It IS always present under configfile.config when in printer.cfg.
  auto cfg = State::get_instance()->get_data(
    "/printer_state/configfile/config/skew_correction"_json_pointer);
  return !cfg.is_null();
}

SkewCorrectionPanel::SkewCorrectionPanel(KWebSocketClient &websocket_client)
  : ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
{
  // Subtela (56,0 424x272) com cabeçalho; o voltar do cabeçalho é o antigo "Voltar".
  Navegacao::subtela(panel_cont);
  Marca::Cabecalho cab = Marca::criar_cabecalho(panel_cont, "Correção de esquadro",
                                                &SkewCorrectionPanel::_handle_callback, this);
  back_btn = cab.voltar;

  // As três medidas ficam na primeira linha (y 48..88), lado a lado: o
  // teclado próprio abre embaixo (y 92..272) sem cobrir o campo.
  for (int i = 0; i < 3; i++) {
    lv_coord_t x = Marca::MARGEM + i * 136;

    lv_obj_t *lbl = Marca::criar_rotulo(panel_cont, FIELD_LABELS[i], Marca::f14(), Marca::GRAFITE);
    lv_obj_set_pos(lbl, x, 60);

    ta[i] = lv_textarea_create(panel_cont);
    lv_textarea_set_one_line(ta[i], true);
    lv_textarea_set_placeholder_text(ta[i], "mm");
    lv_obj_set_style_text_font(ta[i], Marca::f16(), 0);
    lv_obj_set_style_pad_hor(ta[i], 10, 0);
    lv_obj_set_style_pad_ver(ta[i], 10, 0);
    lv_obj_set_pos(ta[i], x + 36, 48);
    lv_obj_set_size(ta[i], 92, 40);
    lv_obj_add_event_cb(ta[i], &SkewCorrectionPanel::_handle_ta, LV_EVENT_ALL, this);
  }

  lv_obj_t *body = Marca::criar_rotulo(panel_cont,
    "Imprima uma peça de calibração de esquadro, meça as três distâncias (mm) "
    "com um paquímetro e digite os valores abaixo. A-C e B-D são as diagonais; A-D é o lado.",
    Marca::f14(), Marca::PAPEL);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(body, Marca::MARGEM, 104);
  lv_obj_set_width(body, 408);

  apply_btn = Marca::criar_botao(panel_cont, LV_SYMBOL_OK "  Aplicar e salvar", Marca::Tipo::DESTAQUE,
                                 Marca::f16());
  lv_obj_set_pos(apply_btn, Marca::MARGEM, 220);
  lv_obj_set_size(apply_btn, 408, 44);
  lv_obj_add_event_cb(apply_btn, &SkewCorrectionPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // Own decimal numeric keyboard (the shared Numpad is integer-only). Digits +
  // "." + backspace + OK (em limão). Fica dentro da subtela, 424 x 180 embaixo;
  // hidden until a field is tapped.
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
  lv_obj_add_event_cb(kb, &SkewCorrectionPanel::_handle_kb, LV_EVENT_ALL, this);
}

SkewCorrectionPanel::~SkewCorrectionPanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
}

void SkewCorrectionPanel::foreground() {
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  Navegacao::abrir(panel_cont);
}

void SkewCorrectionPanel::handle_ta(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, lv_event_get_target(e));
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
  }
}

void SkewCorrectionPanel::handle_kb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}

void SkewCorrectionPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == back_btn) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    Navegacao::fechar(panel_cont);
  } else if (btn == apply_btn) {
    apply_and_save();
  }
}

void SkewCorrectionPanel::apply_and_save() {
  std::string s[3];
  for (int i = 0; i < 3; i++) {
    const char *txt = lv_textarea_get_text(ta[i]);
    s[i] = txt ? std::string(txt) : std::string();
    if (s[i].empty() || std::atof(s[i].c_str()) <= 0.0) {
      KUtils::notify_toast("Digite as três medidas (mm) primeiro.", 4000);
      return;
    }
  }
  // SET_SKEW XY=<ac>,<bd>,<ad> then persist (restarts Klipper).
  ws.gcode_script(fmt::format("SET_SKEW XY={},{},{}\nSAVE_CONFIG",
                              s[0], s[1], s[2]));
  KUtils::notify_toast("Correção de esquadro aplicada.\nSalvando... O Klipper vai reiniciar.", 4000);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  Navegacao::fechar(panel_cont);
}
