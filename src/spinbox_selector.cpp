#include "spinbox_selector.h"
#include "marca.h"

SpinBoxSelector::SpinBoxSelector(lv_obj_t *parent,
				 const std::string &name,
				 int min,
				 int max,
				 int value,
				 std::function<void(int)> update_cb)
  : cont(lv_obj_create(parent))
  , sb(NULL)
  , cb(update_cb)
{
  // linha 196 x 44, posições fixas (sem flex): nome | - | valor | +
  lv_obj_set_size(cont, 196, Marca::TOQUE);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *l = Marca::criar_rotulo(cont, name.c_str(), Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(l, 0, 14);
  lv_obj_set_width(l, 50);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);

  lv_obj_t *minus = Marca::criar_botao(cont, LV_SYMBOL_MINUS, Marca::Tipo::NORMAL, Marca::f16());
  lv_obj_set_pos(minus, 52, 0);
  lv_obj_set_size(minus, Marca::TOQUE, Marca::TOQUE);

  sb = lv_spinbox_create(cont);
  lv_obj_set_pos(sb, 100, 0);
  lv_obj_set_size(sb, 48, Marca::TOQUE);
  lv_obj_set_style_text_font(sb, Marca::f14(), 0);
  lv_obj_set_style_pad_all(sb, 0, 0);
  lv_obj_set_style_pad_left(sb, 13, 0);
  lv_obj_set_style_pad_top(sb, 13, 0);   // (44 - 2 de borda - 16 da linha) / 2
  lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);
  lv_spinbox_set_range(sb, min, max);
  lv_spinbox_set_value(sb, value);
  lv_spinbox_set_step(sb, 1);
  lv_spinbox_set_digit_format(sb, 2, 0);
  lv_obj_set_style_border_width(sb, 0, LV_PART_CURSOR);
  lv_obj_set_style_border_opa(sb, LV_OPA_0, LV_PART_CURSOR);
  lv_obj_set_style_bg_opa(sb, LV_OPA_0, LV_PART_CURSOR);
  lv_textarea_set_cursor_click_pos(sb, false);

  lv_obj_t *plus = Marca::criar_botao(cont, LV_SYMBOL_PLUS, Marca::Tipo::NORMAL, Marca::f16());
  lv_obj_set_pos(plus, 152, 0);
  lv_obj_set_size(plus, Marca::TOQUE, Marca::TOQUE);

  lv_obj_add_event_cb(plus, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    SpinBoxSelector *panel = (SpinBoxSelector*)e->user_data;
    if(code == LV_EVENT_LONG_PRESSED_REPEAT) {
      lv_spinbox_increment(panel->sb);
    }

    if (code == LV_EVENT_RELEASED) {
      lv_spinbox_increment(panel->sb);
      if (panel->cb) {
	panel->cb(lv_spinbox_get_value(panel->sb));
      }
    }

  }, LV_EVENT_ALL, this);

  lv_obj_add_event_cb(minus, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    SpinBoxSelector *panel = (SpinBoxSelector*)e->user_data;
    if(code == LV_EVENT_LONG_PRESSED_REPEAT) {
      lv_spinbox_decrement(panel->sb);
    }

    if (code == LV_EVENT_RELEASED) {
      lv_spinbox_decrement(panel->sb);
      if (panel->cb) {
	panel->cb(lv_spinbox_get_value(panel->sb));
      }
    }
  }, LV_EVENT_ALL, this);
}

SpinBoxSelector::~SpinBoxSelector() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void SpinBoxSelector::update_value(int v) {
  lv_spinbox_set_value(sb, v);
}

void SpinBoxSelector::set_hidden(bool h) {
  if (cont != NULL) {
    if (h) {
      lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
    }
  }
}
