#ifndef __MAIN_PANEL_H__
#define __MAIN_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "sensor_container.h"
#include "button_container.h"
#include "prompt_panel.h"
#include "numpad.h"
#include "homing_panel.h"
#include "extruder_panel.h"
#include "fan_panel.h"
#include "led_panel.h"
#include "print_panel.h"
#include "macros_panel.h"
#include "console_panel.h"
#include "printertune_panel.h"
#include "setting_panel.h"
#include "print_status_panel.h"
#include "spoolman_panel.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <map>
#include <memory>
#include <string>

// Dono da página Início (quadro 01) e da aba Temperatura (quadro 07) da
// Proposta B. Cria os painéis e passa os pais novos da Navegacao; cria as
// subtelas de Macros e Console e registra as ações "macros" e "console".
class MainPanel : public NotifyConsumer {
 public:
  MainPanel(KWebSocketClient &ws,
	    std::mutex &lv_lock,
	    SpoolmanPanel &sm);

  ~MainPanel();
  void consume(json &data);
  void init(json &data);
  void subscribe();
  PrinterTunePanel& get_tune_panel();
  void enable_spoolman();
  bool is_prompt_visible() const { return prompt_panel.is_visible(); }

  void create_panel();
  void create_sensors(json &temp_sensors);
  void create_fans(json &temp_fans);
  void create_leds(json &leds);
#ifdef SIMULATOR
  /* Populate sensors directly from config and start a periodic timer that
   * pushes mock temperature values, so the home panel is visible without
   * a live Moonraker connection. */
  void sim_setup_mock_data();
#endif

  // abre a subtela do Console no terminal (ex.: depois de rodar uma macro)
  void show_console();

 private:
  void criar_inicio();
  void criar_temperatura();
  // atualiza Início e cartões de temperatura a partir de um objeto de status
  // (params[0] de notify_status_update ou result.status da assinatura)
  void atualizar_de(const json *status, bool transicoes);
  void atualizar_chip();
  void atualizar_luz(const json &obj_luz);
  void pedir_ultima_impressao();
  void agendar_ultima_impressao();
  void mostrar_ultima_impressao(const json *job);   // com lv_lock preso
  void carregar_miniatura(const std::string &arquivo);
  void preaquecer(int bico, int mesa);

  KWebSocketClient &ws;
  HomingPanel homing_panel;
  FanPanel fan_panel;
  LedPanel led_panel;
  lv_obj_t *macros_cont;
  MacrosPanel macros_panel;
  lv_obj_t *console_cont;
  ConsolePanel console_panel;
  SettingPanel setting_panel;
  PrintStatusPanel print_status_panel;
  PrintPanel print_panel;
  PrinterTunePanel printertune_panel;
  Numpad numpad;
  ExtruderPanel extruder_panel;
  PromptPanel prompt_panel;
  SpoolmanPanel &spoolman_panel;

  // ---- Início ----
  lv_obj_t *inicio_chip;
  lv_obj_t *bico_alvo;
  lv_obj_t *bico_valor;
  lv_obj_t *mesa_alvo;
  lv_obj_t *mesa_valor;
  lv_obj_t *cartao_luz;
  lv_obj_t *luz_icone;
  lv_obj_t *luz_valor;
  lv_obj_t *ultima_thumb;
  lv_obj_t *ultima_sem_thumb;
  lv_obj_t *ultima_nome;
  lv_obj_t *ultima_selo;
  lv_obj_t *ultima_status;
  lv_obj_t *ultima_btn;
  std::string ultima_arquivo;
  int ultima_estado;              // -1 ainda nada, 0 sem histórico, 1 com histórico
  std::string estado_impressao;   // print_stats.state
  std::string luz_id;             // primeira luz configurada ("" = nenhuma)
  int luz_ligada;                 // -1 desconhecido, 0 desligada, 1 ligada

  std::map<std::string, std::shared_ptr<SensorContainer>> sensors;
};
#endif // __MAIN_PANEL_H__
