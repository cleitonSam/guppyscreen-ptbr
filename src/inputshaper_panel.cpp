#include "inputshaper_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <algorithm>

LV_IMG_DECLARE(resume);
LV_IMG_DECLARE(sd_img);

LV_FONT_DECLARE(dejavusans_mono_14);

#define X_DATA "/tmp/resonances_x_x.csv"
#define X_PNG "resonances_x.png"
#define Y_DATA "/tmp/resonances_y_y.csv"
#define Y_PNG "resonances_y.png"

// Zoom do gráfico: pequeno no cartão (156 x 108) e "ampliado" ocupando o
// corpo da subtela (408 x 212). O PNG é gerado com o tamanho da tela (480 x 272).
#define ZOOM_PEQUENO 80
#define ZOOM_AMPLIADO 186

std::vector<std::string> InputShaperPanel::shapers = {
  "zv",
  "mzv",
  "ei",
  "2hump_ei",
  "3hump_ei"
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

InputShaperPanel::InputShaperPanel(KWebSocketClient &c, std::mutex &l)
  : ws(c)
  , lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , corpo(lv_obj_create(cont))

  // xgraph
  , xgraph_cont(lv_obj_create(corpo))
  , xgraph(lv_img_create(xgraph_cont))
  , xoutput(lv_label_create(corpo))
  , xspinner(lv_spinner_create(corpo, 1000, 60))

  // ygraph
  , ygraph_cont(lv_obj_create(corpo))
  , ygraph(lv_img_create(ygraph_cont))
  , youtput(lv_label_create(corpo))
  , yspinner(lv_spinner_create(corpo, 1000, 60))

  // x controls
  , xcontrol(lv_obj_create(corpo))
  , xaxis_label(lv_label_create(xcontrol))
  , x_switch(lv_switch_create(xcontrol))
  , xslider_cont(lv_obj_create(xcontrol))
  , xslider(lv_slider_create(xslider_cont))
  , xlabel(lv_label_create(xslider_cont))
  , xshaper_dd(lv_dropdown_create(xcontrol))

  // y controls
  , ycontrol(lv_obj_create(corpo))
  , yaxis_label(lv_label_create(ycontrol))
  , y_switch(lv_switch_create(ycontrol))
  , yslider_cont(lv_obj_create(ycontrol))
  , yslider(lv_slider_create(yslider_cont))
  , ylabel(lv_label_create(yslider_cont))
  , yshaper_dd(lv_dropdown_create(ycontrol))

  , button_cont(lv_obj_create(corpo))
  , switch_cont(lv_obj_create(button_cont))
  , graph_switch_label(lv_label_create(switch_cont))
  , graph_switch(lv_switch_create(switch_cont))
  , calibrate_btn(button_cont, &resume, "Calibrar", &InputShaperPanel::_handle_callback, this)
  , save_btn(button_cont, &sd_img, "Salvar", &InputShaperPanel::_handle_callback, this)
  , ximage_fullsized(false)
  , yimage_fullsized(false)
  , x_pending(false)
  , y_pending(false)
  , y_after_move(false)
  , next_axis_is_x(false)
  , cal_watchdog(NULL)
{
  // Subtela (Proposta B): cabeçalho com voltar (fecha a subtela) e o conteúdo
  // num "corpo" em (0,44) 424 x 228, com margem 8 e grade:
  //   coluna 0 (80): Calibrar, Salvar, Gráfico
  //   colunas 1 e 2 (156 cada): resultado X | resultado Y (108 de altura)
  //   linhas 1 e 2 (44 cada): controles do eixo X e do eixo Y
  // A parada de emergência fica no botão vermelho da coluna (sempre visível).
  Navegacao::subtela(cont);
  Marca::Cabecalho cab = Marca::criar_cabecalho(cont, "Compensação de vibração");
  lv_obj_set_width(cab.titulo, 366);  // 269 px na f20; o lado direito não é usado

  lv_obj_set_pos(corpo, 0, Marca::TOQUE);
  lv_obj_set_size(corpo, Marca::CONT_L, Marca::TELA_A - Marca::TOQUE);
  lv_obj_set_style_pad_all(corpo, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(corpo, Marca::VAO, 0);
  lv_obj_set_style_pad_column(corpo, Marca::VAO, 0);
  lv_obj_clear_flag(corpo, LV_OBJ_FLAG_SCROLLABLE);

  // cartões dos gráficos (PRETO_2, raio 12); tocar amplia/reduz
  lv_obj_t *graph_label = lv_label_create(xgraph_cont);
  lv_label_set_text(graph_label, "Resposta em frequência X");
  lv_obj_set_style_text_font(graph_label, &lv_font_montserrat_10, 0);  // 160 px na 12: não cabe em 156
  lv_obj_set_style_text_color(graph_label, lv_color_hex(Marca::GRAFITE), 0);
  lv_obj_align(graph_label, LV_ALIGN_BOTTOM_MID, 0, -4);

  Marca::cartao(xgraph_cont);
  lv_obj_add_flag(xgraph_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(xgraph_cont, &InputShaperPanel::_handle_image_clicked,
    LV_EVENT_CLICKED, this);

  graph_label = lv_label_create(ygraph_cont);
  lv_label_set_text(graph_label, "Resposta em frequência Y");
  lv_obj_set_style_text_font(graph_label, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(graph_label, lv_color_hex(Marca::GRAFITE), 0);
  lv_obj_align(graph_label, LV_ALIGN_BOTTOM_MID, 0, -4);

  Marca::cartao(ygraph_cont);
  lv_obj_add_flag(ygraph_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ygraph_cont, &InputShaperPanel::_handle_image_clicked,
    LV_EVENT_CLICKED, this);

  // graphs
  lv_img_set_zoom(xgraph, ZOOM_PEQUENO);
  lv_obj_align(xgraph, LV_ALIGN_CENTER, 0, -6);  // acima da legenda
  // lv_img_set_src(xgraph, "A:/usr/data/printer_data/thumbnails/resonances_x.png");

  lv_img_set_zoom(ygraph, ZOOM_PEQUENO);
  lv_obj_align(ygraph, LV_ALIGN_CENTER, 0, -6);
  // lv_img_set_src(ygraph, "A:/usr/data/printer_data/thumbnails/resonances_y.png");

  // text output (10: a linha "2hump_ei  54.2 Hz  vib. 3.4%" mede 170 px na 12)
  lv_label_set_text(xoutput, "");
  lv_obj_set_style_text_font(xoutput, &lv_font_montserrat_10, LV_STATE_DEFAULT);

  lv_label_set_text(youtput, "");
  lv_obj_set_style_text_font(youtput, &lv_font_montserrat_10, LV_STATE_DEFAULT);

  // spinners: escondidos e parados até medir
  lv_obj_set_size(xspinner, 64, 64);
  girador(xspinner, false);

  lv_obj_set_size(yspinner, 64, 64);
  girador(yspinner, false);

  // buttons (coluna da esquerda, 80 de largura): só texto, 44 de altura
  lv_obj_set_size(button_cont, 80, LV_PCT(100));
  lv_obj_clear_flag(button_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(button_cont, 0, 0);
  lv_obj_set_style_pad_row(button_cont, Marca::VAO, 0);
  lv_obj_set_flex_flow(button_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(button_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  calibrate_btn.sem_icone();
  lv_obj_set_size(calibrate_btn.get_container(), 80, Marca::TOQUE);
  calibrate_btn.set_tipo(Marca::Tipo::DESTAQUE);
  save_btn.sem_icone();
  lv_obj_set_size(save_btn.get_container(), 80, Marca::TOQUE);

  // "Gráfico" (12 G) com a chave embaixo, depois dos botões
  lv_obj_move_foreground(switch_cont);
  lv_obj_set_size(switch_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(switch_cont, 0, 0);
  lv_obj_set_style_pad_top(switch_cont, 4, 0);
  lv_obj_set_style_pad_bottom(switch_cont, 10, 0);   // a área de toque da chave cabe dentro
  lv_obj_set_style_pad_row(switch_cont, 4, 0);
  lv_obj_set_flex_flow(switch_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(switch_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_set_style_text_align(graph_switch_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(graph_switch_label, "Gráfico");
  lv_obj_set_width(graph_switch_label, LV_PCT(100));
  lv_obj_set_style_text_font(graph_switch_label, Marca::f12(), 0);
  lv_obj_set_style_text_color(graph_switch_label, lv_color_hex(Marca::GRAFITE), 0);
  lv_obj_set_size(graph_switch, 44, 24);
  lv_obj_set_ext_click_area(graph_switch, 10);   // toque de 64 x 44
  lv_obj_clear_state(graph_switch, LV_STATE_CHECKED);
  lv_obj_clear_flag(switch_cont, LV_OBJ_FLAG_SCROLLABLE);

  // controls: uma linha de 44 por eixo (320 de largura):
  //   "Eixo X" 12 G | chave 44 x 24 | régua (cresce) com o valor em Hz embaixo | lista 88 x 44
  lv_obj_t *linhas[] = {xcontrol, ycontrol};
  lv_obj_t *rotulos[] = {xaxis_label, yaxis_label};
  lv_obj_t *chaves[] = {x_switch, y_switch};
  lv_obj_t *reguas_cont[] = {xslider_cont, yslider_cont};
  lv_obj_t *reguas[] = {xslider, yslider};
  lv_obj_t *valores[] = {xlabel, ylabel};
  lv_obj_t *listas[] = {xshaper_dd, yshaper_dd};
  const char *nomes[] = {"Eixo X", "Eixo Y"};
  for (int i = 0; i < 2; i++) {
    lv_obj_set_flex_flow(linhas[i], LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(linhas[i], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(linhas[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(linhas[i], 0, 0);
    lv_obj_set_style_pad_column(linhas[i], 6, 0);

    lv_label_set_text(rotulos[i], nomes[i]);
    lv_obj_set_width(rotulos[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(rotulos[i], Marca::f12(), 0);
    lv_obj_set_style_text_color(rotulos[i], lv_color_hex(Marca::GRAFITE), 0);

    lv_obj_set_size(chaves[i], 44, 24);
    lv_obj_add_state(chaves[i], LV_STATE_CHECKED);

    lv_obj_clear_flag(reguas_cont[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(reguas_cont[i], Marca::TOQUE);
    lv_obj_set_flex_grow(reguas_cont[i], 1);
    lv_obj_set_style_pad_all(reguas_cont[i], 0, 0);
    lv_obj_set_style_pad_hor(reguas_cont[i], 10, 0);  // espaço para o botão da régua

    lv_obj_set_width(reguas[i], LV_PCT(100));
    lv_obj_align(reguas[i], LV_ALIGN_TOP_MID, 0, 9);
    lv_slider_set_range(reguas[i], 0, 1400);
    lv_obj_add_event_cb(reguas[i], &InputShaperPanel::_handle_update_slider,
      LV_EVENT_VALUE_CHANGED, this);

    lv_obj_set_style_text_font(valores[i], Marca::f12(), 0);
    lv_obj_align(valores[i], LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_label_set_text(valores[i], "0 Hz");

    lv_dropdown_set_options(listas[i], fmt::format("{}", fmt::join(shapers, "\n")).c_str());
    lv_obj_set_size(listas[i], 88, Marca::TOQUE);
    lv_obj_set_style_text_font(listas[i], Marca::f12(), 0);
  }

  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), Marca::TOQUE, Marca::TOQUE,
    LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {80, LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(corpo, grid_main_col_dsc, grid_main_row_dsc);

  // row 1, col 2/3 (cartão/texto ocupam a célula toda; girador no meio)
  lv_obj_set_grid_cell(xspinner, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(yspinner, LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(xgraph_cont, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_grid_cell(ygraph_cont, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_grid_cell(xoutput, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_grid_cell(youtput, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 0, 1);

  // row 2, col 2 span 2
  lv_obj_set_grid_cell(xcontrol, LV_GRID_ALIGN_STRETCH, 1, 2, LV_GRID_ALIGN_STRETCH, 1, 1);

  // row 3, col 2 span 2
  lv_obj_set_grid_cell(ycontrol, LV_GRID_ALIGN_STRETCH, 1, 2, LV_GRID_ALIGN_STRETCH, 2, 1);

  // col 1, todas as linhas
  lv_obj_set_grid_cell(button_cont, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 3);

  // TODO: show only register when issuing macros inputshaper cares about, then unregister after.
  // ws.register_gcode_resp([this](json& d) { this->handle_macro_response(d); });
  ws.register_method_callback("notify_gcode_response",
    "InputShaperPanel",
    [this](json &d) { this->handle_macro_response(d); });
}

InputShaperPanel::~InputShaperPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void InputShaperPanel::foreground() {
  auto inputshaper = State::get_instance()
    ->get_data("/printer_state/configfile/config/input_shaper"_json_pointer);
  spdlog::trace("input shapper {}", inputshaper.dump());

  if (!inputshaper.is_null()) {
    auto v = inputshaper["/shaper_freq_x"_json_pointer];
    if (!v.is_null()) {
      double hz = std::stod(v.template get<std::string>());
      lv_slider_set_value(xslider, hz * 10, LV_ANIM_OFF);
      lv_label_set_text(xlabel, fmt::format("{} Hz", hz).c_str());
    }

    v = inputshaper["/shaper_type_x"_json_pointer];
    if (!v.is_null()) {
      auto shaper = v.template get<std::string>();
      auto idx = find_shaper_index(shapers, shaper);
      lv_dropdown_set_selected(xshaper_dd, idx);
    }

    v = inputshaper["/shaper_freq_y"_json_pointer];
    if (!v.is_null()) {
      double hz = std::stod(v.template get<std::string>());
      lv_slider_set_value(yslider, hz * 10, LV_ANIM_OFF);
      lv_label_set_text(ylabel, fmt::format("{} Hz", hz).c_str());
    }

    v = inputshaper["/shaper_type_y"_json_pointer];
    if (!v.is_null()) {
      auto shaper = v.template get<std::string>();
      auto idx = find_shaper_index(shapers, shaper);
      lv_dropdown_set_selected(yshaper_dd, idx);
    }
  }

  Navegacao::abrir(cont);
}


void InputShaperPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == calibrate_btn.get_container()) {
    bool x_requested = lv_obj_has_state(x_switch, LV_STATE_CHECKED);
    bool y_requested = lv_obj_has_state(y_switch, LV_STATE_CHECKED);

    if (!x_requested && !y_requested) {
      KUtils::notify_toast("Escolha o eixo X, o Y ou os dois e toque em Calibrar.", 2500);
      return;
    }

    // Axes run ONE AT A TIME so the accelerometer can be moved between them.
    // Confirm placement before each axis; both -> Y first, then prompt for X.
    y_after_move = (x_requested && y_requested);
    // lay out the result panes for this run (graph + console for a single axis)
    layout_panes(lv_obj_has_state(graph_switch, LV_STATE_CHECKED), x_requested, y_requested);
    // X is shown on top in the GUI, so run X first when selected; Y second.
    show_placement_prompt(/*is_x=*/x_requested, /*moving=*/false);

  } else if (btn == save_btn.get_container()) {
    double xhz = (double)lv_slider_get_value(xslider) / 10.0;
    double yhz = (double)lv_slider_get_value(yslider) / 10.0;

    char xbuf[10];
    lv_dropdown_get_selected_str(xshaper_dd, xbuf, sizeof(xbuf));

    char ybuf[10];
    lv_dropdown_get_selected_str(yshaper_dd, ybuf, sizeof(ybuf));

    ws.gcode_script(fmt::format("SAVE_INPUT_SHAPER SHAPER_FREQ_X={} SHAPER_TYPE_X={} SHAPER_FREQ_Y={} SHAPER_TYPE_Y={}\nSAVE_CONFIG",
      xhz, xbuf, yhz, ybuf));

    KUtils::notify_toast(
      fmt::format("Salvo: X {} em {:.1f} Hz e Y {} em {:.1f} Hz. O Klipper vai reiniciar para aplicar. "
        "Se aparecer 'shutdown', reinicie o Klipper de novo.", xbuf, xhz, ybuf, yhz),
      6000);
  }
  // "Voltar" é o do cabeçalho; a parada de emergência é o botão vermelho da coluna.
}

void InputShaperPanel::handle_macro_response(json &j) {
  spdlog::trace("inputshapping macro response: {}", j.dump());
  auto &v = j["/params/0"_json_pointer];
  if (!v.is_null()) {
    std::string resp = v.template get<std::string>();
    std::lock_guard<std::mutex> lock(lv_lock);
    bool graph_requested = lv_obj_has_state(graph_switch, LV_STATE_CHECKED);
    if (resp.rfind("// {\"shapers\":", 0) == 0) {
      auto res = json::parse(resp.substr(3));
      auto &axis_log = res["/logfile"_json_pointer];
      auto &axis_png = res["/png"_json_pointer];

      if (!axis_log.is_null()) {
        std::string fn = axis_log.template get<std::string>();
        if (X_DATA == fn) {
          if (graph_requested && !axis_png.is_null()) {
            spdlog::trace("found generated x freq png");
            auto config_root = KUtils::get_root_path("config");
            auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp", X_PNG);

            png_path =
              fmt::format("A:{}", KUtils::is_running_local()
                ? png_path
                : KUtils::download_file("config", X_PNG, Config::get_instance()->get_thumbnail_path()));

            spdlog::trace("x freq png path {}", png_path);

            // single-axis run -> show the readable console beside the graph (col2)
            bool only_x = !lv_obj_has_state(y_switch, LV_STATE_CHECKED);
            lv_img_set_src(xgraph, png_path.c_str());
            lv_obj_clear_flag(xgraph_cont, LV_OBJ_FLAG_HIDDEN);
            if (only_x) {
              set_shaper_detail(res, xoutput, xslider, xlabel, xshaper_dd);
              lv_obj_move_foreground(xoutput);
            } else {
              lv_label_set_text(xoutput, "");
              set_shaper_detail(res, NULL, xslider, xlabel, xshaper_dd);
            }
            lv_obj_move_foreground(xgraph_cont);
          } else {
            set_shaper_detail(res, xoutput, xslider, xlabel, xshaper_dd);
            lv_obj_move_foreground(xoutput);
          }

          girador(xspinner, false);
          lv_obj_move_background(xspinner);
          {
            auto &b = res["/best"_json_pointer];
            if (b.is_string()) {
              auto bn = b.template get<std::string>();
              auto &fq = res["shapers"][bn]["freq"];
              if (fq.is_number()) {
                KUtils::notify_toast(fmt::format("Eixo X pronto: recomendado {} em {:.1f} Hz. Toque em Salvar para aplicar.",
                  bn, fq.template get<double>()), 4000);
              }
            }
          }
          x_pending = false;   // X is finished regardless of what's next
          if (y_after_move) {
            // both axes: pause the watchdog and prompt to move the sensor for Y
            if (cal_watchdog != NULL) {
              lv_timer_del(cal_watchdog);
              cal_watchdog = NULL;
            }
            show_placement_prompt(/*is_x=*/false, /*moving=*/true);
          } else if (!x_pending && !y_pending) {
            end_calibration_ui();
          }

        } else if (Y_DATA == fn) {
          if (graph_requested && !axis_png.is_null()) {
            spdlog::trace("found generated y freq png");
            auto config_root = KUtils::get_root_path("config");
            auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp", Y_PNG);

            png_path =
              fmt::format("A:{}", KUtils::is_running_local()
                ? png_path
                : KUtils::download_file("config", Y_PNG, Config::get_instance()->get_thumbnail_path()));

            spdlog::trace("y freq png path {}", png_path);

            // single-axis run -> show the readable console beside the graph (col2)
            bool only_y = !lv_obj_has_state(x_switch, LV_STATE_CHECKED);
            lv_img_set_src(ygraph, png_path.c_str());
            lv_obj_clear_flag(ygraph_cont, LV_OBJ_FLAG_HIDDEN);
            if (only_y) {
              set_shaper_detail(res, youtput, yslider, ylabel, yshaper_dd);
              lv_obj_move_foreground(youtput);
            } else {
              lv_label_set_text(youtput, "");
              set_shaper_detail(res, NULL, yslider, ylabel, yshaper_dd);
            }
            lv_obj_move_foreground(ygraph_cont);
          } else {
            set_shaper_detail(res, youtput, yslider, ylabel, yshaper_dd);
            lv_obj_move_foreground(youtput);
          }

          girador(yspinner, false);
          lv_obj_move_background(yspinner);
          {
            auto &b = res["/best"_json_pointer];
            if (b.is_string()) {
              auto bn = b.template get<std::string>();
              auto &fq = res["shapers"][bn]["freq"];
              if (fq.is_number()) {
                KUtils::notify_toast(fmt::format("Eixo Y pronto: recomendado {} em {:.1f} Hz. Toque em Salvar para aplicar.",
                  bn, fq.template get<double>()), 4000);
              }
            }
          }
          y_pending = false;   // Y is the last axis when both are run
          if (!x_pending && !y_pending) {
            end_calibration_ui();
          }
        }
      }

    } else if ("// Resonances data written to " Y_DATA " file" == resp) {
      KUtils::notify_toast(graph_requested ? "Analisando o eixo Y e gerando o gráfico (mais lento)..." : "Analisando o eixo Y...", 3000);
      auto config_root = KUtils::get_root_path("config");
      auto screen_width = (double)lv_disp_get_physical_hor_res(NULL) / 100.0;
      auto screen_height = (double)lv_disp_get_physical_ver_res(NULL) / 100.0;
      auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp", Y_PNG);
      std::string arg = graph_requested
        ? fmt::format("{} -o {} -w {} -l {}", Y_DATA, png_path, screen_width, screen_height)
        : Y_DATA;

      ws.gcode_script(fmt::format("RUN_SHELL_COMMAND CMD=guppy_input_shaper PARAMS={:?}", arg));

    } else if ("// Resonances data written to " X_DATA " file" == resp) {
      KUtils::notify_toast(graph_requested ? "Analisando o eixo X e gerando o gráfico (mais lento)..." : "Analisando o eixo X...", 3000);
      auto config_root = KUtils::get_root_path("config");
      auto screen_width = (double)lv_disp_get_physical_hor_res(NULL) / 100.0;
      auto screen_height = (double)lv_disp_get_physical_ver_res(NULL) / 100.0;
      auto png_path = fmt::format("{}/{}", config_root.length() > 0 ? config_root : "/tmp", X_PNG);
      std::string arg = graph_requested
        ? fmt::format("{} -o {} -w {} -l {}", X_DATA, png_path, screen_width, screen_height)
        : X_DATA;

      ws.gcode_script(fmt::format("RUN_SHELL_COMMAND CMD=guppy_input_shaper PARAMS={:?}", arg));
    }
  }
}

// Cartão do gráfico: pequeno (preso na célula da grade, zoom pequeno) ou
// ampliado (solto da grade, cobre o corpo da subtela 408 x 212; o cabeçalho
// com o voltar continua à vista). Tocar de novo reduz.
static void cartao_grafico(lv_obj_t *graph_cont, lv_obj_t *graph, bool ampliado) {
  lv_obj_invalidate(graph);
  if (ampliado) {
    lv_img_set_zoom(graph, ZOOM_AMPLIADO);
    lv_obj_add_flag(graph_cont, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(graph_cont, 0, 0);
    lv_obj_set_size(graph_cont, LV_PCT(100), LV_PCT(100));
  } else {
    lv_img_set_zoom(graph, ZOOM_PEQUENO);
    lv_obj_clear_flag(graph_cont, LV_OBJ_FLAG_FLOATING);  // a grade volta a esticar na célula
  }
}

void InputShaperPanel::handle_image_clicked(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *clicked = lv_event_get_target(e);

    if (clicked == xgraph_cont) {
      if (yimage_fullsized) {
        cartao_grafico(ygraph_cont, ygraph, false);
        yimage_fullsized = false;
      }

      cartao_grafico(xgraph_cont, xgraph, !ximage_fullsized);
      lv_obj_move_foreground(xgraph_cont);
      ximage_fullsized = !ximage_fullsized;

    } else if (clicked == ygraph_cont) {
      if (ximage_fullsized) {
        cartao_grafico(xgraph_cont, xgraph, false);
        ximage_fullsized = false;
      }

      cartao_grafico(ygraph_cont, ygraph, !yimage_fullsized);
      lv_obj_move_foreground(ygraph_cont);
      yimage_fullsized = !yimage_fullsized;
    }
  }
}

void InputShaperPanel::handle_update_slider(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *obj = lv_event_get_target(e);
    double hz = (double)lv_slider_get_value(obj);
    lv_slider_set_value(obj, hz, LV_ANIM_OFF);
    if (obj == xslider) {
      lv_label_set_text(xlabel, fmt::format("{} Hz", hz / 10.0).c_str());
    } else if (obj == yslider) {
      lv_label_set_text(ylabel, fmt::format("{} Hz", hz / 10.0).c_str());
    }
  }
}

uint32_t InputShaperPanel::find_shaper_index(const std::vector<std::string> &s,
  const std::string &shaper) {
  return std::distance(s.cbegin(), std::find(s.cbegin(), s.cend(), shaper));
}

void InputShaperPanel::set_shaper_detail(json &res,
  lv_obj_t *label,
  lv_obj_t *slider,
  lv_obj_t *slider_label,
  lv_obj_t *dd) {
  auto &shapers_resp = res["/shapers"_json_pointer];
  if (!shapers_resp.is_null()) {
    std::vector<std::string> shaper_details;
    auto &best_shaper = res["/best"_json_pointer];
    if (!best_shaper.is_null()) {
      auto bs_name = best_shaper.template get<std::string>();
      double f = shapers_resp[bs_name]["freq"]
        .template get<double>();
      shaper_details.push_back(fmt::format("Melhor: {} em {:.1f} Hz\n", bs_name, f));

      uint32_t idx = find_shaper_index(shapers, bs_name);
      lv_dropdown_set_selected(dd, idx);

      lv_slider_set_value(slider, f * 10, LV_ANIM_OFF);
      lv_label_set_text(slider_label, fmt::format("{:.1f} Hz", f).c_str());
    }

    if (label != NULL) {
      // compact, proportional-font-friendly list (the full table is on the graph)
      // Order to match the graph legend (Klipper's INPUT_SHAPERS order), NOT the
      // alphabetical order nlohmann::json's default map would give us.
      static const std::vector<std::string> graph_order = {
        "zv", "mzv", "zvd", "ei", "2hump_ei", "3hump_ei"
      };
      auto emit = [&](const std::string &name, json &el) {
        shaper_details.push_back(
          fmt::format("{}  {:.1f} Hz  vib. {:.1f}%",
            name,
            el["freq"].template get<double>(),
            el["vib"].template get<double>()));
      };
      for (auto &name : graph_order) {
        auto it = shapers_resp.find(name);
        if (it != shapers_resp.end()) {
          emit(name, *it);
        }
      }
      // safety: surface any shaper the graph order list doesn't know about
      for (auto &el : shapers_resp.items()) {
        if (std::find(graph_order.cbegin(), graph_order.cend(), el.key())
            == graph_order.cend()) {
          emit(el.key(), el.value());
        }
      }

      lv_label_set_text(label, fmt::format("{}", fmt::join(shaper_details, "\n")).c_str());
    }
  }
}

void InputShaperPanel::layout_panes(bool graph, bool x_sel, bool y_sel) {
  // girador no meio da célula; cartão do gráfico e texto ocupam a célula toda
  auto cell = [](lv_obj_t *o, uint8_t col) {
    lv_obj_set_grid_cell(o, LV_GRID_ALIGN_CENTER, col, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  };
  auto cell_cheia = [](lv_obj_t *o, uint8_t col) {
    lv_obj_set_grid_cell(o, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  };
  // wipe stale console text so a previous run can't peek out behind a new pane
  lv_label_set_text(xoutput, "");
  lv_label_set_text(youtput, "");
  // default: each axis owns its column (X=1, Y=2) - graphs side by side
  cell(xspinner, 1); cell_cheia(xgraph_cont, 1); cell_cheia(xoutput, 1);
  cell(yspinner, 2); cell_cheia(ygraph_cont, 2); cell_cheia(youtput, 2);

  // single axis WITH graph -> graph on the left, its text console on the right.
  // O cartão (opaco) do outro eixo sai da frente para o texto não ficar por
  // cima dele; ele volta quando aquele eixo tiver gráfico de novo.
  if (graph && x_sel && !y_sel) {
    cell_cheia(xoutput, 2);                       // X graph stays col1, console to col2
    lv_obj_add_flag(ygraph_cont, LV_OBJ_FLAG_HIDDEN);
  } else if (graph && y_sel && !x_sel) {
    cell(yspinner, 1); cell_cheia(ygraph_cont, 1); // Y graph to col1, console stays col2
    lv_obj_add_flag(xgraph_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void InputShaperPanel::end_calibration_ui() {
  calibrate_btn.enable();
  save_btn.enable();
  // unlock the Graph switch
  lv_obj_clear_state(graph_switch, LV_STATE_DISABLED);
  lv_obj_add_flag(graph_switch, LV_OBJ_FLAG_CLICKABLE);
  girador(xspinner, false);
  girador(yspinner, false);
  if (cal_watchdog != NULL) {
    lv_timer_del(cal_watchdog);
    cal_watchdog = NULL;
  }
}

void InputShaperPanel::handle_watchdog() {
  // Runs on the LVGL timer thread (lv_timer_handler already holds lv_lock), and
  // repeat_count is 1 so LVGL frees the timer after this returns - drop our handle
  // and DON'T lv_timer_del it here.
  cal_watchdog = NULL;
  if (x_pending || y_pending) {
    x_pending = false;
    y_pending = false;
    y_after_move = false;
    calibrate_btn.enable();
    save_btn.enable();
    girador(xspinner, false);
    girador(yspinner, false);
    KUtils::notify_toast(
      "A calibração demorou demais e nada foi alterado. Confira a impressora e tente de novo.", 6000);
  }
}

void InputShaperPanel::start_axis(bool is_x) {
  lv_obj_t *graph = is_x ? xgraph : ygraph;
  lv_obj_t *graph_cont = is_x ? xgraph_cont : ygraph_cont;
  lv_obj_t *output = is_x ? xoutput : youtput;
  lv_obj_t *spinner = is_x ? xspinner : yspinner;

  ws.gcode_script(fmt::format("TEST_RESONANCES AXIS={} NAME={}\nM400",
    is_x ? "X" : "Y", is_x ? "x" : "y"));

  // free src + hack to color in empty space (same as the original per-axis path)
  lv_img_set_src(graph, NULL);
  ((lv_img_t *)graph)->src_type = LV_IMG_SRC_SYMBOL;

  lv_label_set_text(output, "");
  lv_obj_add_flag(graph_cont, LV_OBJ_FLAG_HIDDEN);
  girador(spinner, true);
  lv_obj_move_foreground(spinner);
}

void InputShaperPanel::begin_axis(bool is_x) {
  calibrate_btn.disable();
  save_btn.disable();

  bool homing = !KUtils::is_homed();
  bool graph = lv_obj_has_state(graph_switch, LV_STATE_CHECKED);
  // lock the Graph mode for the whole run so it can't be toggled mid-test
  lv_obj_add_state(graph_switch, LV_STATE_DISABLED);
  lv_obj_clear_flag(graph_switch, LV_OBJ_FLAG_CLICKABLE);
  KUtils::notify_toast(
    fmt::format("Calibrando o eixo {}: a impressora vai vibrar por cerca de 1 minuto. Não mexa nela.{}{}",
      is_x ? "X" : "Y",
      homing ? " Antes, ela vai à origem." : "",
      graph ? " Com o gráfico ligado, demora mais." : ""),
    5000);

  if (is_x) {
    x_pending = true;
  } else {
    y_pending = true;
  }

  if (cal_watchdog != NULL) {
    lv_timer_del(cal_watchdog);
    cal_watchdog = NULL;
  }
  // Backstop only (prevents an endless spinner) - must NOT fire on a normal run.
  // One axis = resonance sweep (~2 min) + analysis, plus graph render when on,
  // so be generous: 10 min with graph, 8 min without. The shell command has its
  // own 600s timeout, so a real hang still surfaces.
  cal_watchdog = lv_timer_create(&InputShaperPanel::_watchdog_cb,
    graph ? 600000 : 480000, this);
  lv_timer_set_repeat_count(cal_watchdog, 1);

  if (homing) {
    ws.gcode_script("G28");
  }
  start_axis(is_x);
}

void InputShaperPanel::show_placement_prompt(bool is_x, bool moving) {
  next_axis_is_x = is_x;
  static const char *btns[] = {"Continuar", "Cancelar", ""};
  const char *title = moving ? "Mudar o acelerômetro de lugar" : "Posicionar o acelerômetro";
  std::string where = is_x ? "no CABEÇOTE (para o X)" : "na MESA (para o Y)";
  std::string body = moving
    ? fmt::format("Primeiro eixo pronto.\n\nAgora prenda o acelerômetro {} e toque em Continuar.", where)
    : fmt::format("Prenda o acelerômetro {} e toque em Continuar.", where);
  lv_obj_t *mbox = lv_msgbox_create(NULL, title, body.c_str(), btns, false);
  KUtils::style_lock_mbox(mbox, 90);
  lv_obj_add_event_cb(mbox, &InputShaperPanel::_placement_prompt_cb, LV_EVENT_VALUE_CHANGED, this);
}

void InputShaperPanel::_placement_prompt_cb(lv_event_t *e) {
  lv_obj_t *mbox = lv_event_get_current_target(e);
  InputShaperPanel *self = (InputShaperPanel *)lv_event_get_user_data(e);
  uint32_t btn = lv_msgbox_get_active_btn(mbox);
  lv_msgbox_close(mbox);

  if (btn == 0) {                         // Continue
    bool is_x = self->next_axis_is_x;
    if (!is_x) {
      self->y_after_move = false;         // Y is now running, nothing deferred
    }
    self->begin_axis(is_x);
  } else {                                // Cancel
    self->y_after_move = false;
    self->end_calibration_ui();           // idempotent; keeps any axis already saved
  }
}
