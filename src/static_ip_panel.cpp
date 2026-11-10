#include "static_ip_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <arpa/inet.h>
#include <thread>

// Subtela 424 x 272 (coordenadas da subtela):
//   cabeçalho "IP fixo" + nome da rede à direita
//   esquerda: 4 endereços em (8,52) 248 x 168 (linhas de 42)
//             "Voltar ao automático (DHCP)" (8,224) 248 x 44 (só com IP fixo ativo)
//   direita:  segmentado Automático/Manual (264,52) 152 x 88
//             estado f12 (264,148) 152 x 68
//             "Salvar" (264,220) 152 x 44 (só no Manual)
//             teclado de números por cima da coluna da direita (264,52) 152 x 212
namespace {

constexpr lv_coord_t COL_X = 264, COL_L = 152;
constexpr lv_coord_t CAMPOS_X = 8, CAMPOS_Y = 52, CAMPOS_L = 248;
constexpr lv_coord_t LINHA_CAMPO = 42;

const char *STATIC_IP_SCRIPT = "/usr/data/printer_data/config/GuppyScreen/scripts/static_ip.py";

std::string shell_quote(const std::string &s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  out += "'";
  return out;
}

bool parse_ipv4(const std::string &s, uint32_t &out) {
  struct in_addr addr;
  if (inet_pton(AF_INET, s.c_str(), &addr) != 1) return false;
  out = ntohl(addr.s_addr);
  return true;
}

// Mirrors static_ip.py's validate_entry() - keep both in sync if either changes.
std::string validate_static_entry(const std::string &ip_s,
                                   const std::string &netmask_s,
                                   const std::string &gateway_s) {
  uint32_t ip, netmask, gateway;
  if (!parse_ipv4(ip_s, ip)) return "Endereço IP inválido.";
  if (!parse_ipv4(netmask_s, netmask)) return "Máscara de rede inválida.";
  uint32_t inv = ~netmask;
  if ((inv & (inv + 1)) != 0) return "Máscara de rede inválida. A mais comum é 255.255.255.0.";
  if (!parse_ipv4(gateway_s, gateway)) return "Endereço do gateway inválido.";

  uint32_t network = ip & netmask;
  uint32_t broadcast = network | inv;

  if ((gateway & netmask) != network) return fmt::format("O gateway {} está fora da sub-rede (máscara {}).", gateway_s, netmask_s);
  if (ip == gateway) return fmt::format("{} é o endereço do gateway. Escolha outro IP.", ip_s);
  if (ip == network) return fmt::format("{} é o endereço da rede. Escolha outro IP.", ip_s);
  if (ip == broadcast) return fmt::format("{} é o endereço de broadcast. Escolha outro IP.", ip_s);
  return "";
}

std::string json_str(const json &j, const char *key) {
  if (!j.contains(key) || j[key].is_null()) return "";
  return j[key].template get<std::string>();
}

std::vector<std::string> split_dots(const std::string &s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == '.') { out.push_back(cur); cur.clear(); }
    else cur += c;
  }
  out.push_back(cur);
  return out;
}

void clear_octet_error(lv_obj_t *ta) {
  lv_obj_set_style_border_width(ta, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(ta, lv_color_hex(Marca::PRETO_3), LV_PART_MAIN);
  lv_obj_set_style_border_color(ta, lv_color_hex(Marca::LIMAO), LV_PART_MAIN | LV_STATE_FOCUSED);
}

void mark_octet_error(lv_obj_t *ta) {
  lv_obj_set_style_border_width(ta, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(ta, lv_color_hex(Marca::ERRO), LV_PART_MAIN);
}

// One address = a label + 4 small octet boxes + 3 dot separators, sharing a
// digit-only keypad. Replaces a single free-text field per address - fewer
// characters to type on a shared numeric pad, and each box can be flagged
// invalid individually (see mark_octet_error) instead of only via one status
// line shared by all four fields.
void make_octet_row(lv_obj_t *parent, const char *label_text, lv_obj_t *out[4]) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, CAMPOS_L, LINHA_CAMPO);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 2, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *lbl = Marca::criar_rotulo(row, label_text, Marca::f12(), Marca::GRAFITE);
  lv_obj_set_width(lbl, 58);

  for (int i = 0; i < 4; i++) {
    if (i > 0) {
      Marca::criar_rotulo(row, ".", Marca::f14(), Marca::GRAFITE);
    }
    lv_obj_t *ta = lv_textarea_create(row);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 3);
    lv_obj_set_style_text_font(ta, Marca::f14(), 0);
    lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_hor(ta, 2, 0);
    lv_obj_set_style_pad_ver(ta, (38 - 16 - 2) / 2, 0);   // f14 centrado na caixa de 38
    lv_obj_set_size(ta, 40, 38);
    // a linha só tem 42 px: a caixa fica com 38, mas o toque pega 44 de altura
    lv_obj_set_ext_click_area(ta, 3);
    lv_obj_clear_flag(ta, LV_OBJ_FLAG_SCROLL_ELASTIC);
    clear_octet_error(ta);
    out[i] = ta;
  }
}

