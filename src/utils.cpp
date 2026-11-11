#include "hv/requests.h"
#include "hv/hurl.h"
#include "utils.h"
#include "config.h"
#include "state.h"
#include "websocket_client.h"
#include "spdlog/spdlog.h"
#include "platform.h"
#include "marca.h"
#include "lvgl/lvgl.h"

#include <cmath>
#include <time.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <linux/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <experimental/filesystem>
#include <regex>

namespace fs = std::experimental::filesystem;

namespace KUtils {

  bool is_homed() {
    auto v = State::get_instance()
      ->get_data("/printer_state/toolhead/homed_axes"_json_pointer);
    if (!v.is_null()) {
      std::string homed_axes = v.template get<std::string>();
      return homed_axes.find("x") != std::string::npos
        && homed_axes.find("y") != std::string::npos
        && homed_axes.find("z") != std::string::npos;
    }
    return false;
  }

  // The open homing prompt (or NULL). Static so the dedup spans every caller -
  // a panel guard and a Klipper-error path won't stack two identical modals.
  static lv_obj_t *homing_prompt_mbox = NULL;

  static void homing_prompt_cb(lv_event_t *e) {
    auto *ws = (KWebSocketClient *)lv_event_get_user_data(e);
    lv_obj_t *mbox = lv_obj_get_parent(lv_event_get_target(e));
    if (lv_msgbox_get_active_btn(mbox) == 0) {  // "Home"
      ws->gcode_script("G28");
    }
    homing_prompt_mbox = NULL;
    lv_msgbox_close(mbox);
  }

  void show_homing_prompt(KWebSocketClient &ws) {
    if (homing_prompt_mbox != NULL) {
      return;  // already open - don't stack
    }
    static const char *btns[] = {"Ir à origem", "Cancelar", ""};
    homing_prompt_mbox = lv_msgbox_create(NULL, "Ir à origem primeiro",
      "Leve a impressora à origem antes.", btns, false);
    style_lock_mbox(homing_prompt_mbox, 90);  // same card + blue buttons as other dialogs
    lv_obj_add_event_cb(homing_prompt_mbox, homing_prompt_cb, LV_EVENT_VALUE_CHANGED, &ws);
  }

  bool is_printing() {
    auto v = State::get_instance()
      ->get_data("/printer_state/print_stats/state"_json_pointer);
    if (!v.is_null()) {
      std::string s = v.template get<std::string>();
      return s == "printing" || s == "paused";
    }
    return false;
  }

  bool is_paused() {
    auto v = State::get_instance()
      ->get_data("/printer_state/print_stats/state"_json_pointer);
    if (!v.is_null()) {
      return v.template get<std::string>() == "paused";
    }
    return false;
  }

  void style_dialog_overlay(lv_obj_t *overlay) {
    // fundo liso e opaco: nada de véu transparente por cima da tela (pesa e
    // fica borrado em 16 bits)
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(Marca::PRETO), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
  }

