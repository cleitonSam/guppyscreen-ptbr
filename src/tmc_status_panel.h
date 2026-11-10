#ifndef __TMC_STATUS_PANEL_H__
#define __TMC_STATUS_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "tmc_status_container.h"
#include "button_container.h"
#include "lvgl/lvgl.h"

#include <map>
#include <memory>

// Subtela "Medições TMC" (Proposta B): cabeçalho com voltar, aviso + chave
// para ligar as medições e, embaixo, um bloco por motor (lista que rola na
// vertical, sem inércia).
class TmcStatusPanel : public NotifyConsumer {
 public:
  TmcStatusPanel(KWebSocketClient &ws,
		 std::mutex &lv_lock);
  ~TmcStatusPanel();

  void foreground();
  void background();

  void init(json &);
  void consume(json &j);

 private:
  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *lista;
  lv_obj_t *top;
  lv_obj_t *toggle;
  std::map<std::string, std::shared_ptr<TmcStatusContainer>> metrics;
};

#endif // __TMC_STATUS_PANEL_H__
