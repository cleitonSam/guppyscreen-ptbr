#include "printertune_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

// Aba Avançado (coordenadas da aba, 424 x 227): grade 2 x 4 de botões
// 200 x 44 em x = 8 / 216 e y = 7, 59, 111, 163. Cada botão: símbolo + nome
// f14 à esquerda e LV_SYMBOL_RIGHT GRAFITE à direita (abre outra tela).
static const lv_coord_t ITEM_L = 200;
static const lv_coord_t COL_X[2] = {8, 216};
static const lv_coord_t LIN_Y[4] = {7, 59, 111, 163};

PrinterTunePanel::PrinterTunePanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, FineTunePanel &finetune, ZOffsetPanel &zoffset)
  : cont(lv_obj_create(parent))
  , bedmesh_panel(c, l)
  , finetune_panel(finetune)
  , zoffset_panel(zoffset)
  , limits_panel(c, l)
  , inputshaper_panel(c, l)
  , tmc_tune_panel(c)
  , tmc_status_panel(c, l)
  , power_panel(c, l)
  , firmware_retraction_panel(c, l)
  , axis_twist_panel(c, l)
  , skew_correction_panel(c)
  , calibration_menu_panel(c, l, bedmesh_panel, inputshaper_panel, axis_twist_panel, skew_correction_panel, tmc_tune_panel)
  , macros_btn(NULL)
  , console_btn(NULL)
  , limits_btn(NULL)
  , retraction_btn(NULL)
  , tmc_status_btn(NULL)
  , power_devices_btn(NULL)
  , finetune_btn(NULL)
{
  // conteúdo da aba: transparente, sem rolagem, coordenadas absolutas
  lv_obj_set_pos(cont, 0, 0);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

  macros_btn        = criar_item(LV_SYMBOL_LIST "   Macros",           COL_X[0], LIN_Y[0]);
  console_btn       = criar_item(LV_SYMBOL_KEYBOARD "   Console",      COL_X[1], LIN_Y[0]);
  limits_btn        = criar_item(LV_SYMBOL_CHARGE "   Limites",        COL_X[0], LIN_Y[1]);
  retraction_btn    = criar_item(LV_SYMBOL_LOOP "   Retração",         COL_X[1], LIN_Y[1]);
  tmc_status_btn    = criar_item(LV_SYMBOL_EYE_OPEN "   Medições TMC", COL_X[0], LIN_Y[2]);
  power_devices_btn = criar_item(LV_SYMBOL_POWER "   Energia",         COL_X[1], LIN_Y[2]);
  finetune_btn      = criar_item(LV_SYMBOL_EDIT "   Ajuste fino",      COL_X[0], LIN_Y[3]);
}

PrinterTunePanel::~PrinterTunePanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *PrinterTunePanel::criar_item(const char *texto, lv_coord_t x, lv_coord_t y) {
  lv_obj_t *btn = Marca::criar_botao(cont, texto, Marca::Tipo::NORMAL, Marca::f14());
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, ITEM_L, Marca::TOQUE);

  // nome à esquerda (o criar_botao centraliza)
  lv_obj_t *label = lv_obj_get_child(btn, 0);
  if (label != NULL) {
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 14, 0);
  }

  // seta à direita: abre outra tela
  lv_obj_t *seta = Marca::criar_rotulo(btn, LV_SYMBOL_RIGHT, Marca::f12(), Marca::GRAFITE);
  lv_obj_align(seta, LV_ALIGN_RIGHT_MID, -12, 0);

  lv_obj_add_event_cb(btn, &PrinterTunePanel::_handle_callback, LV_EVENT_CLICKED, this);
  return btn;
}

lv_obj_t *PrinterTunePanel::get_container() {
  return cont;
}

BedMeshPanel &PrinterTunePanel::get_bedmesh_panel() {
  return bedmesh_panel;
}

PowerPanel &PrinterTunePanel::get_power_panel() {
  return power_panel;
}

void PrinterTunePanel::init(json &j) {
  limits_panel.init(j);

  tmc_status_panel.init(j);

  // TMC Autotune's motor_database.cfg detection now lives with the row itself,
  // inside the Calibration hub.
  calibration_menu_panel.init(j);
}

void PrinterTunePanel::abrir_retracao() {
  // Check 1: is [firmware_retraction] configured in Klipper at all? When it
  // is, Klipper exposes a firmware_retraction status object.
  auto fr = State::get_instance()->get_data(
    "/printer_state/firmware_retraction"_json_pointer);
  if (fr.is_null()) {
    KUtils::notify_toast(
      "A retração pelo firmware está desligada nesta impressora.\n"
      "A retração vem do fatiador (Orca).",
      4000);
    return;
  }
  // Check 2: only meaningful mid-print. Idle, we let the user tune freely;
  // printing, we make sure the running file is actually sliced for firmware
  // retraction (uses G10/G11) or the live values won't do anything.
  if (KUtils::is_printing()
      && KUtils::print_uses_firmware_retraction() == 0) {
    KUtils::notify_toast(
      "Esta impressão não usa retração pelo firmware.\n"
      "Estes ajustes não terão efeito.",
      4000);
    return;
  }
  firmware_retraction_panel.foreground();
}

// Roda dentro do evento do LVGL (lv_lock já preso): nada de prender de novo.
void PrinterTunePanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(event);

  if (btn == macros_btn) {
    spdlog::trace("avancado: macros");
    if (!Navegacao::executar("macros")) {
      spdlog::warn("ação 'macros' não registrada");
    }
  } else if (btn == console_btn) {
    spdlog::trace("avancado: console");
    if (!Navegacao::executar("console")) {
      spdlog::warn("ação 'console' não registrada");
    }
  } else if (btn == finetune_btn) {
    spdlog::trace("tune finetune pressed");
    // o lote Acompanhamento registra "ajuste_fino"; até lá, o caminho antigo
    if (!Navegacao::executar("ajuste_fino")) {
      finetune_panel.foreground();
    }
  } else if (btn == limits_btn) {
    spdlog::trace("limits pressed");
    // abre livre: a pergunta (imprimindo) vem ao mudar um valor
    limits_panel.foreground();
  } else if (btn == tmc_status_btn) {
    spdlog::trace("tmc metrics pressed");
    tmc_status_panel.foreground();
  } else if (btn == power_devices_btn) {
    spdlog::trace("power devices pressed");
    // abre livre: a pergunta (imprimindo) vem ao ligar/desligar um aparelho
    power_panel.foreground();
  } else if (btn == retraction_btn) {
    spdlog::trace("retraction pressed");
    abrir_retracao();
  }
}
