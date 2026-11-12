#ifndef __ZOFFSET_PANEL_H__
#define __ZOFFSET_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "selector.h"
#include "lvgl/lvgl.h"

#include <atomic>
#include <mutex>

// "Altura do bico". Subtela com cabeçalho; abre pela página Calibração
// (executar("altura_do_bico")) e pela aba Avançado. Dois usos na mesma tela:
//
//  Imprimindo -> ajuste fino (baby-stepping): SET_GCODE_OFFSET Z_ADJUST ... MOVE=1,
//    passos pequenos. Persistence is handled by the Creality Helper Script's Save
//    Z-Offset macros, which override SET_GCODE_OFFSET to mirror every adjustment
//    into variables.cfg and reload it on boot: a baby-step is saved the instant it
//    is applied, so there is no Save button.
//
//  Parada -> teste do papel: o bico desce com movimento comum (G1 para uma altura
//    exata), então NADA fica salvo no caminho; o botão "Gravar altura" chama a
//    macro GRAVAR_ALTURA_PAPEL, que guarda só a diferença para o zero do sensor.
//    Antes a descida inteira (uns 5 mm) ficava salva como ajuste fino e a
//    impressão seguinte não saía.
class ZOffsetPanel : public NotifyConsumer {
 public:
  ZOffsetPanel(KWebSocketClient &ws, std::mutex &lock);
  ~ZOffsetPanel();

  void foreground();
  void consume(json &j);
  void handle_callback(lv_event_t *e);
  static void _handle_callback(lv_event_t *e) {
    ((ZOffsetPanel *)e->user_data)->handle_callback(e);
  }

 private:
  double cur_offset();
  void update_value(double z);
  void apply_step(bool raise); // raise = nozzle up / farther from bed
  // Parada = teste do papel (passos grandes + "Gravar altura"); imprimindo =
  // ajuste fino (passos pequenos, salva sozinho).
  void aplicar_modo(bool papel);
  // --- teste do papel ---
  double altura_papel(bool *ok);          // altura do bico sobre o ponto gravado (Z do gcode)
  void mostrar_altura();                  // "Bico a 5.00 mm" no cartão
  void passo_papel(bool raise, double passo);
  void mover_papel(const std::string &movimentos, double alvo);
  void ir_ao_centro();
  void gravar_altura();

  KWebSocketClient &ws;
  lv_obj_t *panel_cont;
  lv_obj_t *value_label;       // big live readout
  lv_obj_t *up_btn;            // raise — farther from bed / less squish
  lv_obj_t *down_btn;          // lower — closer to bed / more squish
  lv_obj_t *reset_btn;         // "Zerar" (só no ajuste fino)
  lv_obj_t *auto_save_hint;    // "Os ajustes são salvos automaticamente"
  lv_obj_t *step_selector;     // segmentado: passos finos (imprimindo) ou do papel (parada)
  lv_obj_t *gravar_btn;        // "Gravar altura" (só no teste do papel)
  lv_obj_t *centro_btn;        // "Ir ao centro" (só no teste do papel)
  lv_obj_t *altura_label;      // cabeçalho, à direita: "Ajuste fino +0.129 mm"
  uint32_t step_idx;
  int modo = -1;               // -1 ainda não aplicado, 0 ajuste fino, 1 teste do papel
  // O estado da impressora chega com atraso: logo depois de mandar um movimento
  // vale a altura que eu mandei, não a que o estado ainda mostra.
  double z_previsto = 0.0;
  std::atomic<bool> previsto_valido{false};   // lido fora do lv_lock em consume()
  uint32_t previsto_ms = 0;
};

#endif  // __ZOFFSET_PANEL_H__
