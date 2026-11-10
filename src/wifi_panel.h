#ifndef __WIFI_PANEL_H__
#define __WIFI_PANEL_H__

#include "wpa_event.h"
#include "static_ip_panel.h"
#include "network_detail_panel.h"
#include "wifi_row_item.h"
#include "lvgl/lvgl.h"
#include <mutex>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Subtela "Wi-Fi" (Configurações › Rede › Wi-Fi): lista das redes à esquerda,
// estado da ligação à direita; ao escolher uma rede nova, o campo da senha e o
// teclado aparecem dentro da própria subtela. A lógica com o wpa_supplicant é
// a de sempre.
class WifiPanel {
public:
  WifiPanel(std::mutex &l);

  ~WifiPanel();

  void foreground();
  void handle_back_btn(lv_event_t *event);
  void handle_wpa_event(const std::string &events);
  void confirm_remove_network(const std::string &ssid, const std::string &nid);
  void handle_kb_input(lv_event_t *e);
  void handle_eye_btn(lv_event_t *e);
  void wait_for_connectivity(const std::string &iface, const std::string &net, uint32_t gen);
  void connect(const char *);
  bool find_current_network();

  // WifiRowItem calls this when its row is tapped: connects/reconnects for
  // an ordinary row, or opens NetworkDetailPanel if the row is the network
  // we're already connected to (where "connect" wouldn't mean anything).
  void handle_row_activated(const std::string &ssid);

  // NetworkDetailPanel's two buttons call back into here - it owns the
  // WpaEvent connection and the StaticIpPanel instance those actions need.
  void open_static_ip_for_current();
  void forget_current_network();

  // Configurações › Rede (com o lv_lock preso):
  // nome da rede conectada que a tela já conhece ("" se ainda não sabe);
  std::string rede_atual();
  // detalhes da rede conectada (sem rede conhecida: abre a lista);
  void abrir_detalhes();
  // IP fixo da rede conectada (sem rede conhecida: abre a lista).
  void abrir_ip_fixo();
  // Chamado (com o lv_lock preso) ao voltar da lista e depois de esquecer
  // uma rede: o SettingPanel atualiza os textos da aba Rede.
  std::function<void()> ao_mudar_rede;

  static void _handle_back_btn(lv_event_t *event) {
    WifiPanel *panel = (WifiPanel *)event->user_data;
    panel->handle_back_btn(event);
  };

  static void _handle_kb_input(lv_event_t *e) {
    WifiPanel *panel = (WifiPanel *)e->user_data;
    panel->handle_kb_input(e);
  };

  static void _handle_eye_btn(lv_event_t *e) {
    WifiPanel *panel = (WifiPanel *)e->user_data;
    panel->handle_eye_btn(e);
  };

private:
  void rebuild_wifi_rows();
  // Senha: esconde a lista e o estado, mostra o campo e o teclado (e volta).
  void modo_senha(bool sim);
  void mostrar_detalhes(const std::string &ssid);

  std::mutex &lv_lock;
  WpaEvent wpa_event;
  lv_obj_t *cont;
  lv_obj_t *wifi_list_cont;   // cartão da lista (rola na vertical)
  lv_obj_t *wifi_right;       // cartão do estado
  lv_obj_t *prompt_cont;      // = wifi_right
  lv_obj_t *wifi_label;       // estado / "Digite a senha de ..."
  lv_obj_t *pw_row;
  lv_obj_t *password_input;
  lv_obj_t *eye_btn;
  lv_obj_t *kb;
  std::string selected_network;
  std::string cur_network;
  // Cópia de cur_network só para a tela: escrita e lida com o lv_lock preso
  // (cur_network é escrito pela linha do wpa_supplicant sem o lock).
  std::string cur_network_ui;
  std::string pending_remove_nid;
  std::map<std::string, std::string> list_networks;

  // Everything the list needs to know about one SSID seen in a scan.
  // "missed" is how many consecutive scans it's been absent from - lets
  // rebuild_wifi_rows age a network out gradually instead of dropping it the
  // instant a single noisy scan misses it.
  struct WifiEntry {
    int signal = -100;
    bool secured = false;
    int missed = 0;
  };
  std::map<std::string, WifiEntry> wifi_name_db;
  std::vector<std::unique_ptr<WifiRowItem>> wifi_rows;

  bool entering_password = false;
  bool pw_visible = false;
  int rescan_budget = 0;
  size_t last_scan_count = 0;
  std::atomic<uint32_t> conn_gen{0};
  StaticIpPanel static_ip_panel;
  NetworkDetailPanel network_detail_panel;

};

#endif // __WIFI_PANEL_H__