std::string join_octets(lv_obj_t *const octets[4]) {
  std::string out;
  for (int i = 0; i < 4; i++) {
    if (i) out += '.';
    const char *t = lv_textarea_get_text(octets[i]);
    out += (t && t[0]) ? t : "";
  }
  return out;
}

void set_octets(lv_obj_t *const octets[4], const std::string &dotted) {
  auto parts = split_dots(dotted);
  for (int i = 0; i < 4; i++) {
    lv_textarea_set_text(octets[i], (size_t)i < parts.size() ? parts[i].c_str() : "");
  }
}

// Returns true if every box in the group holds a valid 0-255 integer;
// invalid boxes are marked red in place via mark_octet_error.
bool check_octet_group(lv_obj_t *const group[4]) {
  bool ok = true;
  for (int i = 0; i < 4; i++) {
    const char *t = lv_textarea_get_text(group[i]);
    int v = -1;
    if (t && t[0]) {
      try { v = std::stoi(t); } catch (...) { v = -1; }
    }
    if (v < 0 || v > 255) {
      mark_octet_error(group[i]);
      ok = false;
    }
  }
  return ok;
}

// item do segmentado vertical Automático/Manual
void pintar_modo(lv_obj_t *btn, bool escolhido) {
  Marca::estilo_botao(btn, escolhido ? Marca::Tipo::DESTAQUE : Marca::Tipo::FANTASMA);
  if (!escolhido) {
    lv_obj_set_style_text_color(btn, lv_color_hex(Marca::GRAFITE), 0);
  }
}

lv_obj_t *criar_item_modo(lv_obj_t *pai, const char *texto, lv_coord_t y) {
  lv_obj_t *btn = lv_btn_create(pai);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_set_pos(btn, 0, y);
  lv_obj_set_size(btn, COL_L, Marca::TOQUE);
  lv_obj_t *l = lv_label_create(btn);
  lv_label_set_text(l, texto);
  lv_obj_set_style_text_font(l, Marca::f14(), 0);
  lv_obj_center(l);
  pintar_modo(btn, false);
  return btn;
}

} // namespace

