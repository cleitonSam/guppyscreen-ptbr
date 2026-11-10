#include "command_item.h"
#include "marca.h"

LV_IMG_DECLARE(img_star);

CommandItem::CommandItem(lv_obj_t *parent,
			 std::function<void(CommandItem*)> on_sel,
			 std::function<void(CommandItem*)> on_fav)
  : cont(lv_obj_create(parent))
  , fav_img(lv_img_create(cont))
  , label(lv_label_create(cont))
  , favorite(false)
  , visible(true)
  , on_select(on_sel)
  , on_favorite(on_fav)
{
  // linha de no mínimo 44: estrela + comando; separador 1 px PRETO_3;
  // a escolhida ganha fundo PRETO_3 liso (set_highlight)
  lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(cont, Marca::TOQUE, 0);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(cont, 6, 0);
  lv_obj_set_style_pad_left(cont, 8, 0);
  lv_obj_set_style_pad_column(cont, 10, 0);
  lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
  lv_obj_set_style_border_width(cont, 1, 0);
  lv_obj_set_style_border_color(cont, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_set_style_radius(cont, Marca::RAIO_MINI, 0);
  lv_obj_set_style_bg_color(cont, lv_color_hex(Marca::PRETO_3), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(cont, &CommandItem::_handle_select, LV_EVENT_CLICKED, this);

  // favorite star (recolored amber/grey, like the macros panel)
  lv_img_set_src(fav_img, &img_star);
  lv_obj_add_flag(fav_img, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(fav_img, 8);   // estrela pequena: toque um pouco maior
  update_favorite_icon();
  lv_obj_add_event_cb(fav_img, &CommandItem::_handle_favorite, LV_EVENT_CLICKED, this);

  lv_obj_set_flex_grow(label, 1);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_label_set_text(label, "");
}

CommandItem::~CommandItem() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void CommandItem::set_command(const std::string &c) {
  cmd = c;
  lv_label_set_text(label, cmd.c_str());
}

void CommandItem::update_favorite_icon() {
  lv_obj_set_style_img_recolor_opa(fav_img, LV_OPA_COVER, LV_PART_MAIN);
  // favorito: PAPEL (aceso); não favorito: GRAFITE (apagado)
  lv_obj_set_style_img_recolor(fav_img,
    lv_color_hex(favorite ? Marca::PAPEL : Marca::GRAFITE), LV_PART_MAIN);
}

void CommandItem::set_favorite(bool fav) {
  favorite = fav;
  update_favorite_icon();
}

void CommandItem::set_highlight(bool h) {
  if (h) {
    // fundo PRETO_3 liso; o comando inteiro quebra em linhas (sem texto rolando)
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  } else {
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  }
}

void CommandItem::set_visible(bool v) {
  visible = v;
  if (v) {
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void CommandItem::handle_select(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && on_select) {
    on_select(this);
  }
}

void CommandItem::handle_favorite(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED && on_favorite) {
    on_favorite(this);
  }
}
