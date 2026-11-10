#ifndef __BELTS_CALIBRATION_PANEL_H__
#define __BELTS_CALIBRATION_PANEL_H__

#include "websocket_client.h"
#include "button_container.h"
#include "lvgl/lvgl.h"

#include <mutex>

class BeltsCalibrationPanel {
 public:
  BeltsCalibrationPanel(KWebSocketClient &c, std::mutex &l);
  ~BeltsCalibrationPanel();

  void foreground();
  void handle_callback(lv_event_t *event);
  void handle_image_clicked(lv_event_t *event);
  void handle_macro_response(json &j);
  void handle_update_slider(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    BeltsCalibrationPanel *panel = (BeltsCalibrationPanel*)event->user_data;
    panel->handle_callback(event);
  };
  
  static void _handle_image_clicked(lv_event_t *event) {
    BeltsCalibrationPanel *panel = (BeltsCalibrationPanel*)event->user_data;
    panel->handle_image_clicked(event);
  };

  static void _handle_update_slider(lv_event_t *event) {
    BeltsCalibrationPanel *panel = (BeltsCalibrationPanel*)event->user_data;
    panel->handle_update_slider(event);
  };

 private:
  KWebSocketClient &ws;
  std::mutex &lv_lock;
  lv_obj_t *cont;   // subtela (Navegacao::subtela), 424 x 272
  lv_obj_t *corpo;  // grade abaixo do cabeçalho: (0,44) 424 x 228
  lv_obj_t *graph_cont;
  lv_obj_t *graph;
  lv_obj_t *spinner;
  lv_obj_t *excite_control;
  lv_obj_t *excite_slider;
  lv_obj_t *excite_label;
  lv_obj_t *excite_dd;
  lv_obj_t *button_cont;
  ButtonContainer calibrate_btn;
  ButtonContainer excite_btn;
  // "Parar tudo" e "Voltar" saíram: a parada é o botão vermelho da coluna
  // (sempre visível nas subtelas) e o voltar é o do cabeçalho.
  bool image_fullsized;

  static std::vector<std::string> axes;

};

#endif // __BELTS_CALIBRATION_PANEL_H__
