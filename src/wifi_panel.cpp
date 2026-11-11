#include "wifi_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <vector>
#include <utility>
#include <thread>
#include <chrono>

// Subtela 424 x 272 (coordenadas da subtela):
//   cabeçalho "Wi-Fi" (0..44)
//   lista   (8,52)   248 x 212   cartão que rola na vertical
//   estado  (264,52) 152 x 212   cartão com o texto da ligação
//   senha:  texto (8,52) 408; campo (8,72) 356 x 44 + olho 44 x 44; teclado (0,124) 424 x 148
namespace {
  constexpr lv_coord_t LISTA_X = 8, LISTA_Y = 52, LISTA_A = 212;
  constexpr lv_coord_t ESTADO_X = 264, ESTADO_L = 152;
  constexpr lv_coord_t SENHA_Y = 72, TECLADO_Y = 124;
}

WifiPanel::WifiPanel(std::mutex &l)
  : lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , wifi_list_cont(lv_obj_create(cont))
  , wifi_right(lv_obj_create(cont))
  , prompt_cont(wifi_right)
  , wifi_label(lv_label_create(cont))
  , pw_row(lv_obj_create(cont))
  , password_input(lv_textarea_create(pw_row))
  , eye_btn(lv_btn_create(pw_row))
  , kb(lv_keyboard_create(cont))
  , static_ip_panel(l)
  , network_detail_panel(l,
      [this]() { open_static_ip_for_current(); },
      [this]() { forget_current_network(); })
{
  Navegacao::subtela(cont);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICKABLE);
  // tocar fora do campo/teclado enquanto digita a senha cancela a senha
  lv_obj_add_event_cb(cont, &WifiPanel::_handle_kb_input, LV_EVENT_CLICKED, this);

  Marca::criar_cabecalho(cont, "Wi-Fi", &WifiPanel::_handle_back_btn, this);

  // lista das redes
  Marca::cartao(wifi_list_cont);
  lv_obj_set_pos(wifi_list_cont, LISTA_X, LISTA_Y);
  lv_obj_set_size(wifi_list_cont, WifiRowItem::LARGURA, LISTA_A);
  lv_obj_add_flag(wifi_list_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(wifi_list_cont, LV_DIR_VER);
  lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scrollbar_mode(wifi_list_cont, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(wifi_list_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(wifi_list_cont, 0, 0);
  lv_obj_add_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);

  // estado da ligação
  Marca::cartao(wifi_right);
  lv_obj_set_pos(wifi_right, ESTADO_X, LISTA_Y);
  lv_obj_set_size(wifi_right, ESTADO_L, LISTA_A);

  lv_obj_set_style_text_font(wifi_label, Marca::f14(), 0);
  lv_obj_set_style_text_color(wifi_label, lv_color_hex(Marca::PAPEL), 0);
  lv_label_set_text(wifi_label, "");

  // campo da senha + olho
  lv_obj_set_pos(pw_row, 8, SENHA_Y);
  lv_obj_set_size(pw_row, 408, Marca::TOQUE);
  lv_obj_set_style_border_width(pw_row, 0, 0);
  lv_obj_set_style_bg_opa(pw_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(pw_row, 0, 0);
  lv_obj_clear_flag(pw_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(pw_row, LV_OBJ_FLAG_HIDDEN);

  lv_textarea_set_password_mode(password_input, true);
  lv_textarea_set_one_line(password_input, true);
  lv_textarea_set_placeholder_text(password_input, "Senha");
  lv_obj_set_style_text_font(password_input, Marca::f14(), 0);
  lv_obj_set_style_pad_hor(password_input, 12, 0);
  lv_obj_set_style_pad_ver(password_input, (Marca::TOQUE - 16 - 2) / 2, 0);
  lv_obj_set_pos(password_input, 0, 0);
  lv_obj_set_size(password_input, 356, Marca::TOQUE);

  lv_obj_set_pos(eye_btn, 364, 0);
  lv_obj_set_size(eye_btn, Marca::TOQUE, Marca::TOQUE);
  lv_obj_set_style_pad_all(eye_btn, 0, 0);
  // o olho não tira o foco do campo (senão a senha fecharia ao tocar nele)
  lv_obj_clear_flag(eye_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  {
    lv_obj_t *lbl = lv_label_create(eye_btn);
    lv_label_set_text(lbl, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_font(lbl, Marca::f16(), 0);
    lv_obj_center(lbl);
  }
  Marca::estilo_botao(eye_btn, Marca::Tipo::NORMAL);
  lv_obj_add_event_cb(eye_btn, &WifiPanel::_handle_eye_btn, LV_EVENT_CLICKED, this);

  // teclado dentro da subtela
  lv_obj_align(kb, LV_ALIGN_TOP_LEFT, 0, TECLADO_Y);
  lv_obj_set_size(kb, Marca::CONT_L, Marca::TELA_A - TECLADO_Y);
  lv_obj_set_style_text_font(kb, Marca::f14(), LV_PART_ITEMS);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_FOCUSED, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_DEFOCUSED, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_READY, this);
  lv_obj_add_event_cb(password_input, &WifiPanel::_handle_kb_input, LV_EVENT_CANCEL, this);

  modo_senha(false);

  wpa_event.register_callback("WifiPanel",
    [this](const std::string &event) { this->handle_wpa_event(event); });

  wpa_event.start();
}

WifiPanel::~WifiPanel() {
  // as linhas apagam o próprio objeto: primeiro elas, depois a subtela
  wifi_rows.clear();
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void WifiPanel::modo_senha(bool sim) {
  if (sim) {
    lv_obj_add_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_long_mode(wifi_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(wifi_label, 8, LISTA_Y);
    lv_obj_set_size(wifi_label, 408, 16);
    lv_obj_clear_flag(pw_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pw_row, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_long_mode(wifi_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(wifi_label, ESTADO_X + 12, LISTA_Y + 12);
    lv_obj_set_size(wifi_label, ESTADO_L - 24, LV_SIZE_CONTENT);
    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiPanel::foreground() {
  spdlog::trace("wifi panel fg");
  Navegacao::abrir(cont);
  rescan_budget = 4;
  last_scan_count = 0;
  if (!entering_password) {
    lv_label_set_text(wifi_label, "Procurando redes...");
    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  }
  std::string resp = wpa_event.send_command("SCAN");
  if (resp.empty() || resp.find("FAIL") != std::string::npos) {
    lv_label_set_text(wifi_label, "Não consegui procurar as redes.\nEspere um pouco e abra\nesta tela de novo.");
    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void WifiPanel::handle_back_btn(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    spdlog::trace("wifi panel bg");
    if (entering_password) {
      // sai da senha do jeito de sempre (nada é enviado)
      lv_event_send(password_input, LV_EVENT_DEFOCUSED, NULL);
    }
    lv_obj_add_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    Navegacao::fechar(cont);
    if (ao_mudar_rede) ao_mudar_rede();
  }
}

void WifiPanel::rebuild_wifi_rows() {
  wifi_rows.clear();
  lv_obj_clean(wifi_list_cont);

  std::vector<std::pair<std::string, WifiEntry>> known, other;
  for (const auto &kv : wifi_name_db) {
    if (list_networks.count(kv.first)) {
      known.push_back(kv);
    } else {
      other.push_back(kv);
    }
  }
  auto by_signal_desc = [](const std::pair<std::string, WifiEntry> &a,
                            const std::pair<std::string, WifiEntry> &b) {
    return a.second.signal > b.second.signal;
  };
  std::sort(known.begin(), known.end(), by_signal_desc);
  std::sort(other.begin(), other.end(), by_signal_desc);

  auto make_group_label = [&](const char *text) {
    lv_obj_t *l = Marca::criar_rotulo(wifi_list_cont, text, Marca::f12(), Marca::GRAFITE);
    lv_obj_set_style_pad_top(l, 10, 0);
    lv_obj_set_style_pad_bottom(l, 4, 0);
    lv_obj_set_style_pad_left(l, 12, 0);
  };

  if (!known.empty()) {
    make_group_label("REDES SALVAS");
    for (auto &kv : known) {
      bool connected = (kv.first == cur_network);
      auto nid_it = list_networks.find(kv.first);
      std::string nid = nid_it != list_networks.end() ? nid_it->second : "";
      wifi_rows.push_back(std::make_unique<WifiRowItem>(
        wifi_list_cont, kv.first, kv.second.signal, connected, /*saved=*/true, nid,
        [this](const std::string &ssid) { handle_row_activated(ssid); },
        [this](const std::string &ssid, const std::string &nid) { confirm_remove_network(ssid, nid); }));
    }
  }
  if (!other.empty()) {
    make_group_label("OUTRAS REDES POR PERTO");
    for (auto &kv : other) {
      wifi_rows.push_back(std::make_unique<WifiRowItem>(
        wifi_list_cont, kv.first, kv.second.signal, /*connected=*/false, /*saved=*/false, "",
        [this](const std::string &ssid) { handle_row_activated(ssid); },
        [this](const std::string &ssid, const std::string &nid) { confirm_remove_network(ssid, nid); }));
    }
  }
}

void WifiPanel::mostrar_detalhes(const std::string &ssid) {
  auto it = wifi_name_db.find(ssid);
  // sem uma busca recente a tela não sabe o sinal: mostra "--"
  std::string signal_text = "--";
  std::string security_text = "--";
  if (it != wifi_name_db.end()) {
    int signal = it->second.signal;
    signal_text = signal >= -50 ? "Excelente" : signal >= -60 ? "Bom"
                : signal >= -70 ? "Razoável" : "Fraco";
    security_text = it->second.secured ? "Protegida (WPA/WPA2)" : "Aberta (sem senha)";
  }
  auto ip = KUtils::interface_ip(KUtils::get_wifi_interface());
  network_detail_panel.foreground(ssid, signal_text, security_text, ip);
}

void WifiPanel::handle_row_activated(const std::string &ssid) {
  if (!cur_network_ui.empty() && ssid == cur_network_ui) {
    mostrar_detalhes(ssid);
    return;
  }

  selected_network = ssid;
  if (list_networks.count(ssid)) {
    auto nid = list_networks.find(ssid)->second;
    lv_label_set_text(wifi_label, fmt::format("Conectando a {}...", ssid).c_str());
    wpa_event.send_command(fmt::format("SELECT_NETWORK {}", nid));
    wpa_event.send_command("SAVE_CONFIG");
    wpa_event.send_command("REASSOCIATE");
    lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  } else {
    entering_password = true;
    lv_label_set_text(wifi_label, fmt::format("Digite a senha de {}", ssid).c_str());
    lv_event_send(password_input, LV_EVENT_FOCUSED, NULL);
  }
}

void WifiPanel::open_static_ip_for_current() {
  if (cur_network_ui.empty()) return;
  static_ip_panel.foreground(cur_network_ui);
}

void WifiPanel::forget_current_network() {
  if (cur_network_ui.empty()) return;
  auto it = list_networks.find(cur_network_ui);
  if (it == list_networks.end()) return;
  confirm_remove_network(cur_network_ui, it->second);
}

std::string WifiPanel::rede_atual() {
  return cur_network_ui;
}

void WifiPanel::abrir_detalhes() {
  if (cur_network_ui.empty()) {
    foreground();
    return;
  }
  mostrar_detalhes(cur_network_ui);
}

void WifiPanel::abrir_ip_fixo() {
  if (cur_network_ui.empty()) {
    foreground();
    return;
  }
  static_ip_panel.foreground(cur_network_ui);
}

void WifiPanel::confirm_remove_network(const std::string &ssid, const std::string &nid) {
  static const char *btns[] = {"Esquecer", "Cancelar", ""};
  std::string msg = fmt::format("Esquecer a rede \"{}\"?\nIsso apaga a senha salva.", ssid);
  lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, msg.c_str(), btns, false);
  lv_obj_add_event_cb(mbox, [](lv_event_t *ev) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(ev));
    auto *self = static_cast<WifiPanel *>(lv_event_get_user_data(ev));
    bool confirmed = lv_msgbox_get_active_btn(obj) == 0;
    std::string nid = self->pending_remove_nid;
    self->pending_remove_nid.clear();
    lv_msgbox_close(obj);
    if (!confirmed) return;

    self->wpa_event.send_command(fmt::format("REMOVE_NETWORK {}", nid));
    self->wpa_event.send_command("SAVE_CONFIG");
    self->cur_network.clear();
    self->cur_network_ui.clear();
    self->selected_network.clear();
    self->network_detail_panel.hide();
    lv_obj_add_flag(self->wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(self->wifi_label, "Procurando redes...");
    lv_obj_clear_flag(self->prompt_cont, LV_OBJ_FLAG_HIDDEN);
    self->wpa_event.send_command("SCAN");
    if (self->ao_mudar_rede) self->ao_mudar_rede();
  }, LV_EVENT_VALUE_CHANGED, this);
  pending_remove_nid = nid;
  KUtils::style_lock_mbox(mbox, 90);
}

void WifiPanel::handle_wpa_event(const std::string &event) {
  if (entering_password) {
    return;
  }
  if (event.rfind("<3>CTRL-EVENT-SCAN-RESULTS", 0) == 0) {
    // result ready
    spdlog::trace("got scan result event");
    std::istringstream f(wpa_event.send_command("SCAN_RESULTS"));
    std::string line;
    // Networks actually reported by THIS scan - merged into wifi_name_db
    // below rather than replacing it outright, and anything missing gets
    // aged out gradually instead of vanishing after a single noisy scan.
    std::map<std::string, WifiEntry> seen_this_scan;

    bool found = find_current_network();
    spdlog::trace("cur_network {}", cur_network);

    std::lock_guard<std::mutex> lock(lv_lock);
    cur_network_ui = cur_network;
    if (!found) {
      lv_label_set_text(wifi_label, "Escolha sua rede Wi-Fi");
    }
    while (std::getline(f, line)) {
      if (line.rfind("bss", 0) == 0) {
        continue;
      }

      auto wifi_parts = KUtils::split(line, '\t');
      spdlog::trace("wifi parts {}", fmt::join(wifi_parts, ", "));
      if (wifi_parts.size() == 5) {
        WifiEntry entry;
        entry.signal = std::stoi(wifi_parts[2]);
        entry.secured = wifi_parts[3].find("WPA") != std::string::npos
                      || wifi_parts[3].find("WEP") != std::string::npos;
        seen_this_scan[wifi_parts[4]] = entry;
      }
    }

    for (const auto &kv : seen_this_scan) {
      auto &e = wifi_name_db[kv.first];
      e.signal = kv.second.signal;
      e.secured = kv.second.secured;
      e.missed = 0;
    }
    static constexpr int MAX_MISSED_SCANS = 3;
    for (auto it = wifi_name_db.begin(); it != wifi_name_db.end();) {
      if (seen_this_scan.count(it->first)) {
        ++it;
        continue;
      }
      if (++it->second.missed > MAX_MISSED_SCANS) {
        it = wifi_name_db.erase(it);
      } else {
        ++it;
      }
    }

    if (!cur_network.empty()) {
      auto ip = KUtils::interface_ip(KUtils::get_wifi_interface());
      lv_label_set_text(wifi_label, fmt::format("Conectado a {}\nIP: {}",
        cur_network, ip).c_str());
      lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
    }

    rebuild_wifi_rows();
    lv_obj_scroll_to_y(wifi_list_cont, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);

    size_t current_count = wifi_name_db.size();
    bool grew = current_count > last_scan_count;
    last_scan_count = current_count;
    if (grew && rescan_budget > 0) {
      rescan_budget--;
      wpa_event.send_command("SCAN");
    }
  } else if (event.rfind("<3>CTRL-EVENT-CONNECTED", 0) == 0) {
    // The driver resets the WiFi low-latency knobs (PM/mpc/roam_off) to their
    // defaults on every link-up (reconnect or network switch), silently
    // reverting them. Re-apply the whole bundle here so the toggle stays
    // truthful. Runs on the wpa monitor thread (no LVGL).
    auto pm = Config::get_instance()->get_json("/wifi_low_latency");
    if (!pm.is_null() && pm.template get<bool>()) {
      // BCM4343 firmware resets PM to its NVRAM default after finalising
      // association, which happens slightly after CONNECTED fires.  A short
      // delay ensures our PM=0 lands after that reset.
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      KUtils::set_wifi_low_latency(true);
    }

    if (find_current_network()) {
      spdlog::trace("cur_network {}", cur_network);
      std::lock_guard<std::mutex> lock(lv_lock);
      cur_network_ui = cur_network;

      lv_label_set_text(wifi_label,
        fmt::format("Conectado a {}\nObtendo o IP...", cur_network).c_str());
      lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);

      rebuild_wifi_rows();
      lv_obj_scroll_to_y(wifi_list_cont, 0, LV_ANIM_OFF);
      lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);

      auto iface = KUtils::get_wifi_interface();
      auto net = cur_network;
      uint32_t gen = ++conn_gen;
      std::thread([this, iface, net, gen]() {
        wait_for_connectivity(iface, net, gen);
      }).detach();
    }
  }
}

void WifiPanel::handle_kb_input(lv_event_t *e)
{
  const lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_FOCUSED) {
    if (!entering_password) return;
    lv_keyboard_set_textarea(kb, password_input);
    // The keyboard covers the lower part of the sub-screen - the network list
    // and the status card are hidden while typing (they're not usable then).
    modo_senha(true);
  } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL) {
    if (!entering_password) return;
    entering_password = false;
    lv_keyboard_set_textarea(kb, NULL);
    lv_label_set_text(wifi_label, "Escolha sua rede Wi-Fi");
    lv_obj_clear_state(password_input, LV_STATE_FOCUSED);
    pw_visible = false;
    lv_textarea_set_password_mode(password_input, true);
    lv_label_set_text(lv_obj_get_child(eye_btn, 0), LV_SYMBOL_EYE_CLOSE);
    modo_senha(false);
    lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_READY) {
    const char *password = lv_textarea_get_text(password_input);
    if (password == NULL || password[0] == 0) {
      return;
    }

    entering_password = false;
    // add network, set password, save wpa
    connect(password);
    lv_textarea_set_text(password_input, "");
    lv_keyboard_set_textarea(kb, NULL);
    lv_label_set_text(wifi_label, fmt::format("Conectando a {}...", selected_network).c_str());
    lv_obj_clear_state(password_input, LV_STATE_FOCUSED);
    pw_visible = false;
    lv_textarea_set_password_mode(password_input, true);
    lv_label_set_text(lv_obj_get_child(eye_btn, 0), LV_SYMBOL_EYE_CLOSE);
    modo_senha(false);
    lv_obj_clear_flag(wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_CLICKED) {
    // toque no fundo da subtela (fora do campo e do teclado)
    lv_obj_t *target = lv_event_get_target(e);
    if (entering_password && target == cont) {
      lv_event_send(password_input, LV_EVENT_DEFOCUSED, NULL);
    }
  }
}

void WifiPanel::wait_for_connectivity(const std::string &iface,
                                       const std::string &net,
                                       uint32_t gen) {
  std::string gw;
  // Poll up to 10s (40 × 250ms): first wait for the default route to appear
  // (signals DHCP completed), then for the gateway ARP entry to complete
  // (signals layer-3 traffic can actually flow).
  for (int i = 0; i < 40; i++) {
    if (conn_gen.load() != gen) return;
    if (gw.empty())
      gw = KUtils::gateway_ip(iface);
    if (!gw.empty() && KUtils::gateway_in_arp(gw))
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  if (conn_gen.load() != gen || entering_password) return;
  auto ip = KUtils::interface_ip(iface);
  std::lock_guard<std::mutex> lock(lv_lock);
  lv_label_set_text(wifi_label,
    fmt::format("Conectado a {}\nIP: {}", net, ip).c_str());
}

void WifiPanel::handle_eye_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_event_stop_bubbling(e);
  pw_visible = !pw_visible;
  lv_textarea_set_password_mode(password_input, !pw_visible);
  lv_label_set_text(lv_obj_get_child(eye_btn, 0),
                    pw_visible ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
}

void WifiPanel::connect(const char *password) {
  std::string nid = wpa_event.send_command("ADD_NETWORK");
  spdlog::trace("add_nework {}", nid);
  if (nid.length() > 0) {
    wpa_event.send_command(fmt::format("SET_NETWORK {} ssid {:?}", nid, selected_network));
    wpa_event.send_command(fmt::format("SET_NETWORK {} psk {:?}", nid, password));
    wpa_event.send_command(fmt::format("ENABLE_NETWORK {}", nid));
    wpa_event.send_command(fmt::format("SELECT_NETWORK {}", nid));
    wpa_event.send_command("SAVE_CONFIG");
  }
}

bool WifiPanel::find_current_network() {
  list_networks.clear();
  std::string nets = wpa_event.send_command("LIST_NETWORKS");
  spdlog::trace("nets = {}", nets);
  std::istringstream f(nets);
  std::string line;
  bool found = false;
  while (std::getline(f, line)) {
    auto wifi_parts = KUtils::split(line, '\t');
    if (wifi_parts.size() == 4 && line.find("[CURRENT]") != std::string::npos) {
      cur_network = wifi_parts[1];
      list_networks.insert({wifi_parts[1], wifi_parts[0]});
      found = true;
    }

    if (wifi_parts.size() > 1) {
      list_networks.insert({wifi_parts[1], wifi_parts[0]});
    }
  }

  return found;
}
