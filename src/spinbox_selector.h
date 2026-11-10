#ifndef __SPINBOX_SELECTOR_H__
#define __SPINBOX_SELECTOR_H__

#include "lvgl/lvgl.h"

#include <functional>
#include <string>

// Uma linha 196 x 44 da tela Medições TMC (Proposta B):
// nome 12 G | "-" 44 x 44 | valor 48 x 44 | "+" 44 x 44.
// Segurar "-" ou "+" anda o número; ao soltar, anda mais um e manda o valor
// (igual ao de antes).
class SpinBoxSelector {
 public:
  SpinBoxSelector(lv_obj_t *parent,
		  const std::string &name,
		  int min,
		  int max,
		  int value,
		  std::function<void(int)>);

  ~SpinBoxSelector();

  void update_value(int v);
  void set_hidden(bool h);

 private:
  lv_obj_t *cont;
  lv_obj_t *sb;
  std::function<void(int)> cb;

};

#endif // __SPINBOX_SELECTOR_H__
