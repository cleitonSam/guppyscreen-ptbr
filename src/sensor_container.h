#ifndef __SENSOR_CONTAINER_H__
#define __SENSOR_CONTAINER_H__

#include "websocket_client.h"
#include "numpad.h"
#include "lvgl/lvgl.h"

#include <ctime>
#include <string>

enum class SensorType {
  Heater,
  TempFan,
  Sensor
};

// Cartão de temperatura da Proposta B (408 x 76, quem chama posiciona):
//   ícone 24 + nome f14; temperatura atual f20; estado com palavra
//   ("Desligado", "No alvo", "Aquecendo...", "Esfriando...");
//   "Alvo" com − / valor / +  (passo 5 °C). − e + mudam o número na hora e só
//   mandam o comando 700 ms depois do último toque (um lv_timer só, pausado).
//   Tocar no valor abre o teclado numérico. O comando é o mesmo de antes:
//   SET_HEATER_TEMPERATURE HEATER={id} TARGET={v}.
// Os construtores continuam com a mesma assinatura; chart/chart_series nulos
// são aceitos (o gráfico saiu do Início). img_scale é ignorado (ícone 24 px).
class SensorContainer {
public:
  SensorContainer(KWebSocketClient &c,
    lv_obj_t *parent,
    const void *img,
    const char *text,
    lv_color_t color,
    bool editable,
    bool show_target,
    Numpad &np,
    std::string name,
    lv_obj_t *chart,
    lv_chart_series_t *chart_series,
    SensorType type = SensorType::Heater);

  SensorContainer(KWebSocketClient &c,
    lv_obj_t *parent,
    const void *img,
    uint16_t img_scale,
    const char *text,
    lv_color_t color,
    bool editable,
    bool show_target,
    Numpad &np,
    std::string name,
    lv_obj_t *chart,
    lv_chart_series_t *chart_series,
    SensorType type = SensorType::Heater);

  ~SensorContainer();

  lv_obj_t *get_sensor();
  void update_target(int new_target);
  void update_value(int new_value);
  void update_series(int value);
  /* Attach a second chart series that draws a horizontal line at the current
   * target temperature. Set once, updated whenever update_target() fires. */
  void set_target_series(lv_chart_series_t *ts);
  void handle_edit(lv_event_t *event);

  static void _handle_edit(lv_event_t *event) {
    SensorContainer *panel = (SensorContainer *)event->user_data;
    panel->handle_edit(event);
  };

private:
  void ajustar(int delta);
  void enviar_alvo(int v);
  std::string comando_alvo(int v) const;   // "" se o tipo não tem alvo
  void atualizar_estado();
  void mostrar_alvo(int v);
  int alvo_maximo();

  static void _menos_cb(lv_event_t *e);
  static void _mais_cb(lv_event_t *e);
  static void _timer_cb(lv_timer_t *t);

  KWebSocketClient &ws;
  lv_obj_t *sensor_cont;
  lv_obj_t *sensor_img;
  lv_obj_t *sensor_label;
  lv_obj_t *value_label;
  int value;
  lv_obj_t *estado_ponto;
  lv_obj_t *estado_label;
  lv_obj_t *alvo_titulo;
  lv_obj_t *menos_btn;
  lv_obj_t *alvo_cont;
  lv_obj_t *target_label;
  lv_obj_t *mais_btn;
  int target;
  int pendente;          // alvo escolhido com − / + ainda não enviado (-1 = nada)
  int estado_atual;      // último estado mostrado (evita redesenho à toa)
  lv_timer_t *timer_envio;
  Numpad &numpad;
  std::string id;
  lv_obj_t *chart;
  lv_chart_series_t *series;
  lv_chart_series_t *target_series;
  std::time_t last_updated_ts;
  SensorType type;
};

#endif // __SENSOR_CONTAINER_H__
