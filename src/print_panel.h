#ifndef __PRINT_PANEL_H__
#define __PRINT_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "notify_consumer.h"
#include "file_panel.h"
#include "print_status_panel.h"
#include "tree.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class SpoolmanPanel;

// Página Arquivos (Proposta B, quadros 02 e 03): abas Local / USB / Histórico,
// 3 cartões por vez com setas de página (sem rolagem), e a subtela Detalhes
// do arquivo (FilePanel) com o botão Imprimir.
class PrintPanel : public NotifyConsumer {
public:
  PrintPanel(KWebSocketClient &ws, std::mutex &lv_lock, PrintStatusPanel &ps, SpoolmanPanel &sm);
  ~PrintPanel();

  // Enables the "use the same filament?" confirm dialog at print start.
  // Called by MainPanel once Moonraker reports the spoolman component.
  void enable_spoolman();

  void consume(json &data);
  void populate_files(json &data);
  void subscribe();
  // Vai para a página Arquivos (o gancho ao_mostrar atualiza a lista).
  void foreground();

  void handle_callback(lv_event_t *event);        // toque num cartão da lista
  void handle_print_callback(lv_event_t *event);  // botão Imprimir dos Detalhes
  void handle_btns(lv_event_t *event);            // ordenar e setas de página
  void handle_tabs(lv_event_t *event);            // Local / USB / Histórico
  void handle_filament_btns(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    PrintPanel *panel = (PrintPanel *)event->user_data;
    panel->handle_callback(event);
  };

  static void _handle_print_callback(lv_event_t *event) {
    PrintPanel *panel = (PrintPanel *)event->user_data;
    panel->handle_print_callback(event);
  };

  static void _handle_btns(lv_event_t *event) {
    PrintPanel *panel = (PrintPanel *)event->user_data;
    panel->handle_btns(event);
  };

  static void _handle_tabs(lv_event_t *event) {
    PrintPanel *panel = (PrintPanel *)event->user_data;
    panel->handle_tabs(event);
  };

  static void _handle_filament_btns(lv_event_t *event) {
    PrintPanel *panel = (PrintPanel *)event->user_data;
    panel->handle_filament_btns(event);
  };


private:
  // um cartão da lista (criado uma vez, só reescrito)
  struct Card {
    lv_obj_t *card = NULL;
    lv_obj_t *mini = NULL;       // caixinha 56 x 56 PRETO_3
    lv_obj_t *img = NULL;        // miniatura (escondida sem miniatura pequena)
    lv_obj_t *icon = NULL;       // símbolo de pasta / arquivo / pendrive
    lv_obj_t *badge = NULL;      // selo do histórico (concluída / cancelada / erro)
    lv_obj_t *name = NULL;
    lv_obj_t *meta = NULL;
    std::string img_src;         // o que está no img agora (evita recarregar)
  };

  // o que cada linha da lista mostra
  enum class EntryType { UP, FOLDER, FILE, HISTORY, EMPTY };
  struct Entry {
    EntryType type;
    Tree *node;                  // UP, FOLDER, FILE
    int history;                 // HISTORY: índice em history_jobs
  };

  struct HistoryJob {
    std::string filename;
    std::string status;
    int64_t duration;
    double when;
  };

  enum { TAB_LOCAL = 0, TAB_USB = 1, TAB_HISTORY = 2 };

  void show_dir(Tree *dir);
  void show_list();               // monta as entradas da aba atual e mostra a página
  void show_page();               // reescreve os 3 cartões e as setas
  void draw_card(int i, const Entry &e);
  void request_list_metadata(Tree *f);
  void request_history();
  void update_sort_label();
  Tree *find_file(const std::string &path);
  Tree *usb_dir();
  void open_file(const std::string &path);
  void show_file_detail(Tree *f);
  void show_filament_dialog();
  void start_pending_print();

  KWebSocketClient &ws;
  SpoolmanPanel &sm;
  lv_obj_t *files_cont;

  // spoolman "use the same filament?" confirm dialog
  lv_obj_t *filament_cont;
  lv_obj_t *filament_box;
  lv_obj_t *filament_content_cont;
  lv_obj_t *filament_row_cont;
  lv_obj_t *filament_title_label = NULL;
  lv_obj_t *filament_swatch;
  lv_obj_t *filament_name_label;
  lv_obj_t *filament_detail_label;
  lv_obj_t *filament_enough_label;
  lv_obj_t *filament_yes_btn;
  lv_obj_t *filament_no_btn;

  // lista (quadro 02)
  lv_obj_t *tabs = NULL;
  lv_obj_t *sort_btn = NULL;
  lv_obj_t *page_up_btn = NULL;
  lv_obj_t *page_down_btn = NULL;
  lv_obj_t *page_label = NULL;
  Card cards[3];

  Tree root;
  Tree *cur_dir;
  FilePanel file_panel;
  PrintStatusPanel &print_status;
  uint32_t sort_mode;
  bool sort_reversed;

  int tab = TAB_LOCAL;
  int page = 0;
  std::vector<Entry> entries;
  std::string empty_text;
  std::vector<HistoryJob> history_jobs;
  bool history_loading = false;
  std::string requested_file;      // abrir_arquivo esperando a lista chegar
  bool files_loaded = false;       // a lista já chegou do Moonraker ao menos uma vez

  bool spoolman_enabled = false;
  bool refreshing_files = false;   // an RPC is in-flight
  bool refresh_pending = false;    // another notify arrived while in-flight
  std::string pending_print_path;
  // filament required by the pending print (from gcode metadata); <0 = unknown
  double pending_needed_g = -1;
  double pending_needed_mm = -1;

};

#endif // __PRINT_PANEL_H__
