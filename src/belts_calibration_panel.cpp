#include "belts_calibration_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(resume);
LV_IMG_DECLARE(inputshaper_img);

#define BELTS_PNG "belts_calibration.png"

// Zoom do gráfico: no cartão (408 x 108) e ampliado no corpo da subtela
// (408 x 212). O PNG é gerado com o tamanho da tela (480 x 272).
#define ZOOM_PEQUENO 100
#define ZOOM_AMPLIADO 196

std::vector<std::string> BeltsCalibrationPanel::axes = {
  "x",
  "y",
  "a",
  "b"
};

// Girador (lv_spinner) só gira enquanto mede: o lv_spinner do LVGL anima para
// sempre, mesmo escondido. Aqui as animações dele são apagadas ao esconder e
// uma rotação simples é ligada ao mostrar. Roda sempre com o lv_lock preso.
static void girador_passo(void *obj, int32_t v) {
  lv_arc_set_rotation((lv_obj_t *)obj, (uint16_t)v);
}

static void girador(lv_obj_t *sp, bool ligar) {
  lv_anim_del(sp, NULL);
  if (!ligar) {
    lv_obj_add_flag(sp, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_arc_set_angles(sp, 0, 60);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, sp);
  lv_anim_set_exec_cb(&a, girador_passo);
  lv_anim_set_values(&a, 0, 360);
  lv_anim_set_time(&a, 1000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
  lv_obj_clear_flag(sp, LV_OBJ_FLAG_HIDDEN);
}

BeltsCalibrationPanel::BeltsCalibrationPanel(KWebSocketClient &c, std::mutex &l)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , corpo(lv_obj_create(cont))
  , graph_cont(lv_obj_create(corpo))
  , graph(lv_img_create(graph_cont))
  , spinner(lv_spinner_create(corpo, 1000, 60))
  , excite_control(lv_obj_create(corpo))
  , excite_slider(lv_slider_create(excite_control))
  , excite_label(lv_label_create(excite_control))
  , excite_dd(lv_dropdown_create(excite_control))
  , button_cont(lv_obj_create(corpo))
  , calibrate_btn(button_cont, &resume, "Testar correias", &BeltsCalibrationPanel::_handle_callback, this)
  , excite_btn(button_cont, &inputshaper_img, "Vibrar", &BeltsCalibrationPanel::_handle_callback, this)
  , image_fullsized(false)
{
  // Subtela (Proposta B): cabeçalho com voltar (fecha a subtela) e o conteúdo
  // num "corpo" em (0,44) 424 x 228, margem 8, grade de uma coluna:
  //   linha 0 (108): cartão do gráfico (tocar amplia) / girador enquanto mede
  //   linha 1 (44): frequência de vibração (régua + Hz) e eixo
  //   linha 2 (44): "Testar correias" (principal) e "Vibrar"
  // "Parar tudo" saiu: a parada é o botão vermelho da coluna (sempre visível).
  Navegacao::subtela(cont);
  Marca::criar_cabecalho(cont, "Calibração de correias");

  lv_obj_set_pos(corpo, 0, Marca::TOQUE);
  lv_obj_set_size(corpo, Marca::CONT_L, Marca::TELA_A - Marca::TOQUE);
  lv_obj_set_style_pad_all(corpo, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(corpo, Marca::VAO, 0);
  lv_obj_set_style_pad_column(corpo, Marca::VAO, 0);
  lv_obj_clear_flag(corpo, LV_OBJ_FLAG_SCROLLABLE);

  Marca::cartao(graph_cont);
  lv_obj_add_flag(graph_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(graph_cont, &BeltsCalibrationPanel::_handle_image_clicked,
    LV_EVENT_CLICKED, this);

  lv_img_set_zoom(graph, ZOOM_PEQUENO);
  // lv_img_set_src(graph, "A:/home/balla/Downloads/belts_calibration.png");
  lv_obj_center(graph);

  // girador escondido e parado até medir
  lv_obj_set_size(spinner, 64, 64);
  girador(spinner, false);

  // excite controls (408 x 44, posições fixas):
  //   "Frequência de vibração" 12 G em (0,14) largura 150 (143 px na 12)
  //   régua em (168,9) largura 142, valor em Hz 12 G centralizado embaixo
  //   lista de eixos 72 x 44 encostada à direita
  lv_obj_set_style_pad_all(excite_control, 0, 0);
  lv_obj_clear_flag(excite_control, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(excite_control);
  lv_label_set_text(label, "Frequência de vibração");
  lv_obj_set_style_text_font(label, Marca::f12(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(Marca::GRAFITE), 0);
  lv_obj_set_pos(label, 0, 14);
  lv_obj_set_width(label, 150);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);

  lv_obj_set_pos(excite_slider, 168, 9);
  lv_obj_set_width(excite_slider, 142);
  lv_slider_set_range(excite_slider, 10, 1400);

  lv_obj_add_event_cb(excite_slider, &BeltsCalibrationPanel::_handle_update_slider,
    LV_EVENT_VALUE_CHANGED, this);

  lv_obj_set_style_text_font(excite_label, Marca::f12(), 0);
  lv_obj_set_style_text_align(excite_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(excite_label, 158, 27);
  lv_obj_set_width(excite_label, 162);
  lv_label_set_text(excite_label, "1 Hz");

  lv_dropdown_set_options(excite_dd, fmt::format("{}", fmt::join(axes, "\n")).c_str());
  lv_obj_set_size(excite_dd, 72, Marca::TOQUE);
  lv_obj_align(excite_dd, LV_ALIGN_TOP_RIGHT, 0, 0);

  // botões: só texto, 200 x 44 cada
  lv_obj_set_flex_flow(button_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(button_cont, 0, 0);
  lv_obj_set_style_pad_column(button_cont, Marca::VAO, 0);
  lv_obj_clear_flag(button_cont, LV_OBJ_FLAG_SCROLLABLE);

  calibrate_btn.sem_icone();
  lv_obj_set_size(calibrate_btn.get_container(), 200, Marca::TOQUE);
  calibrate_btn.set_tipo(Marca::Tipo::DESTAQUE);
  excite_btn.sem_icone();
  lv_obj_set_size(excite_btn.get_container(), 200, Marca::TOQUE);

  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), Marca::TOQUE, Marca::TOQUE, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(corpo, grid_main_col_dsc, grid_main_row_dsc);

  lv_obj_set_grid_cell(graph_cont, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_grid_cell(spinner, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(excite_control, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);

  lv_obj_set_grid_cell(button_cont, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

  ws.register_method_callback("notify_gcode_response",
    "BeltsCalibrationPanel",
    [this](json &d) { this->handle_macro_response(d); });

}

BeltsCalibrationPanel::~BeltsCalibrationPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void BeltsCalibrationPanel::foreground() {
  Navegacao::abrir(cont);
}

void BeltsCalibrationPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == calibrate_btn.get_container()) {
    auto config_root = KUtils::get_root_path("config");
    auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp", BELTS_PNG);

    if (!KUtils::is_homed()) {
      ws.gcode_script("G28");
    }

    auto screen_width = (double)lv_disp_get_physical_hor_res(NULL) / 100.0;
    auto screen_height = (double)lv_disp_get_physical_ver_res(NULL) / 100.0;
    ws.gcode_script(fmt::format("GUPPY_BELTS_SHAPER_CALIBRATION PNG_OUT_PATH={} PNG_WIDTH={} PNG_HEIGHT={}",
      png_path, screen_width, screen_height));

    // ws.gcode_script(fmt::format("GUPPY_BELTS_SHAPER_CALIBRATION PNG_OUT_PATH={} PNG_WIDTH={} PNG_HEIGHT={} FREQ_START=5 FREQ_END=10",
    // 				png_path, screen_width, screen_height));


    lv_obj_add_flag(graph, LV_OBJ_FLAG_HIDDEN);
    lv_img_set_src(graph, NULL);
    lv_obj_invalidate(graph);
    girador(spinner, true);
    lv_obj_move_foreground(spinner);

  } else if (btn == excite_btn.get_container()) {
    double excite_hz = (double)lv_slider_get_value(excite_slider) / 10.0 + 1; // [x-1, x+1]
    char excite_buf[10];
    lv_dropdown_get_selected_str(excite_dd, excite_buf, sizeof(excite_buf));

    if (!KUtils::is_homed()) {
      ws.gcode_script("G28");
    }
    ws.gcode_script(fmt::format("GUPPY_EXCITATE_AXIS_AT_FREQ FREQUENCY={} AXIS={}", excite_hz, excite_buf));
  }
  // "Voltar" é o do cabeçalho; a parada de emergência é o botão vermelho da coluna.
}

void BeltsCalibrationPanel::handle_image_clicked(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *clicked = lv_event_get_target(e);

    if (clicked == graph_cont) {
      if (image_fullsized) {
        lv_img_set_zoom(graph, ZOOM_PEQUENO);
        lv_obj_clear_flag(graph_cont, LV_OBJ_FLAG_FLOATING);  // a grade volta a esticar na célula

      } else {
        // solto da grade, cobre o corpo (408 x 212); o cabeçalho continua à vista
        lv_img_set_zoom(graph, ZOOM_AMPLIADO);
        lv_obj_add_flag(graph_cont, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_pos(graph_cont, 0, 0);
        lv_obj_set_size(graph_cont, LV_PCT(100), LV_PCT(100));
      }
      lv_obj_move_foreground(graph_cont);
      image_fullsized = !image_fullsized;
    }
  }
}

void BeltsCalibrationPanel::handle_macro_response(json &j) {
  spdlog::trace("belts calbiration macro response: {}", j.dump());
  auto &v = j["/params/0"_json_pointer];
  if (!v.is_null()) {
    std::string resp = v.template get<std::string>();
    std::lock_guard<std::mutex> lock(lv_lock);
    if (resp.rfind("// Command {guppy_belts_calibration} finished", 0) == 0) {
      spdlog::trace("belts calbiration finished");
      auto config_root = KUtils::get_root_path("config");
      auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp", BELTS_PNG);

      png_path =
        fmt::format("A:{}", KUtils::is_running_local()
          ? png_path
          : KUtils::download_file("config", BELTS_PNG, Config::get_instance()->get_thumbnail_path()));

      lv_img_set_src(graph, png_path.c_str());
      lv_obj_clear_flag(graph, LV_OBJ_FLAG_HIDDEN);
      girador(spinner, false);
      lv_obj_move_background(spinner);
    }
  }
}

void BeltsCalibrationPanel::handle_update_slider(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    double hz = (double)lv_slider_get_value(excite_slider);
    lv_slider_set_value(excite_slider, hz, LV_ANIM_OFF);
    lv_label_set_text(excite_label, fmt::format("{} Hz", hz / 10.0).c_str());
  }
}
