#ifndef __HOMING_PANEL_H__
#define __HOMING_PANEL_H__

#include "lvgl/lvgl.h"
#include "button_container.h"
#include "websocket_client.h"
#include "selector.h"
#include "notify_consumer.h"

#include <mutex>
#include <string>

// Aba Mover (Controle), quadro 08 da Proposta B: disco X/Y com a origem XY no
// meio, pílula Z (Z+, origem Z, Z-), posição de agora, distância e os botões
// largos Origem XYZ e Soltar motores. A conta do movimento, as setas
// invertidas, os limites dos eixos e a velocidade (100 mm/s) são os de antes.
class HomingPanel : public NotifyConsumer {
public:
  HomingPanel(KWebSocketClient &ws, std::mutex &);
  ~HomingPanel();

  void consume(json &data);
  lv_obj_t *get_container();
  // Mostra a aba Mover (Controle).
  void foreground();
  void handle_callback(lv_event_t *event);
  void handle_selector_cb(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    HomingPanel *panel = (HomingPanel *)event->user_data;
    panel->handle_callback(event);
  };

  static void _handle_selector_cb(lv_event_t *event) {
    HomingPanel *panel = (HomingPanel *)event->user_data;
    panel->handle_selector_cb(event);
  };

private:
  // Botão 48 x 48 com símbolo em cima (filho 0) e rótulo f12 embaixo (filho 1).
  lv_obj_t *criar_seta(lv_coord_t x, lv_coord_t y, const char *simbolo, const char *rotulo);
  // O que o foreground() antigo fazia: setas invertidas e eixos com origem.
  // Roda no gancho ao_mostrar (lv_lock já preso: não prende de novo).
  void atualizar();
  void ajustar_setas();
  void mostrar_posicao(json &pos);
  // Habilita/desabilita os botões pela trava de impressão e pelos eixos com origem.
  void aplicar_estado();
  // Imprimindo e não pausada.
  bool travado();

  KWebSocketClient &ws;
  lv_obj_t *homing_cont;

  lv_obj_t *disco;
  lv_obj_t *y_up_btn;
  lv_obj_t *x_down_btn;
  lv_obj_t *home_xy_btn;
  lv_obj_t *x_up_btn;
  lv_obj_t *y_down_btn;

  lv_obj_t *pilula;
  lv_obj_t *z_down_btn;   // em cima (como no painel antigo)
  lv_obj_t *home_z_btn;
  lv_obj_t *z_up_btn;     // embaixo

  lv_obj_t *pos_x;
  lv_obj_t *pos_y;
  lv_obj_t *pos_z;

  lv_obj_t *dist_titulo;
  lv_obj_t *dist_sel;
  lv_obj_t *aviso;

  lv_obj_t *home_all_btn;
  lv_obj_t *motoroff_btn;

  uint32_t dist_idx;
  std::string eixos;      // último toolhead.homed_axes
};

#endif // __HOMING_PANEL_H__
