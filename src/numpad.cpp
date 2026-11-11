#include "numpad.h"
#include "marca.h"
#include "spdlog/spdlog.h"

#include <string>

Numpad *Numpad::instancia = nullptr;

Numpad::Numpad(lv_obj_t *parent)
  : edit_cont(lv_obj_create(parent))
  , input(lv_textarea_create(edit_cont))
  , kb(lv_keyboard_create(edit_cont))
  , ready_cb([](double v){})
{
  if (instancia == nullptr) {
    instancia = this;
  }

  spdlog::trace("creating numpad");
  lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(edit_cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_move_background(edit_cont);

  // 208 x 272 encostado à direita, PRETO_2 liso
  lv_obj_set_size(edit_cont, 208, Marca::TELA_A);
  lv_obj_align(edit_cont, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(edit_cont, lv_color_hex(Marca::PRETO_2), 0);
  lv_obj_set_style_bg_opa(edit_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(edit_cont, 0, 0);
  lv_obj_set_style_radius(edit_cont, 0, 0);
  lv_obj_set_style_pad_all(edit_cont, 4, 0);
  lv_obj_set_style_pad_row(edit_cont, 4, 0);

  lv_obj_set_flex_flow(edit_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(edit_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_size(input, LV_PCT(100), LV_SIZE_CONTENT);
  lv_textarea_set_one_line(input, true);
  lv_textarea_set_max_length(input, 4);   // 0-9999: nada de número gigante
  lv_obj_set_style_text_font(input, Marca::f20(), 0);
  lv_obj_set_style_text_align(input, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(input, Marca::f20(), LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_set_style_text_align(input, LV_TEXT_ALIGN_CENTER, LV_PART_TEXTAREA_PLACEHOLDER);

  lv_obj_set_width(kb, LV_PCT(100));
  lv_obj_set_flex_grow(kb, 1);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_set_style_pad_gap(kb, 4, 0);
  lv_obj_set_style_text_font(kb, Marca::f20(), LV_PART_ITEMS);
  static const char * kb_map[] = {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", LV_SYMBOL_BACKSPACE, "0", LV_SYMBOL_OK, NULL };
  // OK (último) marcado: fica limão com texto preto pelo tema
  static const lv_btnmatrix_ctrl_t kb_ctrl[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    (lv_btnmatrix_ctrl_t)(1 | LV_BTNMATRIX_CTRL_CHECKED)};
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);

  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
  lv_keyboard_set_textarea(kb, input);

  // lv_textarea_del_char() returns immediately (no VALUE_CHANGED) when cur_pos==0.
  // So if the keyboard fires VALUE_CHANGED for backspace but the textarea did NOT
  // fire its own VALUE_CHANGED, del_char was a no-op → field was already empty → dismiss.
  lv_obj_add_event_cb(kb, [](lv_event_t *e) {
    Numpad *np = (Numpad*)e->user_data;
    bool changed = np->field_changed_this_press;
    np->field_changed_this_press = false;
    if (lv_btnmatrix_get_selected_btn(lv_event_get_target(e)) == 9
        && !changed
        && lv_textarea_get_text(np->input)[0] == '\0') {
      lv_obj_add_flag(np->edit_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_background(np->edit_cont);
      lv_textarea_set_text(np->input, "");
    }
  }, LV_EVENT_VALUE_CHANGED, this);

  lv_obj_add_event_cb(input, &Numpad::_handle_input, LV_EVENT_ALL, this);
  // lv_obj_add_event_cb(edit_cont, &Numpad::_handle_defocused, LV_EVENT_DEFOCUSED, this);
}

Numpad::~Numpad() {
  if (instancia == this) {
    instancia = nullptr;
  }
  if (edit_cont != NULL) {
    lv_obj_del(edit_cont);
    edit_cont = NULL;
  }
}

Numpad *Numpad::compartilhado() {
  return instancia;
}

void Numpad::set_callback(std::function<void(double)> cb) {
  ready_cb = cb;
}

void Numpad::handle_input(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_VALUE_CHANGED) {
    field_changed_this_press = true;
  }

  if (code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(edit_cont);
  }

  if (code == LV_EVENT_READY) {
    // input validation, e.g. range
    std::string value = std::string(lv_textarea_get_text(input));
    if (value.length() > 0) {
      ready_cb(std::stod(value));
    }

    lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(edit_cont);
    lv_textarea_set_text(input, "");
  }
}

// void Numpad::handle_defocused(lv_event_t *e) {
//   const lv_event_code_t code = lv_event_get_code(e);

//   if (code == LV_EVENT_DEFOCUSED) {
//     spdlog::debug("numpad group defocused");
//     lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_move_background(edit_cont);
//     lv_textarea_set_text(input, "");
//   }
// }

void Numpad::foreground_reset(int initial) {
  spdlog::trace("resetting foreground with initial {}", initial);
  // O valor atual aparece apagado (dica) e o campo começa vazio: o que for
  // digitado substitui o número (antes "245" + "250" virava "245250").
  // OK com o campo vazio fecha sem mudar nada.
  lv_textarea_set_text(input, "");
  lv_textarea_set_placeholder_text(input, fmt::format("{}", initial < 0 ? 0 : initial).c_str());
  field_changed_this_press = false;  // lv_textarea_set_text fires VALUE_CHANGED; clear it so it doesn't bleed into the first key press
  lv_obj_clear_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(edit_cont);
}

void Numpad::abrir(int inicial, std::function<void(double)> cb) {
  set_callback(cb);
  foreground_reset(inicial);
}

void Numpad::fechar() {
  if (edit_cont == NULL || lv_obj_has_flag(edit_cont, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }
  lv_obj_add_flag(edit_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(edit_cont);
  lv_textarea_set_text(input, "");
}
