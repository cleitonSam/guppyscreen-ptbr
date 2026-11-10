#ifndef __BUTTON_CONTAINER_H__
#define __BUTTON_CONTAINER_H__

#include "lvgl/lvgl.h"
#include "marca.h"

#include <string>
#include <functional>

class ButtonContainer {
 public:
  ButtonContainer(lv_obj_t *parent,
		  const void *btn_img,
		  const char *text,
		  lv_event_cb_t cb,
		  void *user_data,
		  const std::string &prompt_text = {},
		  const std::function<void()> &prompt_callback = {});
  ~ButtonContainer();

  lv_obj_t *get_container();
  lv_obj_t *get_button();
  lv_obj_t *get_label();
  void disable();
  void enable();
  void hide();

  void set_image(const void *img);
  void set_text(const char *text);

  // Visual da marca (Proposta B). Padrão: NORMAL (PRETO_3, raio 12, pad 4,
  // ícone em cima do texto f14).
  void set_tipo(Marca::Tipo t);
  void set_deitado(bool deitado);   // ícone e texto lado a lado
  void sem_icone();                 // esconde a imagem (só texto)
  void set_fonte(const lv_font_t *f);

  void handle_callback(lv_event_t *event);

  void handle_prompt();
  void run_callback();
  
  static void _handle_callback(lv_event_t *event) {
    ButtonContainer *button_container = (ButtonContainer*)event->user_data;
    button_container->handle_callback(event);
  };

 private:
  lv_obj_t *btn_cont;
  lv_obj_t *btn;
  lv_obj_t *label;
  std::string prompt_text;
  std::function<void()> prompt_callback;
};

#endif // __BUTTON_CONTAINER_H__
