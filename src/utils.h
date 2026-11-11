#ifndef __K_UTILS_H__
#define __K_UTILS_H__

#include "hv/json.hpp"
#include "lvgl/lvgl.h"
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <utility>

using json = nlohmann::json;

class KWebSocketClient;

namespace KUtils {
  bool is_homed();
  // Actionable "Homing required" modal (Home / Cancel); the Home button sends
  // G28. Shared by the notification manager (on a Klipper "Must home axis first"
  // error) and by panels that guard a homing-dependent action before sending it.
  // Self-deduping: a second call while one is open is a no-op.
  void show_homing_prompt(KWebSocketClient &ws);
  // True while a print job is active (printing or paused). Panels that move axes,
  // home, or run calibration must not act in this state.
  bool is_printing();
  // True only while the print is paused. When paused the toolhead is parked, so
  // jogging/extruding (filament runout, manual M600 colour change) is safe and
  // we let Homing/Extrude through.
  bool is_paused();
  // Modal "Unavailable while printing" notice (single OK button).
  void notify_locked();
  // Transient, auto-dismissing message (no buttons). Used for non-blocking
  // feedback like "Heating, press again when ready".
  void notify_toast(const std::string &msg, uint32_t timeout_ms = 1800);
  // Runs cb immediately when idle; while printing, shows a Confirm/Cancel modal
  // and only runs cb if the user confirms the override.
  void confirm_if_printing(const std::string &msg, const std::function<void()> &cb);
  // Sempre pergunta (Confirmar / Cancelar) e so roda cb se confirmar. Para
  // botoes que mexem o bico ou aquecem (toque por engano nao pode agir).
  void confirmar(const std::string &msg, const std::function<void()> &cb);
  // janela só com OK (informação)
  void avisar(const std::string &msg);
  // fecha a pergunta aberta (se houver) sem fazer a ação
  void fechar_pergunta();
  bool is_running_local();
  std::string get_root_path(const std::string root_name);

  // Does the currently-printing gcode use firmware retraction? Scans only a
  // bounded prefix of the file for G10/G11/SET_RETRACTION and early-exits on
  // the first hit, so it's cheap even on the printer; reads off local disk when
  // running on the host, else a single Range request. Verdict cached per
  // filename. Returns 1 = yes, 0 = no, -1 = couldn't tell (not printing, file
  // missing/unreadable).
  int print_uses_firmware_retraction();

  // ---- Shared dialog look ----------------------------------------------------
  // Visual das janelas na marca (Proposta B). Diálogos feitos à mão (fundo +
  // caixa) usam os três primeiros; os dois últimos pintam um lv_msgbox.
  // Tamanho/alinhamento ficam com cada diálogo; aqui só o visual.
  void style_dialog_overlay(lv_obj_t *overlay);  // fundo da tela inteira, PRETO opaco
  void style_dialog_box(lv_obj_t *box);          // caixa PRETO_2, raio 12, sem borda
  void style_dialog_title(lv_obj_t *title);      // título f16 no alto
  void style_dialog_msgbox(lv_obj_t *mbox);      // caixa + botões da marca num lv_msgbox
  // Caixa completa: título e corpo centralizados e a fileira de botões de 44 px
  // flutuando embaixo (btns_pct = largura da fileira).
  void style_lock_mbox(lv_obj_t *mbox, lv_coord_t btns_pct);

  // path, width
  std::pair<std::string, std::pair<size_t, size_t>> get_thumbnail(const std::string &gcode_file, json &j);
  // Mesma coisa (mesmo retorno e mesmo download), mas escolhe a MENOR miniatura
  // com largura >= largura_desejada; se nenhuma chega lá, a maior.
  std::pair<std::string, std::pair<size_t, size_t>> get_thumbnail(const std::string &gcode_file, json &j,
                                                                  int largura_desejada);

  std::string download_file(const std::string &root,
    const std::string &fname,
    const std::string &dest);

  std::vector<std::string> get_interfaces();
  std::string interface_ip(const std::string &interface);
  std::string get_wifi_interface();
  std::string gateway_ip(const std::string &iface);
  bool gateway_in_arp(const std::string &gw_ip);

  template <typename Out>
  void split(const std::string &s, char delim, Out result);

  std::vector<std::string> split(const std::string &s, char delim);

  std::string get_obj_name(const std::string &id);
  std::string to_title(std::string s);
  // Friendly display name for a fan object key (e.g. "output_pin fan0" ->
  // "Part Cooling Fan", "heater_fan nozzle_fan" -> "Hotend Fan").
  std::string fan_display_name(const std::string &key);
  std::string eta_string(int64_t s);
  size_t bytes_to_mb(size_t s);
  // Tempo curto: "2h 21min", "1h 09min", "4min 42s", "45s".
  std::string tempo_curto(int64_t s);
  // Tamanho de arquivo: "20 KB", "7.8 MB", "1.2 GB".
  std::string tamanho_arquivo(size_t bytes);

  // Backlight helpers. Scan /sys/class/backlight/ on first call; pick the first
  // device. backlight_max() returns 0 if no device is found, in which case the
  // callers should hide the brightness UI and skip writes. backlight_set() is
  // a no-op when no device is available (or in SIMULATOR builds where it just
  // caches the value for the sysinfo slider to read back).
  int backlight_max();
  int backlight_get();
  void backlight_set(int value);

  // WiFi power-save. on=true disables NIC power-save (Broadcom `wl PM 0`) for
  // lower/steadier latency; on=false restores fast power-save (`wl PM 2`).
  // Shells out to /usr/bin/wl; a harmless no-op when that tool is absent
  // (non-Broadcom hardware / simulator). Returns true if the command was run.
  bool set_wifi_low_latency(bool on);

  // Runs `cmd` via popen (stdout+stderr combined into one stream) and blocks
  // until it exits, returning {exit_code, combined_output}. Blocking - the
  // only precedent for shelling out in this codebase (set_wifi_low_latency,
  // set_bluetooth_running) is fire-and-forget system() with no output
  // capture, so callers on the LVGL thread must run this on a background
  // std::thread and re-acquire their panel's lv_lock before touching any
  // LVGL object with the result (see wifi_panel.cpp's wait_for_connectivity
  // for the established pattern).
  std::pair<int, std::string> exec_capture(const std::string &cmd);

  template<typename T, typename U> void sort_map_values(std::map<T, U> v,
    std::vector<U> &out_vect,
    std::function<bool(U &, U &)> sorter) {
    for (auto &el : v) {
      out_vect.push_back(el.second);
    }

    std::sort(out_vect.begin(), out_vect.end(), sorter);
  };

  std::map<std::string, std::map<std::string, std::string>> parse_macros(json &m);

};

#endif // __K_UTILS_H__
