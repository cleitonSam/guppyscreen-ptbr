#ifndef __LED_PANEL_H__
#define __LED_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "notify_consumer.h"
#include "slider_container.h"
#include "button_container.h"

#include <mutex>
#include <map>
#include <string>
#include <vector>

// Subtela "Luz" (Proposta B). Cada luz num cartão 408 x 76:
//  - luz com brilho: "- valor +" (passo 10%; tocar no valor abre o teclado 0-100);
//  - luz só liga/desliga (output_pin sem pwm): botões "Desligar" e "Ligar".
// Os comandos são os mesmos de antes. Registra a ação "luz" da navegação.
class LedPanel : public NotifyConsumer {
 public:
  LedPanel(KWebSocketClient &, std::mutex &);
  ~LedPanel();

  void consume(json &j);

  lv_obj_t *get_container();
  void init(json&);
  // Abre a subtela "Luz" (lv_lock já preso: chamado de evento ou ação).
  void foreground();
  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    LedPanel *panel = (LedPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  struct Luz {
    std::string id;        // "output_pin X", "led X", "neopixel X"...
    bool pino;             // output_pin (SET_PIN) ou não (SET_LED)
    bool binaria;          // output_pin sem pwm: só liga/desliga
    lv_obj_t *cartao;
    lv_obj_t *icone;
    lv_obj_t *estado;      // "Ligada" / "Desligada"
    lv_obj_t *menos;       // com brilho
    lv_obj_t *valor_cont;
    lv_obj_t *valor;
    lv_obj_t *mais;
    lv_obj_t *desligar;    // binária
    lv_obj_t *ligar;
    int pct;               // 0..100 (-1 = ainda sem leitura)
  };

  void ler_estado(Luz &l);
  void mostrar(Luz &l, int pct);
  void enviar(const Luz &l, int pct);
  void ajustar_rolagem();

  KWebSocketClient &ws;
  lv_obj_t *ledpanel_cont;
  lv_obj_t *leds_cont;
  std::vector<Luz> leds;
};

#endif // __LED_PANEL_H__
