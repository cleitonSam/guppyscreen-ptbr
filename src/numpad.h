#ifndef __NUMPAD_H__
#define __NUMPAD_H__

#include "lvgl/lvgl.h"
#include <functional>

// Teclado numérico (só inteiros). O main_panel cria um só, em lv_layer_top(),
// 208 x 272 encostado à direita. Como mora na camada de cima, o
// acompanhamento automático não o esconde.
class Numpad {
 public:
  Numpad(lv_obj_t *parent);
  ~Numpad();

  // O primeiro teclado criado (o do main_panel). Pode ser nulo antes disso.
  static Numpad *compartilhado();

  void set_callback(std::function<void(double)> cb);
  void handle_input(lv_event_t *event);
  /* void handle_defocused(lv_event_t *event); */
  /* initial >= 0 pre-fills the input field (useful for showing current set temp
   * so the user can adjust rather than retype). Pass -1 (default) for blank. */
  void foreground_reset(int initial = -1);

  // set_callback(cb) + foreground_reset(inicial): abre já com o valor atual.
  void abrir(int inicial, std::function<void(double)> cb);
  // Fecha sem enviar nada.
  void fechar();

  static void _handle_input(lv_event_t *event) {
    Numpad *panel = (Numpad*)event->user_data;
    panel->handle_input(event);
  };

  /* static void _handle_defocused(lv_event_t *event) { */
  /*   Numpad *panel = (Numpad*)event->user_data; */
  /*   panel->handle_defocused(event); */
  /* }; */

 private:
  static Numpad *instancia;
  lv_obj_t *edit_cont;
  lv_obj_t *input;
  lv_obj_t *kb;
  std::function<void(double)> ready_cb;
  bool field_changed_this_press = false; // set by textarea VALUE_CHANGED; cleared per keyboard press
};

#endif // __NUMPAD_H__
