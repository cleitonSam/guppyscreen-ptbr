#ifndef __FAN_PANEL_H__
#define __FAN_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "notify_consumer.h"
#include "slider_container.h"
#include "button_container.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Aba Ventoinhas (Controle), Proposta B: cada ventoinha editável num cartão
// 408 x 76 com "- valor +" (passo de 10%; tocar no valor abre o teclado
// numérico 0-100). As ventoinhas só de leitura ficam numa linha 12 grafite
// cada, abaixo. Os comandos por tipo de ventoinha são os mesmos de antes.
class FanPanel : public NotifyConsumer {
 public:
  FanPanel(KWebSocketClient &ws, std::mutex &lock);
  ~FanPanel();

  void consume(json &j);

  lv_obj_t *get_container();
  void create_fans(json &f);
  // Mostra a aba Ventoinhas (Controle).
  void foreground();
  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    FanPanel *panel = (FanPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  struct Ventoinha {
    std::string id;        // chave do Klipper: "fan", "output_pin X", "fan_generic X"...
    lv_obj_t *cartao;
    lv_obj_t *estado;      // "Ligada" / "Desligada" (+ RPM, se houver)
    lv_obj_t *menos;
    lv_obj_t *valor_cont;  // caixa do valor (tocar abre o teclado)
    lv_obj_t *valor;
    lv_obj_t *mais;
    int pct;               // 0..100
  };

  void atualizar();      // gancho ao_mostrar (lv_lock já preso)
  void ler_estado(Ventoinha &v);
  void mostrar(Ventoinha &v, int pct);
  void enviar(const std::string &id, int pct);
  void ajustar_rolagem();
  void create_readonly_fans();                  // read-only fans (heater_fan, controller_fan, fan output pins)
  void update_readonly();
  bool is_binary_fan(const std::string &key);
  std::string fmt_ro_value(const std::string &key);

  KWebSocketClient &ws;
  lv_obj_t *fanpanel_cont;
  lv_obj_t *fans_cont;
  std::vector<Ventoinha> fans;
  std::map<std::string, lv_obj_t*> ro_labels;   // read-only fan id -> value label
  lv_obj_t *ro_cartao = nullptr;                 // cartão das ventoinhas automáticas
};

#endif // __FAN_PANEL_H__
