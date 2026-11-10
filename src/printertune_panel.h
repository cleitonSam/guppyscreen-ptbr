#ifndef __PRINTERTUNE_PANEL_H__
#define __PRINTERTUNE_PANEL_H__

#include "finetune_panel.h"
#include "zoffset_panel.h"
#include "limits_panel.h"
#include "bedmesh_panel.h"
#include "inputshaper_panel.h"
#include "tmc_tune_panel.h"
#include "tmc_status_panel.h"
#include "power_panel.h"
#include "firmware_retraction_panel.h"
#include "axis_twist_panel.h"
#include "skew_correction_panel.h"
#include "calibration_menu_panel.h"
#include "button_container.h"
#include "lvgl/lvgl.h"

#include <mutex>

// Aba Configurações > Avançado (Proposta B, tela deitada).
// Grade 2 x 4 de botões 200 x 44 que abrem as subtelas do que quase não se
// usa: Macros, Console, Limites, Retração, Medições TMC, Energia e Ajuste fino.
// Continua dono dos painéis (malha, calibração, TMC, energia...), porque
// outros arquivos chegam a eles por get_bedmesh_panel() / get_power_panel().
class PrinterTunePanel {
 public:
  PrinterTunePanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, FineTunePanel &, ZOffsetPanel &);
  ~PrinterTunePanel();

  lv_obj_t *get_container();
  BedMeshPanel &get_bedmesh_panel();
  PowerPanel &get_power_panel();
  void init(json &j);
  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    PrinterTunePanel *panel = (PrinterTunePanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  lv_obj_t *criar_item(const char *texto, lv_coord_t x, lv_coord_t y);
  void abrir_retracao();

  lv_obj_t *cont;
  BedMeshPanel bedmesh_panel;
  FineTunePanel &finetune_panel;
  ZOffsetPanel &zoffset_panel;
  LimitsPanel limits_panel;
  InputShaperPanel inputshaper_panel;
  TmcTunePanel tmc_tune_panel;
  TmcStatusPanel tmc_status_panel;
  PowerPanel power_panel;
  FirmwareRetractionPanel firmware_retraction_panel;
  AxisTwistPanel axis_twist_panel;
  SkewCorrectionPanel skew_correction_panel;
  // Construído por último: recebe referências de vários painéis acima. A
  // página Calibração (lote Calibração) é quem mostra este menu.
  CalibrationMenuPanel calibration_menu_panel;

  // botões da aba Avançado (criados uma vez)
  lv_obj_t *macros_btn;
  lv_obj_t *console_btn;
  lv_obj_t *limits_btn;
  lv_obj_t *retraction_btn;
  lv_obj_t *tmc_status_btn;
  lv_obj_t *power_devices_btn;
  lv_obj_t *finetune_btn;
};

#endif // __PRINTERTUNE_PANEL_H__
