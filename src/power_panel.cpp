#include "power_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <map>
#include <fstream>

PowerPanel::PowerPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : ws(websocket_client)
  , lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , lista(lv_obj_create(cont))
{
  // subtela 424 x 272 com cabeçalho "Energia" (voltar = fechar a subtela)
  Navegacao::subtela(cont);
  Marca::criar_cabecalho(cont, "Energia");

  // lista abaixo do cabeçalho; rola na vertical se houver muitos
  // dispositivos (sem inércia e sem elástico)
  lv_obj_set_pos(lista, 0, Marca::TOQUE);
  lv_obj_set_size(lista, Marca::CONT_L, Marca::TELA_A - Marca::TOQUE);
  lv_obj_set_style_pad_all(lista, 0, 0);
  lv_obj_set_style_pad_hor(lista, Marca::MARGEM, 0);
  lv_obj_set_style_pad_bottom(lista, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(lista, 0, 0);
  lv_obj_set_flex_flow(lista, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(lista, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_scroll_dir(lista, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(lista, LV_SCROLLBAR_MODE_ACTIVE);
  lv_obj_clear_flag(lista, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(lista, LV_OBJ_FLAG_SCROLL_ELASTIC);

  // Power-loss recovery sits at the top, above the power-device toggles.
  build_recovery_section();
}

void PowerPanel::build_recovery_section() {
  // cartão PRETO_2: título f14, texto 12 G e os dois botões de 44
  recovery_cont = lv_obj_create(lista);
  Marca::cartao(recovery_cont);
  lv_obj_set_size(recovery_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(recovery_cont, 12, 0);
  lv_obj_set_style_pad_row(recovery_cont, 8, 0);
  lv_obj_set_flex_flow(recovery_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(recovery_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  Marca::criar_rotulo(recovery_cont, LV_SYMBOL_CHARGE " Queda de energia", Marca::f14(), Marca::PAPEL);

  recovery_status = Marca::criar_rotulo(recovery_cont, "", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_width(recovery_status, LV_PCT(100));
  lv_label_set_long_mode(recovery_status, LV_LABEL_LONG_WRAP);

  lv_obj_t *btn_row = lv_obj_create(recovery_cont);
  lv_obj_set_size(btn_row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_set_style_pad_column(btn_row, 8, 0);
  lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

  // "Retomar impressão" é a ação principal da tela: DESTAQUE (limão, texto preto)
  recovery_resume_btn = Marca::criar_botao(btn_row, LV_SYMBOL_PLAY " Retomar impressão", Marca::Tipo::DESTAQUE);
  lv_obj_set_size(recovery_resume_btn, 200, Marca::TOQUE);
  lv_obj_add_event_cb(recovery_resume_btn, &PowerPanel::_handle_callback, LV_EVENT_CLICKED, this);

  recovery_dismiss_btn = Marca::criar_botao(btn_row, LV_SYMBOL_CLOSE " Ignorar", Marca::Tipo::NORMAL);
  lv_obj_set_size(recovery_dismiss_btn, 120, Marca::TOQUE);
  lv_obj_add_event_cb(recovery_dismiss_btn, &PowerPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // Hidden until refresh_recovery() finds a recoverable print.
  lv_obj_add_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
}

std::string PowerPanel::recoverable_print(std::string &display) {
  display = "";
  // The saved-state file lives on the printer's filesystem; only readable when the UI
  // runs locally on the machine (not the desktop simulator / remote).
  if (!KUtils::is_running_local()) {
    return "";
  }
  std::ifstream f("/usr/data/creality/userdata/config/print_file_name.json");
  if (!f.good()) {
    return "";
  }
  try {
    json j = json::parse(f, nullptr, false);
    if (j.is_discarded() || !j.contains("file_path")) {
      return "";
    }
    std::string fp = j["file_path"].template get<std::string>();
    if (fp.empty()) {
      return "";
    }
    // SDCARD_PRINT_FILE wants the path relative to the gcodes dir.
    std::string rel = fp;
    auto pos = fp.find("/gcodes/");
    if (pos != std::string::npos) {
      rel = fp.substr(pos + 8); // strlen("/gcodes/")
    }
    auto slash = rel.find_last_of('/');
    display = (slash == std::string::npos) ? rel : rel.substr(slash + 1);
    return rel;
  } catch (...) {
    return "";
  }
}

void PowerPanel::refresh_recovery() {
  if (recovery_cont == nullptr) {
    return;
  }
  std::string display;
  recovery_relpath = recoverable_print(display);

  bool show_actions = false;
  if (KUtils::is_printing()) {
    lv_label_set_text(recovery_status,
      "Impressão em andamento. Se faltar energia, abra esta tela de novo para retomar.");
  } else if (recovery_relpath.empty()) {
    lv_label_set_text(recovery_status, "Nenhuma impressão interrompida para retomar.");
  } else {
    lv_label_set_text(recovery_status,
      ("Última impressão: " + display + "\nRetomar de onde parou quando faltou energia?").c_str());
    show_actions = true;
  }

  if (show_actions) {
    lv_obj_clear_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

PowerPanel::~PowerPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }

  devices.clear();
}

void PowerPanel::create_device(json &j) {
  std::string name = j["device"].template get<std::string>();

  lv_obj_t *power_device_toggle;
  auto entry = devices.find(name);
  if (entry == devices.end()) {
    // linha 44 de altura: nome f14 à esquerda, chave 44 x 24 à direita,
    // separador 1 px PRETO_3 embaixo (criada uma vez por dispositivo)
    lv_obj_t *power_device_toggle_cont = lv_obj_create(lista);
    lv_obj_set_size(power_device_toggle_cont, LV_PCT(100), Marca::TOQUE);
    lv_obj_set_style_pad_all(power_device_toggle_cont, 0, 0);
    lv_obj_set_style_pad_hor(power_device_toggle_cont, 6, 0);
    lv_obj_set_style_border_side(power_device_toggle_cont, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(power_device_toggle_cont, 1, 0);
    lv_obj_set_style_border_color(power_device_toggle_cont, lv_color_hex(Marca::PRETO_3), 0);
    lv_obj_clear_flag(power_device_toggle_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = Marca::criar_rotulo(power_device_toggle_cont, name.c_str(), Marca::f14(), Marca::PAPEL);
    lv_obj_set_width(l, 300);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

    power_device_toggle = lv_switch_create(power_device_toggle_cont);
    lv_obj_set_size(power_device_toggle, 44, 24);
    lv_obj_set_ext_click_area(power_device_toggle, 10);
    lv_obj_align(power_device_toggle, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_add_event_cb(power_device_toggle, &PowerPanel::_handle_callback,
	        LV_EVENT_VALUE_CHANGED, this);

    devices.insert({name, power_device_toggle});
  } else {
    power_device_toggle = entry->second;
  }

  std::string status = j["status"].template get<std::string>();
  spdlog::debug("Fetched initial status for power device {}: {}", name, status);

  if (status == "on") {
    lv_obj_add_state(power_device_toggle, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(power_device_toggle, LV_STATE_CHECKED);
  }
}

void PowerPanel::create_devices(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  if (j.contains("result")) {
    json result = j["result"];
    if (result.contains("devices")) {
      json devices = result["devices"];
      for (auto &device : devices.items()) {
        create_device(device.value());
      }
    }
  }
}

void PowerPanel::handle_device_callback(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);

  if (j.contains("result")) {
    json result = j["result"];
    for (auto &device : result.items()) {
      auto entry = devices.find(device.key());
      if (entry != devices.end()) {
        std::string new_status = device.value();

        spdlog::debug("Fetched new status for power device {}: {}", entry->first, new_status);

        if (new_status == "on") {
          lv_obj_add_state(entry->second, LV_STATE_CHECKED);
        } else {
          lv_obj_clear_state(entry->second, LV_STATE_CHECKED);
        }
      }
    }
  }
}

void PowerPanel::foreground() {
  lv_obj_scroll_to_y(lista, 0, LV_ANIM_OFF);
  Navegacao::abrir(cont);

  // Re-check for a recoverable print each time the screen is opened.
  refresh_recovery();

  json params;
  for (auto &device : devices) {
    params[device.first] = 0; // value is ignored by moonraker
  }
  ws.send_jsonrpc("machine.device_power.status", params, [this](json& j) {
    this->handle_device_callback(j);
  });
}

void PowerPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);
    if (btn == recovery_resume_btn) {
      if (!recovery_relpath.empty()) {
        // Creality's virtual_sdcard restores the saved breakpoint (reheat + position)
        // when ISCONTINUEPRINT=1; if no valid breakpoint exists it safely starts the
        // file normally rather than crashing.
        ws.gcode_script("SDCARD_PRINT_FILE FILENAME=\"" + recovery_relpath + "\" ISCONTINUEPRINT=1");
        KUtils::notify_toast("Retomando a impressão após a queda de energia...", 2500);
        lv_label_set_text(recovery_status, "Retomando...");
        lv_obj_add_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
        Navegacao::fechar(cont);
      }
    } else if (btn == recovery_dismiss_btn) {
      lv_label_set_text(recovery_status, "Nenhuma impressão interrompida para retomar.");
      lv_obj_add_flag(recovery_resume_btn, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(recovery_dismiss_btn, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *obj = lv_event_get_target(e);
    for (auto &device : devices) {
      if (obj == device.second) {
        bool turnOn = lv_obj_has_state(device.second, LV_STATE_CHECKED);
        std::string status = turnOn ? "on" : "off";

        spdlog::debug("Turning power device {} {}", device.first, status);

        json params;
        params["device"] = device.first;
        params["action"] = status;
        ws.send_jsonrpc("machine.device_power.post_device", params, [this](json& j) {
          this->handle_device_callback(j);
        });
	      break;
      }
    }
  }
}
