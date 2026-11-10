#include "calibration_menu_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

LV_IMG_DECLARE(bedmesh_img);
LV_IMG_DECLARE(z_closer);

// Posições dos 6 cartões (200 x 48) na página, em 2 colunas (quadro 11).
static const lv_coord_t CARTAO_X[6] = {8, 216, 8, 216, 8, 216};
static const lv_coord_t CARTAO_Y[6] = {52, 52, 108, 108, 164, 164};

CalibrationMenuPanel::Cartao CalibrationMenuPanel::criar_cartao(int idx, const char *numero, const char *nome) {
  Cartao c;
  c.cartao = lv_obj_create(panel_cont);
  lv_obj_set_pos(c.cartao, CARTAO_X[idx], CARTAO_Y[idx]);
  lv_obj_set_size(c.cartao, 200, 48);
  Marca::cartao_tocavel(c.cartao);

  // selo numerado 28 px em (10,10)
  c.selo = Marca::criar_selo(c.cartao, 28, Marca::PRETO_3, Marca::PAPEL, numero);
  lv_obj_set_pos(c.selo, 10, 10);

  // nome f14 em (46, meio) com 146 px, até 2 linhas, centralizado na vertical
  c.nome = Marca::criar_rotulo(c.cartao, nome, Marca::f14(), Marca::PAPEL);
  lv_obj_set_width(c.nome, 146);
  lv_label_set_long_mode(c.nome, LV_LABEL_LONG_WRAP);
  lv_obj_align(c.nome, LV_ALIGN_LEFT_MID, 46, 0);

  lv_obj_add_event_cb(c.cartao, &CalibrationMenuPanel::_handle_callback, LV_EVENT_CLICKED, this);
  return c;
}

void CalibrationMenuPanel::marcar_disponivel(Cartao &c, bool disponivel) {
  // Só cor: o cartão continua tocável para mostrar o motivo.
  uint32_t cor = disponivel ? Marca::PAPEL : Marca::GRAFITE;
  lv_obj_set_style_text_color(c.nome, lv_color_hex(cor), 0);
  lv_obj_set_style_text_color(c.selo, lv_color_hex(cor), 0);
}

