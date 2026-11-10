#ifndef __FINETINE_PANEL_H__
#define __FINETINE_PANEL_H__

#include "lvgl/lvgl.h"
#include "button_container.h"
#include "selector.h"
#include "image_label.h"
#include "websocket_client.h"
#include "notify_consumer.h"

#include <functional>
#include <mutex>
#include <string>

// Ajuste fino durante a impressão (Proposta B, quadro 06): subtela com
// cabeçalho e 4 cartões "- valor +" (Velocidade, Fluxo, Ventoinha da peça,
// Altura do bico). Os comandos são os de sempre: M220, M221, os da ventoinha
// por tipo (iguais ao fan_panel) e SET_GCODE_OFFSET Z_ADJUST ... MOVE=1.
class FineTunePanel : public NotifyConsumer {
 public:
  FineTunePanel(KWebSocketClient &, std::mutex &);
  ~FineTunePanel();
  void foreground();
  void handle_callback(lv_event_t *event);

  void consume(json &j);

  // Texto à direita do cabeçalho ("47% · camada 58/240"); o acompanhamento
  // atualiza. Chamar com o lv_lock preso.
  void set_resumo(const std::string &txt);
  // O que o botão "Objetos" do cabeçalho abre (o ExcludeObjectPanel).
  void set_objetos_cb(std::function<void()> cb);

  static void _handle_callback(lv_event_t *event) {
    FineTunePanel *panel = (FineTunePanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  // Um cartão 200 x 76: rótulo, "-", valor, "+".
  struct Linha {
    lv_obj_t *cartao;
    lv_obj_t *menos;
    lv_obj_t *valor;      // caixa do valor (tocável, menos no Z)
    lv_obj_t *valor_txt;
    lv_obj_t *mais;
  };
  Linha criar_linha(lv_coord_t x, lv_coord_t y, const char *titulo, bool valor_tocavel);

  void ajustar_velocidade(int direcao);
  void ajustar_fluxo(int direcao);
  void ajustar_ventoinha(int direcao);
  void ajustar_z(bool subir);
  void mandar_velocidade(int pct);
  void mandar_fluxo(int pct);
  void mandar_ventoinha(int pct);

  int passo_pct();
  int ventoinha_atual();          // -1 se não há ventoinha
  void escolher_ventoinha();      // primeira ventoinha editável
  void atualizar_objetos();
  void mostrar_valores();         // lê tudo do State

  KWebSocketClient &ws;
  lv_obj_t *panel_cont;
  lv_obj_t *titulo;
  lv_obj_t *resumo;
  lv_obj_t *objetos_btn;
  Linha velocidade;
  Linha fluxo;
  Linha ventoinha;
  Linha altura;
  lv_obj_t *passo_pct_sel;        // {"1","5","10"}
  lv_obj_t *passo_mm_sel;         // {"0.01","0.05"}
  uint32_t passo_pct_idx;
  uint32_t passo_mm_idx;

  std::string fan_id;             // chave do Klipper ("fan", "output_pin fan0"...)
  std::string resumo_txt;
  bool tem_objetos;
  std::function<void()> objetos_cb;
};

#endif  // __FINETINE_PANEL_H__
