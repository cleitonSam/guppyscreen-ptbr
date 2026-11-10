#ifndef __CALIBRATION_MENU_PANEL_H__
#define __CALIBRATION_MENU_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "recalibration_wizard_panel.h"
#include "esteps_calibration_panel.h"
#include "bedmesh_panel.h"
#include "inputshaper_panel.h"
#include "axis_twist_panel.h"
#include "skew_correction_panel.h"
#include "tmc_tune_panel.h"

#include <mutex>

// Página Calibração (Proposta B, quadro 11): a 5ª página da coluna da
// esquerda. Mostra as 6 calibrações numeradas, NESTA ORDEM, todas à vista
// (sem rolar), na mesma ordem de wiki/Calibration-Explained.md: Torção do
// eixo X vem antes da malha de propósito (o Klipper aplica a correção da
// torção em toda medição dali para a frente, então uma malha medida antes
// fica torta e precisa ser refeita).
//
// Embaixo, em OUTROS, os dois atalhos do dia a dia: Malha da mesa e Altura do
// bico, que abrem por ação com nome (Navegacao::executar).
//
// As 6 calibrações não abrem durante a impressão (KUtils::notify_locked);
// a malha (só ver) e a altura do bico abrem.
//
// Os painéis de Malha / Compensação de vibração / Torção / Esquadro / TMC são
// do PrinterTunePanel e chegam aqui por referência (mesma assinatura de antes).
class CalibrationMenuPanel {
 public:
  CalibrationMenuPanel(KWebSocketClient &, std::mutex &,
                        BedMeshPanel &, InputShaperPanel &,
                        AxisTwistPanel &, SkewCorrectionPanel &,
                        TmcTunePanel &);
  ~CalibrationMenuPanel();

  // Detecta o motor_database.cfg (como o PrinterTunePanel fazia) e pinta de
  // grafite as calibrações indisponíveis. Só troca cores: não cria objetos.
  void init(json &j);

  // Vai para a página Calibração.
  void foreground();
  void handle_callback(lv_event_t *event);
  static void _handle_callback(lv_event_t *e) { ((CalibrationMenuPanel*)e->user_data)->handle_callback(e); }

 private:
  struct Cartao {
    lv_obj_t *cartao;
    lv_obj_t *selo;
    lv_obj_t *nome;
  };

  Cartao criar_cartao(int idx, const char *numero, const char *nome);
  void marcar_disponivel(Cartao &c, bool disponivel);
  void activate_row(lv_obj_t *row);

  KWebSocketClient &ws;

  lv_obj_t *panel_cont;

  // numeradas, na ordem recomendada
  Cartao axis_twist_row;
  Cartao wizard_row;
  Cartao inputshaper_row;
  Cartao esteps_row;
  Cartao skew_row;
  Cartao tmc_tune_row;
  // OUTROS
  lv_obj_t *bedmesh_btn;
  lv_obj_t *zoffset_btn;

  RecalibrationWizardPanel recalibration_wizard_panel;
  EstepsCalibrationPanel esteps_calibration_panel;
  BedMeshPanel &bedmesh_panel;
  InputShaperPanel &inputshaper_panel;
  AxisTwistPanel &axis_twist_panel;
  SkewCorrectionPanel &skew_correction_panel;
  TmcTunePanel &tmc_tune_panel;

  bool tmc_tune_available;
};

#endif  // __CALIBRATION_MENU_PANEL_H__
