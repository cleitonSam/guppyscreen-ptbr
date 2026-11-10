#include "tmc_status_container.h"
#include "marca.h"
#include "spdlog/spdlog.h"

TmcStatusContainer::TmcStatusContainer(KWebSocketClient &c,
				       lv_obj_t *parent,
				       const std::string &stepper_name)
  : ws(c)
  , cont(lv_obj_create(parent))
  , chart_cont(lv_obj_create(cont))
  , label(lv_label_create(chart_cont))
  , legend(lv_obj_create(chart_cont))
  , chart(lv_chart_create(chart_cont))
  , sg_series(lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_ORANGE), LV_CHART_AXIS_PRIMARY_Y))
  , irms_series(lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y))
  , semin_series(lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y))
  , semax_series(lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y))
  , stepper_config(lv_obj_create(cont))

  , semin_sb(stepper_config, "semin", 0, 15, 0,
	     [this, stepper_name](int v) {
	       auto n = stepper_name.substr(stepper_name.find(' ') + 1);
	       update_tmc_value(n, "semin", v);
	     })
  , semax_sb(stepper_config, "semax", 0, 15, 0,
 	     [this, stepper_name](int v) {
	       auto n = stepper_name.substr(stepper_name.find(' ') + 1);
	       update_tmc_value(n, "semax", v);
	     })
  , seup_sb(stepper_config, "seup", 0, 3, 0,
 	     [this, stepper_name](int v) {
	       auto n = stepper_name.substr(stepper_name.find(' ') + 1);
	       update_tmc_value(n, "seup", v);
	     })
  , sedn_sb(stepper_config, "sedn", 0, 3, 0,
 	     [this, stepper_name](int v) {
	       auto n = stepper_name.substr(stepper_name.find(' ') + 1);
	       update_tmc_value(n, "sedn", v);
	     })
  , toff_sb(stepper_config, "toff", 0, 15, 0,
 	     [this, stepper_name](int v) {
	       auto n = stepper_name.substr(stepper_name.find(' ') + 1);
	       update_tmc_value(n, "toff", v);
	     })
  , tbl_sb(stepper_config, "tbl", 0, 3, 0,
 	     [this, stepper_name](int v) {
	       auto n = stepper_name.substr(stepper_name.find(' ') + 1);
	       update_tmc_value(n, "tbl", v);
	     })
  , hstrt_sb(stepper_config, "hstrt", 0, 7, 0,
 	     [this, stepper_name](int v) {
	       auto n = stepper_name.substr(stepper_name.find(' ') + 1);
	       update_tmc_value(n, "hstrt", v);
	     })
  , hend_sb(stepper_config, "hend", 0, 15, 0,
 	     [this, stepper_name](int v) {
	       auto n = stepper_name.substr(stepper_name.find(' ') + 1);
	       update_tmc_value(n, "hend", v);
	     })
{
  // Um bloco por motor (Proposta B): nome f14, legenda f12, gráfico 100% x 120
  // (fundo PRETO_2 e linhas de divisão PRETO_3, do tema) e, embaixo, as linhas
  // de ajuste (nome | - | valor | +) em duas colunas. Separador 1 px PRETO_3.
  lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_pad_top(cont, 8, 0);
  lv_obj_set_style_pad_bottom(cont, 12, 0);
  lv_obj_set_style_pad_row(cont, 8, 0);
  lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(cont, 1, 0);
  lv_obj_set_style_border_color(cont, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_flex_flow(chart_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(chart_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(chart_cont, 0, 0);
  lv_obj_set_style_pad_row(chart_cont, 4, 0);
  lv_obj_clear_flag(chart_cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_font(label, Marca::f14(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(Marca::PAPEL), 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_label_set_text(label, fmt::format("{} (corrente/carga)", stepper_name).c_str());

  lv_obj_set_size(legend, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(legend, 0, 0);
  lv_obj_set_style_pad_column(legend, 12, 0);
  lv_obj_set_style_pad_row(legend, 2, 0);
  lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_clear_flag(legend, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *axis_label = Marca::criar_rotulo(legend, "sg_result", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_style_text_color(axis_label, lv_palette_main(LV_PALETTE_ORANGE), 0);

  axis_label = Marca::criar_rotulo(legend, "i_rms (mA)", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_style_text_color(axis_label, lv_palette_main(LV_PALETTE_RED), 0);

  axis_label = Marca::criar_rotulo(legend, "semin * 32", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_style_text_color(axis_label, lv_palette_main(LV_PALETTE_BLUE), 0);

  axis_label = Marca::criar_rotulo(legend, "(semin + semax + 1) * 32", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_style_text_color(axis_label, lv_palette_main(LV_PALETTE_GREEN), 0);

  // gráfico: fundo PRETO_2 e linhas PRETO_3 (garantido aqui também, além do
  // tema); números do eixo f12 GRAFITE à esquerda (40 px de margem para eles)
  // 408 de largura útil: 40 para os números + 368 de gráfico (deslocado 40
  // para a direita; o LVGL 8.3 não tem margem, o deslocamento é o jeito leve)
  lv_obj_set_size(chart, 368, 120);
  lv_obj_set_style_translate_x(chart, 40, 0);
  lv_obj_set_style_bg_color(chart, lv_color_hex(Marca::PRETO_2), 0);
  lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
  lv_obj_set_style_line_color(chart, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_set_style_border_width(chart, 0, 0);
  lv_obj_set_style_radius(chart, Marca::RAIO_MINI, 0);
  lv_obj_set_style_text_font(chart, Marca::f12(), LV_PART_TICKS);
  lv_obj_set_style_text_color(chart, lv_color_hex(Marca::GRAFITE), LV_PART_TICKS);
  lv_obj_set_style_line_color(chart, lv_color_hex(Marca::PRETO_3), LV_PART_TICKS);
  lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLL_ELASTIC);

  lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);

  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1600);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 6, 5, true, 40);

  lv_chart_set_div_line_count(chart, 3, 8);
  lv_chart_set_point_count(chart, 5000);
  lv_chart_set_zoom_x(chart, 5000);
  lv_obj_scroll_to_x(chart, LV_COORD_MAX, LV_ANIM_OFF);

  // linhas de ajuste: duas por fila (196 + 8 + 196 cabem nos 408)
  lv_obj_set_size(stepper_config, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(stepper_config, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_all(stepper_config, 0, 0);
  lv_obj_set_style_pad_row(stepper_config, 8, 0);
  lv_obj_set_style_pad_column(stepper_config, 8, 0);
  lv_obj_set_flex_align(stepper_config, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_clear_flag(stepper_config, LV_OBJ_FLAG_SCROLLABLE);

  // CoolStep fields (semin/semax/seup/sedn) exist only on StallGuard-capable
  // drivers (TMC2209+). Hide by default; update() reveals them only if the
  // driver actually reports them. Prevents "value does not exist" errors when
  // adjusting them on a TMC2208 (e.g. the Ender-3 V3 KE).
  semin_sb.set_hidden(true);
  semax_sb.set_hidden(true);
  seup_sb.set_hidden(true);
  sedn_sb.set_hidden(true);
}

TmcStatusContainer::~TmcStatusContainer()
{
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void TmcStatusContainer::update(json &stepper) {
  // spdlog::debug("tmc stepper {}", stepper.dump());
  if (!stepper.is_null()) {
    auto v = stepper["/i_rms"_json_pointer];
    if (!v.is_null()) {
      lv_chart_set_next_value(chart, irms_series, v.template get<double>());
    }
    
    v = stepper["/semin"_json_pointer];
    int semin = 0;
    semin_sb.set_hidden(v.is_null());
    if (!v.is_null()) {
      semin = v.template get<int>();
      lv_chart_set_next_value(chart, semin_series, semin * 32);
      semin_sb.update_value(semin);
    }

    v = stepper["/semax"_json_pointer];
    semax_sb.set_hidden(v.is_null());
    if (!v.is_null()) {
      auto semax = v.template get<int>();
      lv_chart_set_next_value(chart, semax_series, (semin + semax + 1) * 32);
      semax_sb.update_value(semax);
    }

    v = stepper["/seup"_json_pointer];
    seup_sb.set_hidden(v.is_null());
    if (!v.is_null()) {
      seup_sb.update_value(v.template get<int>());
    }

    v = stepper["/sedn"_json_pointer];
    sedn_sb.set_hidden(v.is_null());
    if (!v.is_null()) {
      sedn_sb.update_value(v.template get<int>());
    }

    v = stepper["/toff"_json_pointer];
    if (!v.is_null()) {
      toff_sb.update_value(v.template get<int>());
    }

    v = stepper["/tbl"_json_pointer];
    if (!v.is_null()) {
      tbl_sb.update_value(v.template get<int>());
    }

    v = stepper["/hstrt"_json_pointer];
    if (!v.is_null()) {
      hstrt_sb.update_value(v.template get<int>());
    }
    
    v = stepper["/hend"_json_pointer];
    if (!v.is_null()) {
      hend_sb.update_value(v.template get<int>());
    }

    v = stepper["/sg_result"_json_pointer];
    if (!v.is_null()) {
      lv_chart_set_next_value(chart, sg_series, v.template get<int>());
    }
  }
}

void TmcStatusContainer::update_tmc_value(const std::string &stepper_name,
					  const std::string &field_name,
					  int value) {
  ws.gcode_script(fmt::format("SET_TMC_FIELD field={} value={} stepper={}",
			      field_name, value, stepper_name));
}

