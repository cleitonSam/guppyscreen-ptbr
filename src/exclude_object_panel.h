#ifndef __EXCLUDE_OBJECT_PANEL_H__
#define __EXCLUDE_OBJECT_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "button_container.h"
#include "lvgl/lvgl.h"

#include <string>
#include <vector>

// Top-down bed map of the current print's objects (Klipper exclude_object).
// Tap an object to exclude it; the currently-printing one is highlighted and
// already-excluded ones are crossed out. Subtela "Objetos" (Proposta B), aberta
// pelo botão "Objetos" do Ajuste fino só quando a impressão tem objetos.
class ExcludeObjectPanel : public NotifyConsumer {
 public:
  ExcludeObjectPanel(KWebSocketClient &ws, std::mutex &l);
  ~ExcludeObjectPanel();

  void foreground();
  void consume(json &j);
  void handle_callback(lv_event_t *e);
  void handle_canvas_click(lv_event_t *e);

  static void _handle_callback(lv_event_t *e) {
    static_cast<ExcludeObjectPanel *>(e->user_data)->handle_callback(e);
  }
  static void _handle_canvas_click(lv_event_t *e) {
    static_cast<ExcludeObjectPanel *>(e->user_data)->handle_canvas_click(e);
  }

 private:
  // One drawn object: bounding box, polygon (for point_in_polygon hit-test), and state.
  struct ObjBox {
    std::string name;
    lv_coord_t x0, y0, x1, y1;
    bool excluded;
    std::vector<lv_point_t> polygon;
  };

  KWebSocketClient &ws;
  lv_obj_t *panel_cont;
  lv_obj_t *canvas;
  lv_color_t *canvas_buf;
  lv_obj_t *back_btn;         // voltar do cabeçalho
  lv_obj_t *count_label;      // direita do cabeçalho: "Objetos: 3, excluídos: 1"
  lv_obj_t *legend_cont;      // legenda (3 linhas: quadradinho + palavra)
  lv_obj_t *status_label;     // aviso quando não há objetos

  bool is_foreground = false;
  bool cancel_on_confirm = false;    // true when tapped object is the last one
  std::string pending_name;          // object awaiting exclude confirmation
  lv_obj_t *confirm_mbox = nullptr;  // open confirm dialog, or null — guards double-taps

  // Bed bounds (mm) the current draw was mapped with; reused by the hit-test.
  double bed_min_x = 0.0, bed_max_x = 220.0;
  double bed_min_y = 0.0, bed_max_y = 220.0;

  std::vector<ObjBox> obj_boxes;

  void load_bed_bounds();
  void redraw();
  lv_point_t to_px(double mx, double my);
  void confirm_exclude(const std::string &name);
  void do_exclude();
  bool visivel();             // subtela na frente de tudo?
};

#endif // __EXCLUDE_OBJECT_PANEL_H__
