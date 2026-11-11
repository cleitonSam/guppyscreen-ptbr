#ifndef __PRINT_STATUS_PANEL_H__
#define __PRINT_STATUS_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "button_container.h"
#include "image_label.h"
#include "finetune_panel.h"
#include "zoffset_panel.h"
#include "exclude_object_panel.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <ctime>
#include <map>
#include <string>

// Acompanhamento da impressão (Proposta B, quadros 04 e 05) e tela de
// impressão concluída. É a aba Início "virando" o acompanhamento: subtela
// registrada em Navegacao::registrar_acompanhamento, que abre sozinha quando
// uma impressão começa (de qualquer origem: tela, Orca, Mainsail).
class PrintStatusPanel : public NotifyConsumer {
 public:
  // mini_parent não é mais usado (a sobreposição pequena saiu); fica na
  // assinatura para não mudar quem cria o painel.
  PrintStatusPanel(KWebSocketClient &ws, std::mutex &lock, lv_obj_t *mini_parent);
  ~PrintStatusPanel();

  void init(json &fans);
  void reset();
  void populate();
  void foreground();
  void background();

  void handle_metadata(const std::string &gcode_file, json &j);
  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    PrintStatusPanel *panel = (PrintStatusPanel*)event->user_data;
    panel->handle_callback(event);
  };

  void consume(json &j);
  void show_completion(const std::string &state);  // fim da impressão: tela de concluída
  void update_time_progress(uint32_t time_passed);
  void update_layers(json &info);
  int max_layer(json &info);
  int current_layer(json &info);

  FineTunePanel &get_finetune_panel();
  ZOffsetPanel &get_zoffset_panel();

#ifdef SIMULATOR
  // Populate fake print state (filename, progress, elapsed) so the panel can be
  // visually verified without a Moonraker connection.
  void sim_setup_mock_data();
#endif

 private:
  // Um dos 4 cartões do meio (Bico, Mesa, Velocidade, Ventoinha).
  struct Cartao {
    lv_obj_t *cont;
    lv_obj_t *valor;
  };
  Cartao criar_cartao_info(lv_coord_t x, const char *nome, const char *valor);

  void mostrar_estado(const std::string &st);   // chip + Pausar/Retomar
  void atualizar_progresso(int pct);
  void atualizar_resumo();                      // texto do cabeçalho do Ajuste fino
  void atualizar_ventoinha();
  void ler_cartoes_do_estado();                 // valores iniciais dos 4 cartões
  // Preparo (antes da primeira camada): no lugar do progresso, mostra a etapa
  // (aquecendo a mesa, zerando, esperando assentar, medindo, conferindo,
  // aquecendo o bico). Some quando sai o primeiro filamento.
  void atualizar_preparo();
  void confirmar_cancelar();

  KWebSocketClient &ws;
  FineTunePanel finetune_panel;
  ZOffsetPanel zoffset_panel;
  ExcludeObjectPanel exclude_object_panel;
  lv_obj_t *status_cont;

  // miniatura
  lv_obj_t *thumb_card;
  lv_obj_t *thumbnail;
  lv_obj_t *sem_thumb;

  // topo direito
  lv_obj_t *chip;
  lv_obj_t *camada_label;
  lv_obj_t *filename_label;
  lv_obj_t *progress_label;     // f40, só dígitos e %
  lv_obj_t *falta_titulo;
  lv_obj_t *falta_label;
  lv_obj_t *progress_bar;
  lv_obj_t *decorrido_label;
  lv_obj_t *total_label;

  // preparo: cartão no lugar do progresso
  lv_obj_t *preparo_cont;
  lv_obj_t *preparo_titulo;
  lv_obj_t *preparo_etapa;
  lv_obj_t *preparo_detalhe;
  static constexpr int ETAPAS = 7;
  lv_obj_t *preparo_pontos[ETAPAS];
  bool preparo_visivel = false;
  std::string fase_atual;   // "FASE:xxx" do M117 da trava (sem o prefixo)

  // cartões
  Cartao bico;
  Cartao mesa;
  Cartao velocidade;
  Cartao ventoinha;

  // faixa de baixo
  lv_obj_t *pause_btn;
  lv_obj_t *resume_btn;
  lv_obj_t *finetune_btn;
  lv_obj_t *cancel_btn;

  // tela de concluída (subtela própria)
  lv_obj_t *complete_cont;
  lv_obj_t *complete_selo;
  lv_obj_t *complete_title;
  lv_obj_t *complete_file;
  lv_obj_t *complete_stats;
  lv_obj_t *complete_done_btn;
  lv_obj_t *complete_again_btn;
  std::string complete_arquivo;   // caminho do gcode para "Imprimir de novo"

  uint32_t estimated_time_s;

  // Latest virtual_sdcard/progress value (0.0 .. 1.0) and print_stats/filament_used
  // (mm). update_time_progress averages the file-, filament- and slicer-based
  // remaining-time estimates the same way Mainsail does, instead of relying on the
  // slicer's static estimate alone (#126).
  double current_progress = 0.0;
  double current_filament_used = 0.0;
  // Estado anterior de print_stats.state: o acompanhamento abre na passagem
  // para "printing" e fecha/mostra a concluída na saída de printing/paused.
  std::string last_print_state;
  // Arquivo da impressão atual (descarta resposta de metadados atrasada).
  std::string arquivo_atual;

  int extruder_target;
  int heater_bed_target;
  int pct_atual = -1;
  int camada_atual = 0;
  int camadas_total = 0;
  json current_file;

  std::map<std::string, int> fan_speeds;
};

#endif // __PRINT_STATUS_PANEL_H__
