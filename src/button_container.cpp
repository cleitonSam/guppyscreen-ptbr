#include "button_container.h"
#include "config.h"
#include "navegacao.h"
#include "utils.h"
#include "spdlog/spdlog.h"

static const char *TEXTO_PARADA = "Fazer parada de emergência?";

ButtonContainer::ButtonContainer(lv_obj_t *parent,
  const void *btn_img,
  const char *text,
  lv_event_cb_t cb,
  void *user_data,
  const std::string &prompt,
  const std::function<void()> &pcb)
  : btn_cont(lv_obj_create(parent))
  , btn(lv_imgbtn_create(btn_cont))
  , label(lv_label_create(btn_cont))
  , prompt_text(prompt)
  , prompt_callback(pcb)
{
  // Visual da marca: contêiner PRETO_3, raio 12, sem borda, pad 4.
  lv_obj_set_style_pad_all(btn_cont, 4, 0);
  lv_obj_set_style_pad_row(btn_cont, 0, 0);
  lv_obj_set_style_pad_column(btn_cont, 6, 0);
  auto width_scale = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;
  lv_obj_set_size(btn_cont, 110 * width_scale, LV_SIZE_CONTENT);
  Marca::estilo_botao(btn_cont, Marca::Tipo::NORMAL);
  lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
  // lv_obj_add_style(btn_cont, &style, LV_PART_MAIN);
  lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_RELEASED, NULL, btn_img, NULL);
  lv_obj_set_width(btn, LV_SIZE_CONTENT);
  // lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
  // lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
  // lv_obj_add_flag(btn_cont, LV_OBJ_FLAG_EVENT_BUBBLE);

  // A parada de emergência SEMPRE pergunta (janela de tela inteira da
  // Navegacao, em handle_prompt). A chave /prompt_emergency_stop continua no
  // arquivo, mas não desliga mais a pergunta.
  if (cb != NULL && !pcb) {
    lv_obj_add_event_cb(btn_cont, &ButtonContainer::_handle_callback, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(btn_cont, &ButtonContainer::_handle_callback, LV_EVENT_RELEASED, this);

    lv_obj_add_event_cb(btn_cont, cb, LV_EVENT_CLICKED, user_data);
  } else if (cb != NULL && pcb) {
    {
      lv_obj_add_event_cb(btn_cont, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
          ((ButtonContainer *)e->user_data)->handle_prompt();
        }
        }, LV_EVENT_CLICKED, this);
    }
  }

  lv_label_set_text(label, text);
  lv_obj_set_size(label, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  // texto f14 PAPEL (herda a cor do contêiner); desabilitado GRAFITE
  lv_obj_set_style_text_font(label, Marca::f14(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(Marca::GRAFITE), LV_STATE_DISABLED);

  // lv_obj_align_to(label, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
  // lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 5);
  // lv_obj_set_style_border_width(btn_cont, 2, 0);
}

ButtonContainer::~ButtonContainer() {
}

lv_obj_t *ButtonContainer::get_container() {
  return btn_cont;
}

lv_obj_t *ButtonContainer::get_button() {
  return btn;
}

lv_obj_t *ButtonContainer::get_label() {
  return label;
}

void ButtonContainer::set_text(const char *text) {
  lv_label_set_text(label, text);
}

void ButtonContainer::disable() {
  lv_obj_add_state(btn, LV_STATE_DISABLED);
  lv_obj_add_state(btn_cont, LV_STATE_DISABLED);
  lv_obj_add_state(label, LV_STATE_DISABLED);

}

void ButtonContainer::enable() {
  lv_obj_clear_state(btn, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_cont, LV_STATE_DISABLED);
  lv_obj_clear_state(label, LV_STATE_DISABLED);
}

void ButtonContainer::hide() {
  lv_obj_add_flag(btn_cont, LV_OBJ_FLAG_HIDDEN);
}

void ButtonContainer::set_image(const void *img) {
  lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_RELEASED, NULL, img, NULL);
}

