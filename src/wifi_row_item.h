#ifndef __WIFI_ROW_ITEM_H__
#define __WIFI_ROW_ITEM_H__

#include "lvgl/lvgl.h"

#include <functional>
#include <string>

// One row in WifiPanel's network list. Exactly one tap target per row - the
// row body activates the network (connect, or open details if it's already
// the connected one); a separately-spaced trailing icon, present only on
// known-but-not-connected rows, forgets the saved network. A prior design
// crammed a status icon and an edit icon into two adjacent narrow table
// columns instead - a mistap between them deleted a saved network's password
// live on-device, which is the failure mode this row layout exists to avoid.
//
// Visual da marca (Proposta B): linha de 44 com o símbolo do Wi-Fi, o nome
// (f14) e embaixo "Conectado" ou o sinal em palavra (f12 GRAFITE); à direita a
// seta (rede conectada) ou o "esquecer" num alvo de 44 x 44 (rede salva).
class WifiRowItem {
public:
  // largura da lista do WifiPanel (as posições da linha contam com ela)
  static constexpr lv_coord_t LARGURA = 248;

  WifiRowItem(lv_obj_t *parent,
              const std::string &ssid,
              int signal_dbm,
              bool connected,
              bool saved,
              const std::string &nid,
              std::function<void(const std::string &ssid)> on_activate,
              std::function<void(const std::string &ssid, const std::string &nid)> on_forget);
  ~WifiRowItem();

  lv_obj_t *get_container() { return cont; }

  void handle_row_click(lv_event_t *e);
  void handle_forget_click(lv_event_t *e);

  static void _handle_row_click(lv_event_t *e) {
    ((WifiRowItem *)e->user_data)->handle_row_click(e);
  }
  static void _handle_forget_click(lv_event_t *e) {
    ((WifiRowItem *)e->user_data)->handle_forget_click(e);
  }

private:
  std::string ssid;
  std::string nid;
  lv_obj_t *cont;
  std::function<void(const std::string &)> on_activate;
  std::function<void(const std::string &, const std::string &)> on_forget;
};

#endif // __WIFI_ROW_ITEM_H__