StaticIpPanel::StaticIpPanel(std::mutex &l)
  : lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , title_label(NULL)
  , rede_label(NULL)
  , mode_cont(lv_obj_create(cont))
  , dhcp_mode_btn(criar_item_modo(mode_cont, "Automático (DHCP)", 0))
  , manual_mode_btn(criar_item_modo(mode_cont, "Manual", Marca::TOQUE))
  , revert_btn(Marca::criar_botao(cont, "Voltar ao automático (DHCP)", Marca::Tipo::NORMAL))
  , status_label(lv_label_create(cont))
  , body_cont(lv_obj_create(cont))
  , save_btn(Marca::criar_botao(cont, LV_SYMBOL_OK " Salvar", Marca::Tipo::DESTAQUE))
  , kb(lv_keyboard_create(cont))
{
  Navegacao::subtela(cont);

  Marca::Cabecalho cab = Marca::criar_cabecalho(cont, "IP fixo", &StaticIpPanel::_handle_back_btn, this);
  title_label = cab.titulo;
  rede_label = cab.direita;

  // segmentado vertical: trilho PRETO_2, escolhido limão com texto preto
  lv_obj_set_pos(mode_cont, COL_X, CAMPOS_Y);
  lv_obj_set_size(mode_cont, COL_L, 2 * Marca::TOQUE);
  lv_obj_set_style_bg_color(mode_cont, lv_color_hex(Marca::PRETO_2), 0);
  lv_obj_set_style_bg_opa(mode_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(mode_cont, Marca::RAIO, 0);
  lv_obj_set_style_border_width(mode_cont, 0, 0);
  lv_obj_set_style_pad_all(mode_cont, 0, 0);
  lv_obj_clear_flag(mode_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(dhcp_mode_btn, &StaticIpPanel::_handle_mode_toggle, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(manual_mode_btn, &StaticIpPanel::_handle_mode_toggle, LV_EVENT_CLICKED, this);

  // Only shown while a static config is actually active for this SSID -
  // reverting it is now its own explicit action, never a side effect of
  // switching to the DHCP tab just to look at it (that tab is a pure view
  // switch, see handle_mode_toggle).
  lv_obj_set_pos(revert_btn, CAMPOS_X, 224);
  lv_obj_set_size(revert_btn, CAMPOS_L, Marca::TOQUE);
  lv_obj_add_event_cb(revert_btn, &StaticIpPanel::_handle_revert_btn, LV_EVENT_CLICKED, this);
  lv_obj_add_flag(revert_btn, LV_OBJ_FLAG_HIDDEN);

  // os 4 endereços
  lv_obj_set_pos(body_cont, CAMPOS_X, CAMPOS_Y);
  lv_obj_set_size(body_cont, CAMPOS_L, 4 * LINHA_CAMPO);
  lv_obj_clear_flag(body_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(body_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_border_width(body_cont, 0, 0);
  lv_obj_set_style_bg_opa(body_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(body_cont, 0, 0);
  lv_obj_set_style_pad_row(body_cont, 0, 0);
  lv_obj_set_flex_flow(body_cont, LV_FLEX_FLOW_COLUMN);

  make_octet_row(body_cont, "IP", ip_octets);
  make_octet_row(body_cont, "Máscara", netmask_octets);
  make_octet_row(body_cont, "Gateway", gateway_octets);
  make_octet_row(body_cont, "DNS", dns_octets);

  for (lv_obj_t *const *group : {ip_octets, netmask_octets, gateway_octets, dns_octets}) {
    for (int i = 0; i < 4; i++) {
      lv_obj_add_event_cb(group[i], &StaticIpPanel::_handle_kb_input, LV_EVENT_FOCUSED, this);
      lv_obj_add_event_cb(group[i], &StaticIpPanel::_handle_kb_input, LV_EVENT_DEFOCUSED, this);
      lv_obj_add_event_cb(group[i], &StaticIpPanel::_handle_kb_input, LV_EVENT_READY, this);
      lv_obj_add_event_cb(group[i], &StaticIpPanel::_handle_kb_input, LV_EVENT_CANCEL, this);
    }
  }

  lv_obj_set_pos(status_label, COL_X, 148);
  lv_obj_set_size(status_label, COL_L, 68);
  lv_label_set_long_mode(status_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(status_label, Marca::f12(), 0);
  lv_obj_set_style_text_color(status_label, lv_color_hex(Marca::GRAFITE), 0);
  lv_label_set_text(status_label, "");

  lv_obj_set_pos(save_btn, COL_X, 220);
  lv_obj_set_size(save_btn, COL_L, Marca::TOQUE);
  lv_obj_add_event_cb(save_btn, &StaticIpPanel::_handle_save_btn, LV_EVENT_CLICKED, this);

  // Digits + backspace + OK only - no "." key. Each box only ever holds one
  // octet, so there's nothing for a dot key to do here.
  static const char *kb_map[] = {"1", "2", "3", "\n",
                                 "4", "5", "6", "\n",
                                 "7", "8", "9", "\n",
                                 LV_SYMBOL_BACKSPACE, "0", LV_SYMBOL_OK, ""};
  static const lv_btnmatrix_ctrl_t kb_ctrl[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_style_text_font(kb, Marca::f20(), LV_PART_ITEMS);
  // Over the right column (segmented, status and Save aren't needed while
  // typing), beside the field rows - the focused box always stays visible.
  lv_obj_align(kb, LV_ALIGN_TOP_LEFT, COL_X, CAMPOS_Y);
  lv_obj_set_size(kb, COL_L, Marca::TELA_A - CAMPOS_Y - 8);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

StaticIpPanel::~StaticIpPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void StaticIpPanel::foreground(const std::string &new_ssid) {
  ssid = new_ssid;
  lv_label_set_text(rede_label, ssid.c_str());
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  Navegacao::abrir(cont);
  refresh_status();
}

void StaticIpPanel::handle_back_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  lv_keyboard_set_textarea(kb, NULL);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  Navegacao::fechar(cont);
}

void StaticIpPanel::set_manual_mode(bool manual) {
  manual_mode = manual;
  pintar_modo(manual_mode_btn, manual);
  pintar_modo(dhcp_mode_btn, !manual);

  for (lv_obj_t *const *group : {ip_octets, netmask_octets, gateway_octets, dns_octets}) {
    for (int i = 0; i < 4; i++) {
      if (manual) {
        lv_obj_clear_state(group[i], LV_STATE_DISABLED);
      } else {
        lv_obj_add_state(group[i], LV_STATE_DISABLED);
      }
    }
  }

  if (manual) {
    lv_obj_clear_flag(save_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(save_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

void StaticIpPanel::update_revert_visibility() {
  if (has_static_config) {
    lv_obj_clear_flag(revert_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(revert_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

void StaticIpPanel::set_status(const std::string &msg, bool destaque) {
  lv_obj_set_style_text_color(status_label,
    lv_color_hex(destaque ? Marca::PAPEL : Marca::GRAFITE), 0);
  lv_label_set_text(status_label, msg.c_str());
}

void StaticIpPanel::set_busy(bool b, const std::string &msg) {
  busy = b;
  if (b) {
    // sem girador: a frase de espera já diz o que está acontecendo
    set_status(msg, false);
  }
}

void StaticIpPanel::run_backend_async(const std::string &args, const std::string &busy_msg,
                                       const std::function<void(int, const std::string &)> &on_done) {
  // No lock here (unlike the detached thread below) - this always runs
  // synchronously from a button/dialog click callback, which LVGL invokes
  // from inside lv_timer_handler() while the main loop already holds
  // lv_lock (see GuppyScreen::loop()). Re-locking here on the same thread
  // self-deadlocks a plain std::mutex instantly - confirmed live 2026-07-12,
  // froze the whole screen (touch input kept working at the hardware level,
  // but the main thread never returned to release its own lock). Matches
  // the same (correct) pattern already used in refresh_status().
  set_busy(true, busy_msg);
  std::string cmd = fmt::format("python3 {} {}", STATIC_IP_SCRIPT, args);
  std::thread([this, cmd, on_done]() {
    auto result = KUtils::exec_capture(cmd);
    std::lock_guard<std::mutex> lock(lv_lock);
    set_busy(false, "");
    on_done(result.first, result.second);
  }).detach();
}

void StaticIpPanel::refresh_status() {
  set_busy(true, "Lendo a configuração atual da rede...");
  std::string cmd = fmt::format("python3 {} status", STATIC_IP_SCRIPT);
  std::string ssid_copy = ssid;
  std::thread([this, cmd, ssid_copy]() {
    auto result = KUtils::exec_capture(cmd);
    int rc = result.first;
    std::string output = result.second;
    std::lock_guard<std::mutex> lock(lv_lock);
    set_busy(false, "");

    if (rc != 0) {
      // Fail safe: without a known state, default to DHCP-mode appearance -
      // fields disabled, Save hidden - rather than leaving both mode buttons
      // and the Save button in their as-constructed (both-active) state.
      has_static_config = false;
      set_manual_mode(false);
      update_revert_visibility();
      set_status(fmt::format("Não foi possível ler a configuração da rede:\n{}", output), true);
      return;
    }

    try {
      json j = json::parse(output);
      json live = j.value("live", json::object());

      set_octets(ip_octets, json_str(live, "ip"));
      set_octets(netmask_octets, json_str(live, "netmask"));
      set_octets(gateway_octets, json_str(live, "gateway"));

      json dns_arr = live.value("dns", json::array());
      std::string dns_val = (dns_arr.is_array() && !dns_arr.empty()) ? dns_arr[0].template get<std::string>() : "";
      set_octets(dns_octets, dns_val);

      json configured = j.value("configured", json::object());
      has_static_config = configured.contains(ssid_copy);
      set_manual_mode(has_static_config);
      update_revert_visibility();

      set_status(fmt::format("Agora: {}, endereço {}", has_static_config ? "IP fixo" : "automático (DHCP)",
                             json_str(live, "ip")), false);
    } catch (const std::exception &ex) {
      set_status(fmt::format("Não foi possível interpretar a configuração da rede: {}", ex.what()), true);
    }
  }).detach();
}

void StaticIpPanel::handle_mode_toggle(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED || busy) return;
  lv_obj_t *target = lv_event_get_current_target(e);
  // Pure view switch, nothing else - reverting a live static config is
  // handle_revert_btn's job now, never a side effect of tapping a tab.
  set_manual_mode(target == manual_mode_btn);
}

void StaticIpPanel::handle_revert_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED || busy) return;

  static const char *btns[] = {"Voltar ao automático", "Cancelar", ""};
  std::string msg = fmt::format("Passar a rede \"{}\" para o automático?\nIsso apaga o IP fixo.", ssid);
  lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, msg.c_str(), btns, false);
  lv_obj_add_event_cb(mbox, [](lv_event_t *ev) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(ev));
    auto *self = static_cast<StaticIpPanel *>(lv_event_get_user_data(ev));
    bool confirmed = lv_msgbox_get_active_btn(obj) == 0;
    lv_msgbox_close(obj);
    if (!confirmed) return;

    self->run_backend_async(fmt::format("remove {}", shell_quote(self->ssid)),
                             "Voltando ao automático (DHCP)...",
                             [self](int rc, const std::string &output) {
      if (rc == 0) {
        self->has_static_config = false;
        self->set_manual_mode(false);
        self->update_revert_visibility();
        self->refresh_status();
      } else {
        self->set_status(fmt::format("Falha ao voltar ao automático:\n{}", output), true);
      }
    });
  }, LV_EVENT_VALUE_CHANGED, this);
  KUtils::style_lock_mbox(mbox, 90);
}

void StaticIpPanel::handle_save_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED || busy) return;

  for (lv_obj_t *const *group : {ip_octets, netmask_octets, gateway_octets, dns_octets}) {
    for (int i = 0; i < 4; i++) clear_octet_error(group[i]);
  }

  bool ip_ok = check_octet_group(ip_octets);
  bool netmask_ok = check_octet_group(netmask_octets);
  bool gateway_ok = check_octet_group(gateway_octets);
  bool dns_ok = check_octet_group(dns_octets);

  if (!ip_ok || !netmask_ok || !gateway_ok || !dns_ok) {
    set_status("Digite um número de 0 a 255 nas caixas em vermelho.", true);
    return;
  }

  std::string ip = join_octets(ip_octets);
  std::string netmask = join_octets(netmask_octets);
  std::string gateway = join_octets(gateway_octets);
  std::string dns = join_octets(dns_octets);

  std::string err = validate_static_entry(ip, netmask, gateway);
  if (!err.empty()) {
    set_status(err, true);
    return;
  }

  std::string args = fmt::format("set {} {} {} {} {}", shell_quote(ssid), ip, netmask, gateway, dns);
  run_backend_async(args, "Aplicando o IP fixo...", [this](int rc, const std::string &output) {
    if (rc == 0) {
      set_status("IP fixo aplicado.", true);
      has_static_config = true;
      update_revert_visibility();
    } else {
      set_status(fmt::format("Falha ao aplicar:\n{}", output), true);
    }
  });
}

void StaticIpPanel::handle_kb_input(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *target = lv_event_get_target(e);

  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(kb, target);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    // Land on the end of whatever's already there (e.g. tapping "137" to
    // change just the last digit), not wherever the tap happened to land -
    // confirmed live 2026-07-12 this wasn't happening by default.
    lv_textarea_set_cursor_pos(target, LV_TEXTAREA_CURSOR_LAST);
  } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL) {
    // Unlike READY below, nothing here cleared LV_STATE_FOCUSED - a box
    // tapped into and then tapped away from (rather than dismissed via the
    // keyboard's own OK) kept the theme's focused-border styling
    // permanently, making its effective size subtly different from a
    // never-touched box in the same row. Confirmed live 2026-07-12: edited
    // octet boxes visibly misaligned against their neighbors afterward.
    lv_obj_clear_state(target, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(kb, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  } else if (code == LV_EVENT_READY) {
    lv_obj_clear_state(target, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(kb, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }
}
