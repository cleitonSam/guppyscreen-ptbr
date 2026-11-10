#include "init_panel.h"
#include "utils.h"
#include "state.h"
#include "config.h"
#include "marca.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdio>

// Aviso "Aguardando a impressora iniciar..." / "Bem-vindo..." (Proposta B).
// Mora em lv_layer_top(): nem o acompanhamento automático nem a navegação o
// escondem. É uma faixa PRETO_2 raio 12 sem borda, encostada embaixo da área
// de conteúdo (x = 56 + 8, largura 408, 8 px da borda de baixo), com texto
// f14 PAPEL. Fica embaixo, e não no alto, para não tapar a barra de abas de
// Configurações (o "Bem-vindo" pede para ir lá adicionar a impressora).
// A coluna da esquerda fica livre. Mostrar/esconder só com LV_OBJ_FLAG_HIDDEN.
InitPanel::InitPanel(MainPanel &mp, BedMeshPanel &bmp, FilamentRunoutPanel &frp, std::mutex& l)
  : cont(lv_obj_create(lv_layer_top()))
  , label(lv_label_create(cont))
  , main_panel(mp)
  , bedmesh_panel(bmp)
  , filament_runout_panel(frp)
  , lv_lock(l)
{
  lv_obj_set_size(cont, Marca::CONT_L - 2 * Marca::MARGEM, LV_SIZE_CONTENT);
  lv_obj_align(cont, LV_ALIGN_BOTTOM_LEFT, Marca::NAV_L + Marca::MARGEM, -Marca::MARGEM);
  lv_obj_set_style_min_height(cont, Marca::TOQUE, 0);

  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(cont, lv_color_hex(Marca::PRETO_2), 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(cont, Marca::RAIO, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_shadow_width(cont, 0, 0);
  lv_obj_set_style_pad_hor(cont, 12, 0);
  lv_obj_set_style_pad_ver(cont, 12, 0);

  lv_obj_set_size(label, LV_PCT(100), LV_SIZE_CONTENT);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(label, Marca::f14(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(Marca::PAPEL), 0);

  Config *conf = Config::get_instance();
  if (!conf->get_json("/default_printer").is_null()) {
    lv_label_set_text(label, LV_SYMBOL_WARNING " Aguardando a impressora iniciar...");
  } else {
    lv_label_set_text(label, "Bem-vindo. Adicione suas impressoras na aba Configurações.");
  }
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

InitPanel::~InitPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void InitPanel::connected(KWebSocketClient &ws) {
  spdlog::debug("init panel connected");
  State *state = State::get_instance();
  state->reset();

  ws.send_jsonrpc("printer.objects.list", [this, &ws](json& d) {
    State *state = State::get_instance();
	state->set_data("printer_objs", d, "/result");

	ws.send_jsonrpc("server.files.roots",
			[](json& j) { State::get_instance()->set_data("roots", j, "/result"); });

	ws.send_jsonrpc("printer.info",
			[](json& j) { State::get_instance()->set_data("printer_info", j, "/result"); });
	
	json h = {
	  { "namespace", "fluidd" },
	  { "key", "console" }
	};
	ws.send_jsonrpc("server.database.get_item", h,
			[](json& j) { State::get_instance()->set_data("console", j, "/result/value"); });

	h = {
	  { "namespace", "guppyscreen" }
	};
	ws.send_jsonrpc("server.database.get_item", h,
			[](json& j) { State::get_instance()->set_data("guppysettings", j, "/result/value"); });	

	// console
	this->main_panel.subscribe();

	// spoolman
	ws.send_jsonrpc("server.info", [this](json &j) {
	  spdlog::debug("server_info {}", j.dump());
	  State::get_instance()->set_data("server_info", j, "/result");
	  
	  auto &components = j["/result/components"_json_pointer];
	  if (!components.is_null()) {
	    const auto &has_spoolman = components.template get<std::vector<std::string>>();
	    if (std::find(has_spoolman.begin(), has_spoolman.end(), "spoolman") != has_spoolman.end()) {
	      this->main_panel.enable_spoolman();
	    }
	  }
	});

	auto display_sensors = state->get_display_sensors();
	this->main_panel.create_sensors(display_sensors);

	auto display_fans = state->get_display_fans();
	this->main_panel.create_fans(display_fans);

	auto display_leds = state->get_display_leds();
	this->main_panel.create_leds(display_leds);

	// subscribe to all objects except gcode_macro
	auto objs = d["/result/objects"_json_pointer];
	if (!objs.is_null()) {
	  json sub_objs;
	  bool has_m600_macro = false;
	  for (auto &obj : objs) {
	    std::string obj_name = obj.template get<std::string>();
	    if (obj_name.rfind("gcode_macro ", 0 ) != 0) {
	      sub_objs[obj_name] = nullptr;
	    } else if (obj_name == "gcode_macro M600") {
	      // gcode_macro objects are deliberately excluded from the live
	      // subscription above, so FilamentRunoutPanel can never discover
	      // this itself from printer_state (found for real on-device
	      // 2026-07-18 - it silently showed its own dialog on every
	      // runout regardless of M600 being installed, since printer_state
	      // never contains a "gcode_macro M600" key at all). This is the
	      // one place that ever sees the real, unfiltered object list.
	      has_m600_macro = true;
	    }
	  }
	  filament_runout_panel.set_has_m600_macro(has_m600_macro);

	  sub_objs["tmcstatus"] = nullptr;

	  json subs = {{ "objects", sub_objs }};
	  spdlog::debug("subcribing to {}", subs.dump());
	  ws.send_jsonrpc("printer.objects.subscribe", subs,
			  [this](json &data) {
			    State::get_instance()->set_data("printer_state",
							    data, "/result/status");
			    this->main_panel.init(data);
			    this->bedmesh_panel.refresh_views_with_lock(
									data["/result/status/bed_mesh"_json_pointer]);
			    spdlog::debug("done init");
			    std::lock_guard<std::mutex> lock(this->lv_lock);
			    lv_obj_add_flag(this->cont, LV_OBJ_FLAG_HIDDEN);
			    lv_obj_move_background(this->cont);
			  });
	}
  });

  ws.send_jsonrpc("machine.device_power.devices", [this](json& j) {
    main_panel.get_tune_panel().get_power_panel().create_devices(j);
  });
}

void InitPanel::disconnected(KWebSocketClient &ws) {
  spdlog::debug("init panel disconnected");
  std::lock_guard<std::mutex> lock(lv_lock);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(cont);
}

#ifdef SIMULATOR
void InitPanel::sim_hide() {
  lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(cont);
}
#endif

void InitPanel::set_message(const char *message) {
	lv_label_set_text(label, message);
}
