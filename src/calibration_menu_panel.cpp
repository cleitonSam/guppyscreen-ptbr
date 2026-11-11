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

// Posições dos 6 cartões (200 x 48) na aba Completa, em 2 colunas.
static const lv_coord_t CARTAO_X[6] = {8, 216, 8, 216, 8, 216};
static const lv_coord_t CARTAO_Y[6] = {8, 8, 64, 64, 120, 120};

// Aba Dia a dia: 6 cartões 200 x 64 em 2 colunas (8 + 64 + 8 + 64 + 8 + 64 = 216).
static const lv_coord_t ATALHO_Y[3] = {8, 80, 152};

static lv_obj_t *criar_aba_cont(lv_obj_t *pai) {
  lv_obj_t *c = lv_obj_create(pai);
  lv_obj_set_pos(c, 0, Marca::ABA_Y);
  lv_obj_set_size(c, Marca::CONT_L, Marca::ABA_CONT_A);
  lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, 0, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);
  return c;
}

CalibrationMenuPanel::Atalho CalibrationMenuPanel::criar_atalho(int idx, const char *titulo, const char *detalhe) {
  Atalho a;
  a.cartao = lv_obj_create(dia_cont);
  lv_obj_set_pos(a.cartao, CARTAO_X[idx], ATALHO_Y[idx / 2]);
  lv_obj_set_size(a.cartao, 200, 64);
  Marca::cartao_tocavel(a.cartao);
  a.titulo = Marca::criar_rotulo(a.cartao, titulo, Marca::f16(), Marca::PAPEL);
  lv_obj_set_pos(a.titulo, 12, 10);
  lv_obj_set_width(a.titulo, 176);
  lv_label_set_long_mode(a.titulo, LV_LABEL_LONG_DOT);
  a.detalhe = Marca::criar_rotulo(a.cartao, detalhe, Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(a.detalhe, 12, 36);
  lv_obj_set_width(a.detalhe, 176);
  lv_label_set_long_mode(a.detalhe, LV_LABEL_LONG_DOT);
  lv_obj_add_event_cb(a.cartao, &CalibrationMenuPanel::_handle_callback, LV_EVENT_CLICKED, this);
  return a;
}

CalibrationMenuPanel::Cartao CalibrationMenuPanel::criar_cartao(int idx, const char *numero, const char *nome) {
  Cartao c;
  c.cartao = lv_obj_create(completa_cont);
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

  // duas abas, no mesmo padrão das páginas Controle e Configurações
  static const char *mapa_abas[] = {"Dia a dia", "Completa", ""};
  abas = Marca::criar_barra_abas(panel_cont, mapa_abas, 8, 408);
  lv_obj_add_event_cb(abas, &CalibrationMenuPanel::_aba_cb, LV_EVENT_VALUE_CHANGED, this);
  dia_cont = criar_aba_cont(panel_cont);
  completa_cont = criar_aba_cont(panel_cont);

  // ---- Dia a dia ----
  altura_auto   = criar_atalho(0, "Altura do bico", "Automática (sensor)");
  medir_barra   = criar_atalho(1, "Medir a barra", "Esquerda x direita do bico");
  chapa_lisa    = criar_atalho(2, "Chapa lisa", "Toque ao colocar esta chapa");
  chapa_textura = criar_atalho(3, "Chapa texturizada", "Toque ao colocar esta chapa");
  malha_atalho  = criar_atalho(4, "Malha da mesa", "Ver e medir a mesa");
  papel_atalho  = criar_atalho(5, "Altura com papel", "Ajuste manual, passo a passo");

  // ---- Completa ----
  lv_obj_t *ordem = Marca::criar_rotulo(completa_cont, "Faça nesta ordem: uma vez e depois de mexer na mecânica",
                                        Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(ordem, 8, 182);
  lv_obj_set_width(ordem, 408);
  lv_label_set_long_mode(ordem, LV_LABEL_LONG_DOT);

  axis_twist_row  = criar_cartao(0, "1", "Torção do eixo X");
  wizard_row      = criar_cartao(1, "2", "Altura do bico e malha da mesa");
  inputshaper_row = criar_cartao(2, "3", "Compensação de vibração");
  esteps_row      = criar_cartao(3, "4", "Calibração da extrusora");
  skew_row        = criar_cartao(4, "5", "Correção de esquadro");
  tmc_tune_row    = criar_cartao(5, "6", "Autoajuste dos motores (opcional)");

  // os atalhos de Malha e Altura com papel agora moram na aba Dia a dia
  bedmesh_btn = malha_atalho.cartao;
  zoffset_btn = papel_atalho.cartao;

  mostrar_aba(0);
}

void CalibrationMenuPanel::_aba_cb(lv_event_t *e) {
  auto *self = (CalibrationMenuPanel *)lv_event_get_user_data(e);
  self->mostrar_aba(lv_btnmatrix_get_selected_btn(self->abas));
}

void CalibrationMenuPanel::mostrar_aba(uint16_t aba) {
  if (aba == 1) {
    lv_obj_add_flag(dia_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(completa_cont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(dia_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(completa_cont, LV_OBJ_FLAG_HIDDEN);
    atualizar_dia_a_dia();
  }
}

void CalibrationMenuPanel::atualizar_dia_a_dia() {
  State *s = State::get_instance();
  auto z = s->get_data("/printer_state/probe/z_offset"_json_pointer);
  if (z.is_number()) {
    Marca::texto(altura_auto.detalhe, fmt::format("Automática · agora {:.3f} mm", z.template get<double>()).c_str());
  }
  auto ch = s->get_data("/printer_state/save_variables/variables/chapa_atual"_json_pointer);
  int chapa = ch.is_number() ? ch.template get<int>() : -1;
  Atalho *cs[2] = {&chapa_lisa, &chapa_textura};
  for (int i = 0; i < 2; i++) {
    bool em_uso = (i == chapa);
    lv_obj_set_style_border_width(cs[i]->cartao, em_uso ? 2 : 0, 0);
    lv_obj_set_style_border_color(cs[i]->cartao, lv_color_hex(Marca::LIMAO), 0);
    Marca::texto(cs[i]->detalhe, em_uso ? "Em uso agora" : "Toque ao colocar esta chapa");
  }
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
  atualizar_dia_a_dia();
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

  // ---- Dia a dia (macros do melhorias.cfg; elas mesmas recusam imprimindo) ----
  if (row == altura_auto.cartao) {
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    KUtils::confirmar(
      "Medir a altura do bico 3 vezes com o sensor de pressão?\n"
      "O bico esquenta e encosta de leve na mesa. Deixe a mesa vazia.",
      [this]() {
        ws.gcode_script("CALIBRAR_ALTURA_BICO");
        KUtils::notify_toast("Medindo a altura do bico (uns 4 min). No fim o Klipper reinicia.", 5000);
      });
    return;
  }
  if (row == medir_barra.cartao) {
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    KUtils::confirmar(
      "Medir a barra do bico (esquerda x direita) com o sensor?\n"
      "Os eixos se movem. Deixe a mesa vazia e as mãos longe.",
      [this]() {
        ws.gcode_script("MEDIR_BARRA");
        KUtils::notify_toast("Medindo a barra... o resultado aparece aqui.", 4000);
      });
    return;
  }
  if (row == chapa_lisa.cartao || row == chapa_textura.cartao) {
    if (KUtils::is_printing()) { KUtils::notify_locked(); return; }
    ws.gcode_script(row == chapa_lisa.cartao ? "CHAPA_LISA" : "CHAPA_TEXTURIZADA");
    // o State atualiza em seguida; a borda limão muda ao voltar para a página
    lv_timer_t *t = lv_timer_create([](lv_timer_t *tm) {
      ((CalibrationMenuPanel *)tm->user_data)->atualizar_dia_a_dia();
    }, 1500, this);
    lv_timer_set_repeat_count(t, 1);
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