  void style_dialog_box(lv_obj_t *box) {
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_radius(box, Marca::RAIO, 0);
    lv_obj_set_style_shadow_width(box, 0, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(Marca::PRETO_2), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(box, lv_color_hex(Marca::PAPEL), 0);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  }

  void style_dialog_title(lv_obj_t *title) {
    lv_obj_set_style_text_font(title, Marca::f16(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
  }

  void style_dialog_msgbox(lv_obj_t *mbox) {
    lv_obj_set_style_border_width(mbox, 0, 0);
    lv_obj_set_style_radius(mbox, Marca::RAIO, 0);
    lv_obj_set_style_shadow_width(mbox, 0, 0);
    lv_obj_set_style_bg_color(mbox, lv_color_hex(Marca::PRETO_2), 0);
    lv_obj_set_style_bg_opa(mbox, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(mbox, lv_color_hex(Marca::PAPEL), 0);
    lv_obj_set_style_text_font(mbox, Marca::f14(), 0);

    // botões da caixa: PRETO_3 com texto PAPEL, raio 12, 8 px entre eles
    lv_obj_t *btnm = lv_msgbox_get_btns(mbox);
    if (btnm != NULL) {
      lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(btnm, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(btnm, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_gap(btnm, Marca::VAO, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS);
      lv_obj_set_style_bg_color(btnm, lv_color_hex(Marca::PRETO_3), LV_PART_ITEMS);
      lv_obj_set_style_text_color(btnm, lv_color_hex(Marca::PAPEL), LV_PART_ITEMS);
      lv_obj_set_style_radius(btnm, Marca::RAIO, LV_PART_ITEMS);
      lv_obj_set_style_shadow_width(btnm, 0, LV_PART_ITEMS);
      lv_obj_set_style_bg_color(btnm, lv_color_hex(Marca::APERTADO), LV_PART_ITEMS | LV_STATE_PRESSED);
      lv_obj_set_style_color_filter_opa(btnm, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_PRESSED);
    }
  }

  // Pergunta modal (lv_msgbox_create com pai NULL): o fundo invisível que
  // segura o toque passa a cobrir só a área das páginas (56,0 424x272). A
  // coluna da esquerda, com a parada de emergência, continua respondendo
  // enquanto a pergunta está aberta. A caixa fica centrada nessa área.
  static void liberar_coluna(lv_obj_t *mbox) {
    lv_obj_t *fundo = lv_obj_get_parent(mbox);
    if (fundo != NULL && lv_obj_check_type(fundo, &lv_msgbox_backdrop_class)) {
      lv_obj_set_pos(fundo, Marca::NAV_L, 0);
      lv_obj_set_size(fundo, Marca::CONT_L, Marca::TELA_A);
      lv_obj_center(mbox);
    } else {
      lv_obj_align(mbox, LV_ALIGN_CENTER, Marca::NAV_L / 2, 0);
    }
  }

  // Shared styling so print-lock dialogs match the app's other msgboxes:
  // centered body text and a floating, centered button row at the bottom.
  void style_lock_mbox(lv_obj_t *mbox, lv_coord_t btns_pct) {
    style_dialog_msgbox(mbox);

    // centre the title too (dialogs with a title were left-aligned otherwise)
    lv_obj_t *title = ((lv_msgbox_t *)mbox)->title;
    if (title != NULL) {
      lv_obj_set_width(title, LV_PCT(100));
      lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_text_font(title, Marca::f16(), 0);
    }

    lv_obj_t *msg = ((lv_msgbox_t *)mbox)->text;
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(msg, LV_PCT(100));
    lv_obj_center(msg);

    lv_obj_t *btnm = lv_msgbox_get_btns(mbox);
    lv_obj_add_flag(btnm, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, 0);

    // botões com 44 px de altura (alvo de toque); caixa de 336 x 160
    lv_obj_set_size(btnm, LV_PCT(btns_pct), Marca::TOQUE);
    lv_obj_set_size(mbox, 336, 160);
    liberar_coluna(mbox);
  }

  void notify_locked() {
    static const char *btns[] = {"OK", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL,
      "Indisponível durante a impressão", btns, false);
    lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
      lv_msgbox_close(lv_obj_get_parent(lv_event_get_target(e)));
    }, LV_EVENT_VALUE_CHANGED, NULL);
    style_lock_mbox(mbox, 50);
  }

  void avisar(const std::string &msg) {
    static const char *btns[] = {"OK", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, msg.c_str(), btns, false);
    lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
      lv_msgbox_close(lv_obj_get_parent(lv_event_get_target(e)));
    }, LV_EVENT_VALUE_CHANGED, NULL);
    style_lock_mbox(mbox, 50);
    lv_obj_set_size(mbox, 400, 180);
    liberar_coluna(mbox);
  }

  void notify_toast(const std::string &msg, uint32_t timeout_ms) {
    // Aviso rápido SEM fundo e sem pegar toque: a caixa mora direto em
    // lv_layer_top() (sem o fundo de tela inteira do lv_msgbox) e nenhum
    // pedaço dela é tocável. O toque passa para a tela: a coluna, a parada
    // de emergência e as janelas continuam funcionando enquanto ele aparece.
    lv_obj_t *mbox = lv_msgbox_create(lv_layer_top(), NULL, msg.c_str(), NULL, false);
    style_dialog_msgbox(mbox);
    lv_obj_clear_flag(mbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *conteudo = lv_msgbox_get_content(mbox);
    if (conteudo != NULL) {
      lv_obj_clear_flag(conteudo, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *txt = lv_msgbox_get_text(mbox);
    if (txt != NULL) {
      lv_obj_clear_flag(txt, LV_OBJ_FLAG_CLICKABLE);
      lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
      lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_width(txt, LV_PCT(100));
    }
    // 336 px fixos, centrado na área das páginas (à direita da coluna)
    lv_obj_set_width(mbox, 336);
    lv_obj_set_height(mbox, LV_SIZE_CONTENT);
    lv_obj_align(mbox, LV_ALIGN_CENTER, Marca::NAV_L / 2, 0);

    // Auto-close once after timeout_ms (repeat_count 1 → LVGL deletes the timer).
    lv_timer_t *t = lv_timer_create([](lv_timer_t *tm) {
      lv_obj_t *m = (lv_obj_t *)tm->user_data;
      if (m != NULL) {
        lv_msgbox_close(m);
      }
    }, timeout_ms, mbox);
    lv_timer_set_repeat_count(t, 1);
  }

  // A pergunta aberta (Confirmar / Cancelar). Só uma por vez; trocar de
  // página pela coluna fecha (como Cancelar), para ela não ficar por cima
  // de outra página.
  static lv_obj_t *pergunta_aberta = NULL;

  void fechar_pergunta() {
    if (pergunta_aberta != NULL) {
      lv_obj_t *m = pergunta_aberta;
      pergunta_aberta = NULL;
      lv_msgbox_close(m);   // o LV_EVENT_DELETE apaga a função guardada
    }
  }

  static void criar_pergunta(const std::string &msg, const std::function<void()> &cb) {
    fechar_pergunta();
    auto *heap_cb = new std::function<void()>(cb);
    static const char *btns[] = {"Confirmar", "Cancelar", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, msg.c_str(), btns, false);
    lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
      lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));
      auto *fn = (std::function<void()> *)lv_event_get_user_data(e);
      bool sim = lv_msgbox_get_active_btn(obj) == 0;
      // copia antes de fechar: fechar apaga a caixa e a função guardada
      std::function<void()> acao = (sim && fn != NULL) ? *fn : std::function<void()>();
      if (pergunta_aberta == obj) {
        pergunta_aberta = NULL;
      }
      lv_msgbox_close(obj);
      if (acao) {
        acao();
      }
    }, LV_EVENT_VALUE_CHANGED, heap_cb);
    lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
      if (pergunta_aberta == lv_event_get_target(e)) {
        pergunta_aberta = NULL;
      }
      delete (std::function<void()> *)lv_event_get_user_data(e);
    }, LV_EVENT_DELETE, heap_cb);
    style_lock_mbox(mbox, 90);
    pergunta_aberta = mbox;
  }

  void confirm_if_printing(const std::string &msg, const std::function<void()> &cb) {
    if (!is_printing()) {
      cb();
      return;
    }
    criar_pergunta(msg, cb);
  }

  void confirmar(const std::string &msg, const std::function<void()> &cb) {
    criar_pergunta(msg, cb);
  }

  bool is_running_local() {
    Config *conf = Config::get_instance();
    std::string df_host = conf->get<std::string>(conf->df() + "moonraker_host");
    return df_host == "localhost" || df_host == "127.0.0.1";
  }

  std::string get_root_path(const std::string root_name) {
    auto roots = State::get_instance()->get_data("/roots"_json_pointer);
    json filtered;
    std::copy_if(roots.begin(), roots.end(),
      std::back_inserter(filtered), [&root_name](const json &item) {
        return item.contains("name") && item["name"] == root_name;
      });

    spdlog::trace("roots {}, filtered {}", roots.dump(), filtered.dump());
    if (!filtered.empty()) {
      return filtered["/0/path"_json_pointer];
    }

    return "";
  }

  // Scan one stream for the first firmware-retraction command, reading at most
  // MAX_SCAN bytes and stopping on the first hit. Returns 1 if found, 0 if the
  // bounded prefix held none.
  static int scan_for_fw_retraction(std::istream &in) {
    constexpr size_t MAX_SCAN = 1024 * 1024;  // bounded prefix; we early-exit on a hit
    std::string line;
    size_t read = 0;
    while (read < MAX_SCAN && std::getline(in, line)) {
      read += line.size() + 1;
      size_t i = line.find_first_not_of(" \t");
      if (i == std::string::npos) continue;
      const char *p = line.c_str() + i;
      // G10 / G11 = retract / unretract. Require a non-digit terminator so we
      // don't match G100 etc.
      if ((p[0] == 'G' || p[0] == 'g') && p[1] == '1' && (p[2] == '0' || p[2] == '1')) {
        char c = p[3];
        if (c == '\0' || c == ' ' || c == '\t' || c == ';' || c == '\r') return 1;
      }
      if (line.compare(i, 14, "SET_RETRACTION") == 0) return 1;
    }
    return 0;
  }

  int print_uses_firmware_retraction() {
    // Verdict cached per filename so re-opening the menu during one print does
    // not rescan. -1 = unknown, 0 = no, 1 = yes.
    static std::string cached_file;
    static int cached_result = -1;

    auto fn = State::get_instance()->get_data("/printer_state/print_stats/filename"_json_pointer);
    if (!fn.is_string()) return -1;
    std::string filename = fn.template get<std::string>();
    if (filename.empty()) return -1;
    if (filename == cached_file) return cached_result;

    int result = -1;
    if (is_running_local()) {
      // Running on the printer: read the gcode straight off disk (no download).
      std::string root = get_root_path("gcodes");
      if (!root.empty()) {
        std::ifstream in(root + "/" + filename, std::ios::binary);
        if (in.is_open()) result = scan_for_fw_retraction(in);
      }
    } else {
      // Remote: fetch only the leading prefix with a Range request, never the
      // whole file.
      Config *conf = Config::get_instance();
      std::string url = fmt::format("http://{}:{}/server/files/gcodes/{}",
        conf->get<std::string>(conf->df() + "moonraker_host"),
        conf->get<uint32_t>(conf->df() + "moonraker_port"),
        HUrl::escape(filename));
      http_headers headers;
      headers["Range"] = "bytes=0-1048575";
      auto resp = requests::get(url.c_str(), headers);
      if (resp != NULL && (resp->status_code == 200 || resp->status_code == 206)) {
        std::istringstream iss(resp->body);
        result = scan_for_fw_retraction(iss);
      }
    }

    cached_file = filename;
    cached_result = result;
    spdlog::debug("firmware-retraction gcode scan for '{}': {}", filename, result);
    return result;
  }

  static int thumb_dim(json &thumb, const char *key) {
    return thumb[key].is_number()
      ? thumb[key].template get<int>()
      : std::stoi(thumb[key].template get<std::string>());
  }

  // Baixa (ou acha no disco, rodando na impressora) a miniatura escolhida.
  static std::pair<std::string, std::pair<size_t, size_t>> thumbnail_at(const std::string &gcode_file,
    json &thumbs, uint32_t index) {
    auto &thumb = thumbs.at(index);
    size_t thumb_width = thumb_dim(thumb, "width");
    size_t thumb_height = thumb_dim(thumb, "height");
    spdlog::debug("using thumb at index {}, {}", index, thumbs.dump());

    std::string relative_path = thumb["relative_path"].template get<std::string>();
    size_t found = gcode_file.find_last_of("/\\");
    if (found != std::string::npos) {
      relative_path = gcode_file.substr(0, found + 1) + relative_path;
    }

    Config *conf = Config::get_instance();
    std::string df_host = conf->get<std::string>(conf->df() + "moonraker_host");
    std::string fname = relative_path.substr(relative_path.find_last_of("/\\") + 1);
    std::string fullpath = fmt::format("{}/{}", conf->get<std::string>("/thumbnail_path"), fname);

    if (is_running_local()) {
      spdlog::debug("running locally, skipping thumbnail downloads");
      auto gcode_root = get_root_path("gcodes");
      fullpath = fmt::format("{}/{}", gcode_root, relative_path);
    } else {
      std::string thumb_url = fmt::format("http://{}:{}/server/files/gcodes/{}",
        df_host,
        conf->get<uint32_t>(conf->df() + "moonraker_port"),
        HUrl::escape(relative_path));
      spdlog::debug("thumb url {}", thumb_url);
      auto size = requests::downloadFile(thumb_url.c_str(), fullpath.c_str());
      spdlog::trace("downloaded size {}", size);
    }

    return std::make_pair(fullpath, std::make_pair(thumb_width, thumb_height));
  }

  std::pair<std::string, std::pair<size_t, size_t>> get_thumbnail(const std::string &gcode_file, json &j) {
    auto &thumbs = j["/result/thumbnails"_json_pointer];
    if (!thumbs.is_null() && !thumbs.empty()) {
      // a maior
      uint32_t closest_index = 0;
      int largest = thumb_dim(thumbs.at(0), "width");
      for (uint32_t i = 1; i < thumbs.size(); i++) {
        int width = thumb_dim(thumbs.at(i), "width");
        if (width > largest) {
          largest = width;
          closest_index = i;
        }
      }
      return thumbnail_at(gcode_file, thumbs, closest_index);
    }

    return std::make_pair("", std::make_pair(0, 0));
  }

  std::pair<std::string, std::pair<size_t, size_t>> get_thumbnail(const std::string &gcode_file, json &j,
                                                                  int largura_desejada) {
    auto &thumbs = j["/result/thumbnails"_json_pointer];
    if (!thumbs.is_null() && !thumbs.empty()) {
      int menor_ok = -1;        // menor largura >= desejada
      uint32_t idx_ok = 0;
      int maior = -1;           // maior largura (se nenhuma chega lá)
      uint32_t idx_maior = 0;
      for (uint32_t i = 0; i < thumbs.size(); i++) {
        int width = thumb_dim(thumbs.at(i), "width");
        if (width >= largura_desejada && (menor_ok < 0 || width < menor_ok)) {
          menor_ok = width;
          idx_ok = i;
        }
        if (width > maior) {
          maior = width;
          idx_maior = i;
        }
      }
      return thumbnail_at(gcode_file, thumbs, menor_ok >= 0 ? idx_ok : idx_maior);
    }

    return std::make_pair("", std::make_pair(0, 0));
  }

  std::string download_file(const std::string &root,
    const std::string &fname,
    const std::string &dest) {

    auto filename = fs::path(fname).filename();
    auto dest_fullpath = fs::path(dest) / filename;

    spdlog::trace("root {}, fname {}, base filename {}, dest_fp {}", root, fname,
      filename.string(), dest_fullpath.string());
    Config *conf = Config::get_instance();
    std::string df_host = conf->get<std::string>(conf->df() + "moonraker_host");

    std::string file_url = fmt::format("http://{}:{}/server/files/{}/{}",
      df_host,
      conf->get<uint32_t>(conf->df() + "moonraker_port"),
      root,
      HUrl::escape(fname));
    // threadpool this
    spdlog::debug("file url {}", file_url);
    auto size = requests::downloadFile(file_url.c_str(), dest_fullpath.c_str());
    spdlog::trace("downloaded file size {}", size);

    return dest_fullpath.string();
  }

  std::vector<std::string> get_interfaces() {
    std::vector<std::string> ifaces;
#ifndef OS_ANDROID
    struct ifaddrs *addrs;
    getifaddrs(&addrs);
    for (struct ifaddrs *addr = addrs; addr != nullptr; addr = addr->ifa_next) {
      if (addr->ifa_addr && addr->ifa_addr->sa_family == AF_PACKET) {
        const std::string iface = addr->ifa_name;
        if (!iface.empty() && (iface[0] == 'w' || iface[0] == 'e')) {
          ifaces.push_back(iface);
        }
      }
    }

    freeifaddrs(addrs);
#endif // OS_ANDROID
    return ifaces;
  }

  std::string interface_ip(const std::string &interface) {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    struct ifreq ifr {};
    strcpy(ifr.ifr_name, interface.c_str());
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    char ip[INET_ADDRSTRLEN];
    strcpy(ip, inet_ntoa(((sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    return ip;
  }

  std::string get_wifi_interface() {
    std::string wpa_socket = Config::get_instance()->get<std::string>("/wpa_supplicant");
    if (fs::is_directory(fs::status(wpa_socket))) {
      for (const auto &e : fs::directory_iterator(wpa_socket)) {
        if (fs::is_socket(e.path()) && e.path().string().find("p2p") == std::string::npos) {
          return e.path().filename().string();
        }
      }
    }

    return "";
  }

  // Returns the default-route gateway IP for `iface`, or "" if none.
  // /proc/net/route stores addresses as hex in host byte order.
  std::string gateway_ip(const std::string &iface) {
    std::ifstream f("/proc/net/route");
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
      std::istringstream iss(line);
      std::string dev, dest, gw;
      iss >> dev >> dest >> gw;
      if (dev != iface || dest != "00000000") continue;
      uint32_t gw_val = 0;
      sscanf(gw.c_str(), "%x", &gw_val);
      if (gw_val == 0) continue;
      struct in_addr addr;
      addr.s_addr = htonl(gw_val);
      return inet_ntoa(addr);
    }
    return "";
  }

  // Returns true once the gateway has a completed ARP entry in /proc/net/arp.
  bool gateway_in_arp(const std::string &gw_ip) {
    std::ifstream f("/proc/net/arp");
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
      std::istringstream iss(line);
      std::string ip, hw_type, flags;
      iss >> ip >> hw_type >> flags;
      if (ip != gw_ip) continue;
      int flag_val = 0;
      sscanf(flags.c_str(), "%x", &flag_val);
      return (flag_val & 0x2) != 0; // ATF_COM: entry is complete
    }
    return false;
  }

  template <typename Out>
  void split(const std::string &s, char delim, Out result) {
    std::istringstream iss(s);
    std::string item;
    while (std::getline(iss, item, delim)) {
      *result++ = item;
    }
  }

  std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
  }

  std::string get_obj_name(const std::string &id) {
    size_t pos = id.find_last_of(' ');
    return id.substr(pos + 1);
  }

  std::string to_title(std::string s) {
    bool last = true;
    for (char &c : s) {
      c = last ? std::toupper(c) : std::tolower(c);
      if (c == '_') {
        c = ' ';
      }

      last = std::isspace(c);
    }
    return s;
  }

  std::string fan_display_name(const std::string &key) {
    std::string lower;
    for (char c : key) {
      lower += std::tolower(c);
    }
    if (lower.find("nozzle") != std::string::npos || lower.find("hotend") != std::string::npos
        || lower.find("heatbreak") != std::string::npos || key.rfind("heater_fan ", 0) == 0) {
      return "Ventoinha do bico";
    }
    if (lower.find("mainboard") != std::string::npos || lower.find("board") != std::string::npos) {
      return "Ventoinha da placa-mãe";
    }
    if (lower.find("controller") != std::string::npos || key.rfind("controller_fan ", 0) == 0) {
      return "Ventoinha da eletrônica";
    }
    if (lower.find("part") != std::string::npos || lower.find("fan0") != std::string::npos
        || key == "fan") {
      return "Ventoinha da peça";
    }
    // fallback: title-case the short name, ensure it reads as a fan
    size_t pos = key.find_last_of(' ');
    // pt-BR: tira o "Fan" do nome e sempre põe "Ventoinha" na frente
    // ("fan1" -> "Ventoinha 1", "exhaust_fan" -> "Ventoinha Exhaust").
    std::string base = to_title(pos != std::string::npos ? key.substr(pos + 1) : key);
    size_t f = base.find("Fan");
    if (f != std::string::npos) {
      base.erase(f, 3);
    }
    while (!base.empty() && base.front() == ' ') base.erase(0, 1);
    while (!base.empty() && base.back() == ' ') base.pop_back();
    return base.empty() ? std::string("Ventoinha") : "Ventoinha " + base;
  }


  std::string eta_string(int64_t s) {
    time_t seconds(s);
    tm p;
    gmtime_r(&seconds, &p);

    std::ostringstream os;

    if (p.tm_yday > 0)
      os << p.tm_yday << "d ";

    if (p.tm_hour > 0)
      os << p.tm_hour << "h ";

    if (p.tm_min > 0)
      os << p.tm_min << "min ";

    os << p.tm_sec << "s";

    return os.str();
  }

  size_t bytes_to_mb(size_t s) {
    return s / 1024 / 1024;
  }

  std::string tempo_curto(int64_t s) {
    if (s < 0) {
      s = 0;
    }
    int64_t h = s / 3600;
    int64_t m = (s % 3600) / 60;
    int64_t seg = s % 60;
    if (h > 0) {
      return fmt::format("{}h {:02}min", h, m);
    }
    if (m > 0) {
      return fmt::format("{}min {}s", m, seg);
    }
    return fmt::format("{}s", seg);
  }

  std::string tamanho_arquivo(size_t bytes) {
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    if (bytes >= (size_t)gb) {
      return fmt::format("{:.1f} GB", bytes / gb);
    }
    if (bytes >= (size_t)mb) {
      return fmt::format("{:.1f} MB", bytes / mb);
    }
    size_t k = (size_t)(bytes / kb + 0.5);
    if (k == 0) {
      k = 1;
    }
    return fmt::format("{} KB", k);
  }

  // First /sys/class/backlight/<dev>/ found, or "" if no device. Cached
  // after first scan so we don't hit the FS on every brightness write.
  static const std::string& backlight_dir() {
    static std::string cached;
    static bool searched = false;
    if (!searched) {
      searched = true;
      try {
        if (fs::is_directory("/sys/class/backlight")) {
          for (auto &p : fs::directory_iterator("/sys/class/backlight")) {
            cached = p.path().string();
            break;
          }
        }
      } catch (const std::exception &e) {
        spdlog::debug("backlight scan failed: {}", e.what());
      }
      if (cached.empty()) {
        spdlog::info("no /sys/class/backlight device - brightness control disabled");
      } else {
        spdlog::info("backlight device: {}", cached);
      }
    }
    return cached;
  }

  static int read_int_file(const std::string &path, int fallback) {
    std::ifstream f(path);
    int v = fallback;
    if (f) { f >> v; }
    return v;
  }

  int backlight_max() {
    auto &d = backlight_dir();
#ifdef SIMULATOR
    // No /sys/class/backlight on x86 desktop; pretend we're a KE so the
    // sysinfo slider renders with the same range it'd show on-device.
    if (d.empty()) return 300;
#endif
    if (d.empty()) return 0;
    return read_int_file(d + "/max_brightness", 0);
  }

  // Used by SIMULATOR and as a fallback when sysfs reads fail.
  static int cached_brightness = -1;

  int backlight_get() {
    auto &d = backlight_dir();
    if (d.empty()) {
      return cached_brightness;
    }
    int v = read_int_file(d + "/brightness", -1);
    if (v >= 0) cached_brightness = v;
    return v;
  }

  void backlight_set(int v) {
    if (v < 0) v = 0;
    int mx = backlight_max();
    if (mx > 0 && v > mx) v = mx;

    cached_brightness = v;
    auto &d = backlight_dir();
    if (d.empty()) return;
    std::ofstream f(d + "/brightness");
    if (f) {
      f << v;
    } else {
      spdlog::warn("backlight_set: failed to write {}/brightness", d);
    }
  }

  // Stop or start the Bluetooth stack. On the KE the WiFi chip is a Broadcom
  // BCM4343 single-chip WiFi+BT combo sharing one 2.4GHz radio and antenna; an
  // active BT daemon makes WiFi yield to it (coexistence), injecting latency
  // spikes into WiFi. In low-latency mode we stop bsa_server to give WiFi the
  // radio to itself, and restore it via its boot script otherwise. Guarded so
  // it is a harmless no-op off-device / in the simulator.
  static void set_bluetooth_running(bool running) {
    if (running) {
      if (access("/etc/init.d/S41bt_bsa_download_firmware", X_OK) == 0) {
        int rc = system("/etc/init.d/S41bt_bsa_download_firmware start > /dev/null 2>&1");
        spdlog::info("set_bluetooth_running: start (rc={})", rc);
      }
    } else if (access("/usr/bin/killall", X_OK) == 0) {
      int rc = system("/usr/bin/killall bsa_server > /dev/null 2>&1");
      spdlog::info("set_bluetooth_running: stopped bsa_server (rc={})", rc);
    }
  }

  bool set_wifi_low_latency(bool on) {
    // The Broadcom `wl` tool drives the WiFi knobs; it is absent on other
    // hardware and in the simulator, where that part is a harmless no-op. The
    // driver resets every one of these to its default on each link-up, so this
    // is re-applied on every CONNECTED event and a few times at startup (see
    // wifi_panel / guppyscreen).
    bool have_wl = access("/usr/bin/wl", X_OK) == 0;
    if (have_wl) {
      // Low-latency bundle vs. stock defaults:
      //   PM 0/2       power-save off / fast power-save (buffers -> latency)
      //   mpc 0/1      keep radio powered / let it sleep when idle (wake delay)
      //   roam_off 1/0 no background roam scans / periodic off-channel scans
      // (wme_apsd is intentionally left alone: changing it needs `wl down`,
      //  which would drop the connection.)
      if (on) {
        system("/usr/bin/wl PM 0 > /dev/null 2>&1");
        system("/usr/bin/wl mpc 0 > /dev/null 2>&1");
        system("/usr/bin/wl roam_off 1 > /dev/null 2>&1");
      } else {
        system("/usr/bin/wl PM 2 > /dev/null 2>&1");
        system("/usr/bin/wl mpc 1 > /dev/null 2>&1");
        system("/usr/bin/wl roam_off 0 > /dev/null 2>&1");
      }
      spdlog::info("set_wifi_low_latency: wl bundle {}",
                   on ? "low-latency (PM0/mpc0/roam_off1)" : "stock (PM2/mpc1/roam_off0)");
    } else {
      spdlog::debug("set_wifi_low_latency: /usr/bin/wl not present, skipping wl bundle");
    }

    // Free (low-latency) or restore (stock) the shared 2.4GHz radio via BT.
    set_bluetooth_running(!on);

    return have_wl;
  }

  std::pair<int, std::string> exec_capture(const std::string &cmd) {
    std::string full_cmd = cmd + " 2>&1";
    FILE *pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
      return {-1, "falha ao executar o comando"};
    }
    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
      output += buf;
    }
    int status = pclose(pipe);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {exit_code, output};
  }

  std::map<std::string, std::map<std::string, std::string>> parse_macros(json &m) {
    std::map<std::string, std::map<std::string, std::string>> macros;

    std::regex param_regex(R"(params\.(\w+)(.*))", std::regex_constants::icase);
    std::regex default_value_regex(R"(\|\s*default\s*\(\s*((["'])(?:\\.|[^\x02])*\2|-?[0-9][^,)]*))",
      std::regex_constants::icase);
    for (auto &el : m.items()) {
      std::string key = el.key();
      if (key.rfind("gcode_macro ", 0) == 0) {
        auto &gcode = el.value()["/gcode"_json_pointer];
        if (!gcode.is_null()) {
          auto macro_split = split(el.key(), ' ');
          if (macro_split.size() > 1 && macro_split[1].rfind("_", 0) != 0) {
            std::string macro_name = macro_split[1];

            const auto &gcode_str = gcode.template get<std::string>();
            auto param_begin =
              std::sregex_iterator(gcode_str.begin(), gcode_str.end(), param_regex);
            auto param_end = std::sregex_iterator();

            std::map<std::string, std::string> macro_params;
            for (std::sregex_iterator i = param_begin; i != param_end; ++i) {
              std::smatch match = *i;
              std::string param_name = match.str(1);
              std::string rest = match.str(2);
              std::smatch matches;
              std::string default_value = "";

              spdlog::trace("macro: {}, param; {}, rest: {}", macro_name, param_name, rest);

              if (std::regex_search(rest, matches, default_value_regex)) {
                default_value = matches.str(1);
              }

              macro_params.insert({param_name, default_value});
            }
            macros.insert({macro_name, macro_params});
          }
        }
      }
    }

    return macros;
  }
}  // namespace KUtils