CalibrationMenuPanel::CalibrationMenuPanel(KWebSocketClient &websocket_client, std::mutex &l,
                                            BedMeshPanel &bedmesh, InputShaperPanel &inputshaper,
                                            AxisTwistPanel &axis_twist, SkewCorrectionPanel &skew,
                                            TmcTunePanel &tmc_tune)
  : ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , recalibration_wizard_panel(websocket_client, l)
  , esteps_calibration_panel(websocket_client, l)
  , bedmesh_panel(bedmesh)
  , inputshaper_panel(inputshaper)
  , axis_twist_panel(axis_twist)
  , skew_correction_panel(skew)
  , tmc_tune_panel(tmc_tune)
  , tmc_tune_available(false)
{
  // Vira o conteúdo da página Calibração (424 x 272).
  Navegacao::colocar(panel_cont, Pagina::CALIBRACAO);
  // a página já é PRETO opaco: o contêiner fica transparente (não pinta duas vezes)
  lv_obj_set_style_bg_opa(panel_cont, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(panel_cont, 0, 0);
  lv_obj_set_style_border_width(panel_cont, 0, 0);
  lv_obj_clear_flag(panel_cont, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = Marca::criar_rotulo(panel_cont, "Calibração", Marca::f20(), Marca::PAPEL);
  lv_obj_set_pos(title, 8, 10);

  lv_obj_t *ordem = Marca::criar_rotulo(panel_cont, "NESTA ORDEM", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(ordem, 276, 16);
  lv_obj_set_width(ordem, 140);
  lv_obj_set_style_text_align(ordem, LV_TEXT_ALIGN_RIGHT, 0);

  axis_twist_row  = criar_cartao(0, "1", "Torção do eixo X");
  wizard_row      = criar_cartao(1, "2", "Altura do bico e malha da mesa");
  inputshaper_row = criar_cartao(2, "3", "Compensação de vibração");
  esteps_row      = criar_cartao(3, "4", "Calibração da extrusora");
  skew_row        = criar_cartao(4, "5", "Correção de esquadro");
  tmc_tune_row    = criar_cartao(5, "6", "Autoajuste dos motores (opcional)");

  // ---- OUTROS ----
  lv_obj_t *outros = Marca::criar_rotulo(panel_cont, "OUTROS", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(outros, 8, 235);

  auto botao_icone = [this](const void *img, const char *txt, lv_coord_t x) {
    lv_obj_t *b = lv_btn_create(panel_cont);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_pos(b, x, 220);
    lv_obj_set_size(b, 168, 44);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, 6, 0);
    Marca::criar_icone(b, img, 20, Marca::PAPEL);
    lv_obj_t *t = lv_label_create(b);
    lv_label_set_text(t, txt);
    lv_obj_set_style_text_font(t, Marca::f14(), 0);
    Marca::estilo_botao(b, Marca::Tipo::NORMAL);  // depois dos filhos (pinta o ícone)
    lv_obj_add_event_cb(b, &CalibrationMenuPanel::_handle_callback, LV_EVENT_CLICKED, this);
    return b;
  };
  bedmesh_btn = botao_icone(&bedmesh_img, "Malha da mesa", 72);
  zoffset_btn = botao_icone(&z_closer, "Altura do bico", 248);
}

CalibrationMenuPanel::~CalibrationMenuPanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
}

void CalibrationMenuPanel::init(json &j) {
  State *s = State::get_instance();
  auto kp = s->get_data("/printer_info/klipper_path"_json_pointer);
  if (!kp.is_null()) {
    auto p = fs::path(kp.template get<std::string>()) / "klippy/extras/motor_database.cfg";
    if (fs::exists(p)) {
      tmc_tune_available = true;
      tmc_tune_panel.init(j, p);
    } else {
      tmc_tune_available = false;
    }
  }

  // Indisponível = grafite. O toque continua mostrando o motivo.
  marcar_disponivel(tmc_tune_row, tmc_tune_available);
  marcar_disponivel(axis_twist_row, AxisTwistPanel::is_enabled());
  marcar_disponivel(skew_row, SkewCorrectionPanel::is_enabled());
}

void CalibrationMenuPanel::foreground() {
  Navegacao::ir_para(Pagina::CALIBRACAO);
}

void CalibrationMenuPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  activate_row(lv_event_get_current_target(e));
}

void CalibrationMenuPanel::activate_row(lv_obj_t *row) {
  // Roda dentro de evento do LVGL (lv_lock já preso): nada aqui prende o lock.

  if (row == bedmesh_btn) {
    // Ver a malha imprimindo pode; as ações que mudam algo (Calibrar /
    // Limpar / Salvar) já são travadas dentro do próprio painel.
    if (!Navegacao::executar("malha_da_mesa")) {
      bedmesh_panel.foreground();
    }
    return;
  }

  if (row == zoffset_btn) {
    if (!Navegacao::executar("altura_do_bico")) {
      spdlog::warn("CalibrationMenuPanel: ação altura_do_bico ainda não registrada");
    }
    return;
  }

  if (row == axis_twist_row.cartao) {
    // Detect via the config section: the v0.12.0 module has no get_status, so
    // it never shows up as a live printer object - only configfile.settings.
    if (!AxisTwistPanel::is_enabled()) {
      KUtils::notify_toast(
        "A compensação da torção do eixo X não está ativada.\n"
        "Adicione [axis_twist_compensation] ao printer.cfg.",
        4000);
      return;
    }
    // Calibration probes the bed; never mid-print.
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    axis_twist_panel.foreground();
    return;
  }

  if (row == wizard_row.cartao) {
    // The wizard inside probes the bed and ends in a SAVE_CONFIG restart;
    // never mid-print.
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    recalibration_wizard_panel.foreground();
    return;
  }

  if (row == inputshaper_row.cartao) {
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    inputshaper_panel.foreground();
    return;
  }

  if (row == esteps_row.cartao) {
    // Heats the hotend and probes extrusion accuracy; never mid-print.
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    esteps_calibration_panel.foreground();
    return;
  }

  if (row == skew_row.cartao) {
    // Detect via the config section (no get_status), like Axis Twist.
    if (!SkewCorrectionPanel::is_enabled()) {
      KUtils::notify_toast(
        "A correção de esquadro não está ativada.\n"
        "Adicione [skew_correction] ao printer.cfg.",
        4000);
      return;
    }
    // Apply & Save runs SAVE_CONFIG (restarts Klipper); never mid-print.
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    skew_correction_panel.foreground();
    return;
  }

  if (row == tmc_tune_row.cartao) {
    if (!tmc_tune_available) {
      // Antes a linha ficava desabilitada e não reagia; agora diz o motivo.
      KUtils::notify_toast(
        "O autoajuste dos motores não está disponível.\n"
        "Falta o motor_database.cfg no Klipper.",
        4000);
      return;
    }
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    tmc_tune_panel.foreground();
    return;
  }
}
