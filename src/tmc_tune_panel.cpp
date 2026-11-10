#include "tmc_tune_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <utility>

LV_IMG_DECLARE(sd_img);

static std::map<std::string, int> goal_idx_map = {
  {"auto", 0 },
  {"silent", 1 },
  {"performance", 2 },
  {"autoswitch", 3 }
};

static std::map<std::string, std::pair<int16_t, int16_t>> tmc_sg_range = {
  {"tmc2130", { -64, 63 } },
  {"tmc2209", { 0, 255 } },
  {"tmc2240", { -64, 63 } },
  {"tmc2660", { -64, 63 } },
  {"tmc5160", { -64, 63 } }
};

AutoTmcContainer::AutoTmcContainer(const std::list<std::string> &motors,
				   const std::string &stepper_name,
				   int motor_idx,
				   int goal_idx,
				   bool sg_configured,
				   int16_t sgthrs,
				   std::pair<int16_t, int16_t> sg_min_max,
				   lv_obj_t *parent)
  : cont(lv_obj_create(parent))
  , name(stepper_name)
  , motors_dd(lv_dropdown_create(cont))
  , tuning_goal_dd(lv_dropdown_create(cont))
  , spinbox_cont(lv_obj_create(cont))
  , sensorless_threshold(lv_spinbox_create(spinbox_cont))
  , configured_motor_idx(motor_idx)
  , configured_goal_idx(goal_idx)
  , has_sg(sg_configured)
  , configured_sensorless_thrs(sgthrs)
  , sg_range(sg_min_max)
{
  // Um cartão por motor (PRETO_2, raio 12), pad 8; linhas de 44 com o nome no alto.
  Marca::cartao(cont);
  lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(cont, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(cont, 6, 0);
  lv_obj_set_style_pad_column(cont, Marca::VAO, 0);

  lv_dropdown_clear_options(motors_dd);

  size_t index = 0;
  lv_dropdown_add_option(motors_dd, "Não configurado", index++);

  for (auto &s : motors) {
    lv_dropdown_add_option(motors_dd, s.substr(s.find(' ') + 1).c_str(), index++);
  }

  lv_obj_set_size(motors_dd, LV_PCT(60), Marca::TOQUE);
  lv_obj_set_style_text_font(motors_dd, Marca::f14(), 0);
  lv_dropdown_set_selected(motors_dd, motor_idx);

  lv_obj_set_size(tuning_goal_dd, LV_PCT(40), Marca::TOQUE);
  lv_obj_set_style_text_font(tuning_goal_dd, Marca::f14(), 0);

  lv_dropdown_set_options(tuning_goal_dd,
			  "auto\n"
			  "silent\n"
			  "performance\n"
			  "autoswitch");

  lv_dropdown_set_selected(tuning_goal_dd, goal_idx);

  // "-" 44 x 44 | valor 64 x 44 | "+" 44 x 44, posições fixas (164 x 44)
  lv_obj_set_style_pad_all(spinbox_cont, 0, 0);
  lv_obj_clear_flag(spinbox_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(sensorless_threshold, 50, 0);
  lv_obj_set_size(sensorless_threshold, 64, Marca::TOQUE);
  lv_obj_set_style_text_font(sensorless_threshold, Marca::f14(), 0);
  lv_obj_set_style_text_align(sensorless_threshold, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_set_size(spinbox_cont, 164, Marca::TOQUE);
  lv_spinbox_set_range(sensorless_threshold, sg_range.first, sg_range.second);
  lv_spinbox_set_value(sensorless_threshold, sgthrs);
  lv_spinbox_set_step(sensorless_threshold, 5);
  lv_spinbox_set_digit_format(sensorless_threshold, 3, 0);
  lv_obj_set_style_border_width(sensorless_threshold, 0, LV_PART_CURSOR);
  lv_obj_set_style_border_opa(sensorless_threshold, LV_OPA_0, LV_PART_CURSOR);
  lv_obj_set_style_bg_opa(sensorless_threshold, LV_OPA_0, LV_PART_CURSOR);
  lv_textarea_set_cursor_click_pos(sensorless_threshold, false);

  lv_obj_t * btn = lv_btn_create(spinbox_cont);
  lv_obj_set_pos(btn, 120, 0);
  lv_obj_set_size(btn, 44, Marca::TOQUE);
  lv_obj_set_style_bg_img_src(btn, LV_SYMBOL_PLUS, 0);
  lv_obj_add_event_cb(btn, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_SHORT_CLICKED || code  == LV_EVENT_LONG_PRESSED_REPEAT) {
      lv_spinbox_increment((lv_obj_t*)e->user_data);
    }
  }, LV_EVENT_ALL, sensorless_threshold);

  btn = lv_btn_create(spinbox_cont);
  lv_obj_set_pos(btn, 0, 0);
  lv_obj_set_size(btn, 44, Marca::TOQUE);
  lv_obj_set_style_bg_img_src(btn, LV_SYMBOL_MINUS, 0);
  lv_obj_add_event_cb(btn, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_SHORT_CLICKED || code  == LV_EVENT_LONG_PRESSED_REPEAT) {
      lv_spinbox_decrement((lv_obj_t*)e->user_data);
    }
  }, LV_EVENT_ALL, sensorless_threshold);


  // linhas pelo conteúdo (o cartão tem altura pelo conteúdo)
  static lv_coord_t grid_main_row3_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};

  // rótulos das linhas: 12 G ("Objetivo do ajuste" mede 111 px; a coluna tem ~128)
  auto rotulo = [this](const char *txt) {
    lv_obj_t *l = lv_label_create(cont);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, Marca::f12(), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(Marca::GRAFITE), 0);
    return l;
  };

  if (!has_sg) {
    lv_obj_add_flag(spinbox_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_grid_dsc_array(cont, grid_main_col_dsc, grid_main_row3_dsc);
  } else {
    lv_obj_set_grid_dsc_array(cont, grid_main_col_dsc, grid_main_row_dsc);

    lv_obj_t *label = rotulo("Limiar sensorless");
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    lv_obj_set_grid_cell(spinbox_cont, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);
  }

  // nome do motor (f14 PAPEL), na linha de cima
  lv_obj_t *label = lv_label_create(cont);
  lv_label_set_text(label, name.c_str());
  lv_obj_set_style_text_font(label, Marca::f14(), 0);

  // row 1
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);

  label = rotulo("Motor");
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_set_grid_cell(motors_dd, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

  label = rotulo("Objetivo do ajuste");
  lv_obj_set_grid_cell(label, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  lv_obj_set_grid_cell(tuning_goal_dd, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);

}

AutoTmcContainer::~AutoTmcContainer() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

std::string AutoTmcContainer::get_config_macro() {
  char buf[128];
  lv_dropdown_get_selected_str(motors_dd, buf, sizeof(buf));
  std::string motor = std::string(buf);
  if (motor == "Não configurado") {
    return fmt::format("_GUPPY_DELETE_CONFIG SECTION=\"autotune_tmc {}\"", name);
  }

  lv_dropdown_get_selected_str(tuning_goal_dd, buf, sizeof(buf));
  if (has_sg) {
    auto sg_value = lv_spinbox_get_value(sensorless_threshold);
    return fmt::format("_GUPPY_SAVE_CONFIG SECTION=\"autotune_tmc {}\" KEY_VALUE=\"motor:{},tuning_goal:{},{}:{}\"",
		       name, motor, buf, sg_range.first == 0 ? "sg4_thrs" : "sgt", sg_value);
  } else {
    return fmt::format("_GUPPY_SAVE_CONFIG SECTION=\"autotune_tmc {}\" KEY_VALUE=\"motor:{},tuning_goal:{}\"",
		       name, motor, buf);
  }
}

TmcTunePanel::TmcTunePanel(KWebSocketClient &c)
  : ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , controls_cont(lv_obj_create(cont))
  , btns_cont(lv_obj_create(cont))
  , save_btn(btns_cont, &sd_img, "Salvar e reiniciar", [](lv_event_t *e) {
    TmcTunePanel *panel = (TmcTunePanel*)e->user_data;
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      panel->save_config();
    }
  }, this)
{
  motor_parser._delim = ":";

  // Subtela (Proposta B), coordenadas da subtela:
  //   cabeçalho "Autoajuste dos motores" com voltar (fecha a subtela)
  //   lista dos motores (8,52) 408 x 152, um cartão por motor, rola na vertical sem inércia
  //   "Salvar e reiniciar" DESTAQUE 408 x 48 em (8,212), f16
  Navegacao::subtela(cont);
  Marca::Cabecalho cab = Marca::criar_cabecalho(cont, "Autoajuste dos motores");
  lv_obj_set_width(cab.titulo, 366);  // 243 px na f20; o lado direito não é usado

  lv_obj_set_pos(controls_cont, Marca::MARGEM, 52);
  lv_obj_set_size(controls_cont, Marca::CONT_L - 2 * Marca::MARGEM, 152);
  lv_obj_set_style_pad_all(controls_cont, 0, 0);
  lv_obj_set_style_pad_row(controls_cont, Marca::VAO, 0);
  lv_obj_set_flex_flow(controls_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(controls_cont, LV_DIR_VER);
  lv_obj_clear_flag(controls_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(controls_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);

  lv_obj_set_pos(btns_cont, Marca::MARGEM, 212);
  lv_obj_set_size(btns_cont, Marca::CONT_L - 2 * Marca::MARGEM, 48);
  lv_obj_set_style_pad_all(btns_cont, 0, 0);
  lv_obj_clear_flag(btns_cont, LV_OBJ_FLAG_SCROLLABLE);

  save_btn.sem_icone();
  save_btn.set_fonte(Marca::f16());
  lv_obj_set_size(save_btn.get_container(), LV_PCT(100), 48);
  save_btn.set_tipo(Marca::Tipo::DESTAQUE);
}

TmcTunePanel::~TmcTunePanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void TmcTunePanel::init(json &j, fs::path &kp) {
  if (motor_parser.LoadFromFile(kp.string().c_str()) != 0) {
    spdlog::error("Failed to load motor_databse.cfg for TMC AUTOTUNE");
    return;
  }

  motor_index.clear();
  steppers.clear();

  // 0 reserved for no configuration
  auto motors = motor_parser.GetSections();
  int motor_idx = 1;
  for (auto &s : motors) {
    motor_index.insert({s.substr(s.find(' ') + 1), motor_idx++});
  }

  State *s = State::get_instance();
  auto v = s->get_data("/printer_state/configfile/config"_json_pointer);

  std::map<std::string, json> tuned_config;
  std::vector<std::string> configured_steppers;

  for (auto &el : v.items()) {
    if (el.key().rfind("autotune_tmc ", 0) == 0) {
      // existing auto tune tmc configs      
      std::string stepper_name = el.key().substr(el.key().find(' ') + 1);
      tuned_config.insert({stepper_name, el.value()});

      spdlog::debug("found tuned stepper {}, {}", stepper_name, el.value().dump());
    }
    
    if (el.key().rfind("tmc", 0) == 0 && el.key().find(' ') != std::string::npos) {
      spdlog::debug("found configured tmc {}", el.key());      
      configured_steppers.push_back(el.key());
    }
  }

  for (auto &s : configured_steppers) {
    std::string tmctype = s.substr(0, s.find(' '));
    std::string stepper_name = s.substr(s.find(' ') + 1);
    spdlog::debug("tmctype {}, stepper {}, s {}", tmctype, stepper_name, s);
    
    auto endstop_pin = v[json::json_pointer(fmt::format("/{}/endstop_pin", stepper_name))];
    auto sg_range_el = tmc_sg_range.find(tmctype);
    bool has_virtual_endstop = !endstop_pin.is_null()
      && endstop_pin.template get<std::string>().find("virtual_endstop") != std::string::npos
      && sg_range_el != tmc_sg_range.end();

    spdlog::debug("has_virtual_endstop {}, {}, {}", !endstop_pin.is_null(),
		  !endstop_pin.is_null() ? endstop_pin.template get<std::string>().find("virtual_endstop") != std::string::npos : false,
		  !endstop_pin.is_null() ? endstop_pin.template get<std::string>(): "null");
    
    auto sg_range = sg_range_el != tmc_sg_range.end()
      ? sg_range_el->second
      : std::make_pair<int16_t, int16_t>(0, 0);

    const auto &el = tuned_config.find(stepper_name);
    if (el != tuned_config.end()) {
      auto motor = el->second["/motor"_json_pointer];
      auto goal = el->second["/tuning_goal"_json_pointer];
      auto sgthrs = el->second["/sg4_thrs"_json_pointer];
      
      spdlog::debug("found tuned stepper {}, {}, {}",
		    motor.is_null() ? "NULL" : motor.template get<std::string>(),
		    goal.is_null() ? "auto" : goal.template get<std::string>(),
		    sgthrs.is_null() ? 10 : std::stoi(sgthrs.template get<std::string>())
		    );

      int midx = 0;
      if (!motor.is_null()) {
	const auto &el = motor_index.find(motor.template get<std::string>());
	if (el != motor_index.end()) {
	  midx = el->second;
	}
      }

      int goal_idx = 0;
      if (!goal.is_null()) {
	const auto &el = goal_idx_map.find(goal.template get<std::string>());
	if (el != goal_idx_map.end()) {
	  goal_idx = el->second;
	}
      }

      int16_t sg_value = !sgthrs.is_null()
	? std::stoi(sgthrs.template get<std::string>())
	: (sg_range.first == 0 ? 80 : 1); // sg4 default 80, sgt default 1

      steppers.push_back(std::make_shared<AutoTmcContainer>(motors, el->first, midx, goal_idx,
							    has_virtual_endstop, sg_value, sg_range,
							    controls_cont));
    } else {
      spdlog::debug("did not find tuned config for stepper {}", s);
      int16_t sg_value = sg_range.first == 0 ? 80 : 1; // sg4 default 80, sgt default 1
      steppers.push_back(std::make_shared<AutoTmcContainer>(motors, stepper_name, 0, 0,
							    has_virtual_endstop, sg_value, sg_range,
							    controls_cont));
    }
  }
}

void TmcTunePanel::foreground() {
  Navegacao::abrir(cont);
}

void TmcTunePanel::background() {
  Navegacao::fechar(cont);
}

void TmcTunePanel::save_config() {
  std::vector<std::string> save_configs;
  for (auto &s : steppers) {
    save_configs.push_back(s->get_config_macro());
  }

  ws.gcode_script(fmt::format("{}\nSAVE_CONFIG", fmt::join(save_configs, "\n")));
}
