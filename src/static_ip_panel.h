#ifndef __STATIC_IP_PANEL_H__
#define __STATIC_IP_PANEL_H__

#include "lvgl/lvgl.h"
#include <functional>
#include <mutex>
#include <string>

// Per-SSID static IP configuration sub-screen, reached from NetworkDetailPanel's
// "Configure Static IP" button (or Configurações › Rede › IP fixo). Backed
// entirely by shelling out to static_ip.py (see
// memory/project_static_ip_design.md for the full design and
// k1/scripts/static_ip.py for the backend) - this panel never touches wlan0
// itself, it only reads/writes state through that script.
//
// Subtela da marca (Proposta B): à esquerda os 4 endereços (caixinhas de
// 0-255) e "Voltar ao automático"; à direita o segmentado Automático/Manual,
// o texto de estado e "Salvar". O teclado de números aparece na coluna da
// direita, ao lado da caixa que está sendo editada.
class StaticIpPanel {
public:
  StaticIpPanel(std::mutex &l);
  ~StaticIpPanel();

  // Scopes the panel to ssid, shows it, and kicks off a live status refresh.
  void foreground(const std::string &ssid);

  void handle_back_btn(lv_event_t *event);
  // DHCP/Manual is a pure view switch now - it only ever changes which
  // fields are shown/editable, never touches the network. Reverting a live
  // static config to DHCP is its own explicitly-labelled action, see
  // handle_revert_btn.
  void handle_mode_toggle(lv_event_t *event);
  void handle_revert_btn(lv_event_t *event);
  void handle_save_btn(lv_event_t *event);
  void handle_kb_input(lv_event_t *event);

  static void _handle_back_btn(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_back_btn(e); }
  static void _handle_mode_toggle(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_mode_toggle(e); }
  static void _handle_revert_btn(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_revert_btn(e); }
  static void _handle_save_btn(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_save_btn(e); }
  static void _handle_kb_input(lv_event_t *e) { ((StaticIpPanel *)e->user_data)->handle_kb_input(e); }

private:
  void refresh_status();
  void set_manual_mode(bool manual);
  void update_revert_visibility();
  void set_busy(bool busy, const std::string &msg);
  // Texto de estado: erro/sucesso em PAPEL (a palavra diz o que houve),
  // informação em GRAFITE.
  void set_status(const std::string &msg, bool destaque);
  // Runs `python3 static_ip.py <args>` on a background thread (backend calls
  // can take several seconds - never block the LVGL thread), then re-takes
  // lv_lock before calling on_done with the result.
  void run_backend_async(const std::string &args, const std::string &busy_msg,
                          const std::function<void(int, const std::string &)> &on_done);

  std::mutex &lv_lock;
  std::string ssid;

  lv_obj_t *cont;
  lv_obj_t *title_label;
  lv_obj_t *rede_label;       // nome da rede (à direita do cabeçalho)
  lv_obj_t *mode_cont;
  lv_obj_t *dhcp_mode_btn;
  lv_obj_t *manual_mode_btn;
  lv_obj_t *revert_btn;
  // Each address is 4 small octet boxes rather than one free-text field with
  // dots - fewer characters to type on the shared numeric pad, and a bad
  // octet can be flagged red right where it was typed instead of only via a
  // status line at the top.
  lv_obj_t *ip_octets[4];
  lv_obj_t *netmask_octets[4];
  lv_obj_t *gateway_octets[4];
  lv_obj_t *dns_octets[4];
  lv_obj_t *status_label;
  // Holds only the 4 field rows; Save is its own sibling and never moves.
  lv_obj_t *body_cont;
  lv_obj_t *save_btn;
  lv_obj_t *kb;

  bool manual_mode = false;
  bool has_static_config = false;
  bool busy = false;
};

#endif // __STATIC_IP_PANEL_H__