void ButtonContainer::handle_callback(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    lv_imgbtn_set_state(btn, LV_IMGBTN_STATE_PRESSED);
  } else if (code == LV_EVENT_RELEASED) {
    lv_imgbtn_set_state(btn, LV_IMGBTN_STATE_RELEASED);
  }
}

void ButtonContainer::handle_prompt() {
  // parada de emergência: sempre a janela de tela inteira
  if (prompt_text == TEXTO_PARADA) {
    Navegacao::confirmar_parada();
    return;
  }

  // Safe action on the left, destructive on the right (red).
  static const char *btns[] = {"Cancelar", "Confirmar", ""};

  lv_obj_t *mbox1 = lv_msgbox_create(NULL, NULL, prompt_text.c_str(), btns, false);
  KUtils::style_lock_mbox(mbox1, 90);
  lv_obj_t *btnm = lv_msgbox_get_btns(mbox1);

  // Draw the right (Confirm) button in red.
  lv_obj_add_event_cb(btnm, [](lv_event_t *e) {
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part == LV_PART_ITEMS && dsc->id == 1 && dsc->rect_dsc != NULL && dsc->label_dsc != NULL) {
      dsc->rect_dsc->bg_color = lv_color_hex(Marca::ERRO);
      dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
      dsc->label_dsc->color   = lv_color_hex(Marca::PAPEL);
    }
  }, LV_EVENT_DRAW_PART_BEGIN, NULL);

  lv_obj_add_event_cb(mbox1, [](lv_event_t *e) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));
    uint32_t clicked_btn = lv_msgbox_get_active_btn(obj);
    if (clicked_btn == 1) {
      ((ButtonContainer *)e->user_data)->run_callback();
    }
    lv_msgbox_close(obj);
  }, LV_EVENT_VALUE_CHANGED, this);

  lv_obj_center(mbox1);
}

void ButtonContainer::run_callback() {
  prompt_callback();
}

void ButtonContainer::set_tipo(Marca::Tipo t) {
  Marca::estilo_botao(btn_cont, t);
  if (t == Marca::Tipo::DESTAQUE) {
    // ícone preto no limão (também apertado, para não sumir no limão)
    lv_obj_set_style_img_recolor(btn, lv_color_hex(Marca::PRETO), 0);
    lv_obj_set_style_img_recolor_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(btn, lv_color_hex(Marca::PRETO), LV_STATE_PRESSED);
    lv_obj_set_style_img_recolor_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    // desabilitado o fundo vira PRETO_3: ícone grafite (preto sumiria)
    lv_obj_set_style_img_recolor(btn, lv_color_hex(Marca::GRAFITE), LV_STATE_DISABLED);
    lv_obj_set_style_img_recolor_opa(btn, LV_OPA_COVER, LV_STATE_DISABLED);
  } else {
    lv_obj_remove_local_style_prop(btn, LV_STYLE_IMG_RECOLOR, 0);
    lv_obj_remove_local_style_prop(btn, LV_STYLE_IMG_RECOLOR_OPA, 0);
    lv_obj_remove_local_style_prop(btn, LV_STYLE_IMG_RECOLOR, LV_STATE_PRESSED);
    lv_obj_remove_local_style_prop(btn, LV_STYLE_IMG_RECOLOR_OPA, LV_STATE_PRESSED);
    lv_obj_remove_local_style_prop(btn, LV_STYLE_IMG_RECOLOR, LV_STATE_DISABLED);
    lv_obj_remove_local_style_prop(btn, LV_STYLE_IMG_RECOLOR_OPA, LV_STATE_DISABLED);
  }
}

void ButtonContainer::set_deitado(bool deitado) {
  if (deitado) {
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(label, LV_SIZE_CONTENT);
  } else {
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(label, LV_PCT(100));
  }
  lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

void ButtonContainer::sem_icone() {
  lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
}

void ButtonContainer::set_fonte(const lv_font_t *f) {
  lv_obj_set_style_text_font(label, f, 0);
}
