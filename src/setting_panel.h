#ifndef __SETTING_PANEL_H__
#define __SETTING_PANEL_H__

#include "platform.h"

#ifndef OS_ANDROID
#include "wifi_panel.h"
#endif

#include "sysinfo_panel.h"
#include "spoolman_panel.h"
#include "printer_select_panel.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>

// Página Configurações (quadro 12 da maquete B). A base cria a barra de abas
// Tela / Rede / Sistema / Avançado e passa Navegacao::aba(Aba::SISTEMA) como
// parent. Este painel monta:
//  - Rede: Wi-Fi, Endereço IP, IP fixo e Baixa latência (lista);
//  - Sistema: reiniciar Klipper / firmware / tela, atualizar tela,
//    Impressoras, Spoolman e o cartão de ajustes do SysInfoPanel (rola na
//    vertical, sem inércia);
//  - Tela: montada pelo SysInfoPanel.
// O Avançado é do PrinterTunePanel.
class SettingPanel {
 public:
  SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm);
  ~SettingPanel();

  lv_obj_t *get_container();
  void enable_spoolman();
  SysInfoPanel &get_sysinfo_panel() { return sysinfo_panel; }

  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    SettingPanel *panel = (SettingPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  // Atualiza os textos da aba Rede (rede, IP). Com o lv_lock preso.
  void atualizar_rede();
  void reiniciar_tela();
  void atualizar_tela();

  KWebSocketClient &ws;
  lv_obj_t *cont;        // conteúdo da aba Sistema (rola na vertical)
  lv_obj_t *rede_cont;   // conteúdo da aba Rede

#ifndef OS_ANDROID
  WifiPanel wifi_panel;
#endif

  SysInfoPanel sysinfo_panel;
  SpoolmanPanel &spoolman_panel;
  PrinterSelectPanel printer_select_panel;

  // Sistema
  lv_obj_t *restart_klipper_btn;
  lv_obj_t *restart_firmware_btn;
  lv_obj_t *guppy_restart_btn;
  lv_obj_t *guppy_update_btn;
  lv_obj_t *printer_select_row;
  lv_obj_t *spoolman_row;
  bool spoolman_ativo;

  // Rede
  lv_obj_t *wifi_row;
  lv_obj_t *wifi_valor;
  lv_obj_t *ip_row;
  lv_obj_t *ip_valor;
  lv_obj_t *ip_fixo_row;
  lv_obj_t *pm_toggle;
};

#endif // __SETTING_PANEL_H__
