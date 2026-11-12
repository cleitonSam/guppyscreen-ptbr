#include "zoffset_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "utils.h"

#include <cmath>
#include <cstdlib>

// Altura do bico: subtela 424 x 272 com cabeçalho.
//   y =  52  cartão 408 x 48: valor f20 à esquerda; à direita o aviso de
//            salvamento (imprimindo) ou o botão "Gravar altura" (parada)
//   y = 108  "Subir  (mais longe da mesa)"   408 x 44
//   y = 160  "Descer  (mais perto da mesa)"  408 x 44
//   y = 212  "Passo (mm)" + segmentado 220 x 44 + "Zerar" / "Ir ao centro" 104 x 44
//
// Dois usos na mesma tela (detalhes em zoffset_panel.h):
//   imprimindo -> ajuste fino: passos pequenos, cada toque já fica salvo;
//   parada     -> teste do papel: o bico anda por movimento comum para uma altura
//                 exata (nada fica salvo no caminho) e "Gravar altura" guarda só a
//                 diferença para o zero do sensor (macro GRAVAR_ALTURA_PAPEL).

namespace {
  static const char *MAPA_FINO[] = {"0.001", "0.005", "0.01", "0.025", "0.05", ""};
  static const char *MAPA_PAPEL[] = {"0.01", "0.05", "0.1", "0.5", "1", ""};

  // Teste do papel, em mm. As alturas "do bico" são sobre o ponto gravado da
  // última vez (Z do gcode); FUNDO e TETO são sobre o zero do sensor, iguais aos
  // limites da macro GRAVAR_ALTURA_PAPEL.
  const double PAPEL_PERTO = 1.0;     // daqui para baixo só passo de 0,1 ou menor
  const double PASSO_GRANDE = 0.5;    // passos a partir deste não chegam perto da mesa
  const double PAPEL_FUNDO = -0.4;    // mais baixo que isso não é ajuste fino
  const double PAPEL_TETO = 1.0;      // mais alto que isso ainda não é hora de gravar
  const double CENTRO_X = 110.0;      // onde a ida à origem deixa o bico na KE
  const double CENTRO_Y = 110.0;
  const double CENTRO_Z = 5.0;
  const uint32_t ESPERA_ESTADO_MS = 1500;
  const uint32_t PASSO_PAPEL_PADRAO = 2;   // 0.1 mm
}

ZOffsetPanel::ZOffsetPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , step_idx(2)   // 0.01 mm
{
  Navegacao::subtela(panel_cont);
  altura_label = Marca::criar_cabecalho(panel_cont, "Altura do bico").direita;

  // valor ao vivo + aviso de que salva sozinho
  lv_obj_t *card = Marca::criar_cartao(panel_cont, 8, 52, 408, 48);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
  value_label = Marca::criar_rotulo(card, "0.000 mm", Marca::f20(), Marca::PAPEL);
  lv_obj_set_pos(value_label, 12, 13);
  lv_obj_set_width(value_label, 120);
  lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);

  // the Helper Script's Save Z-Offset macros persist every adjustment to
  // variables.cfg automatically, so there is no Save button - just say so.
  auto_save_hint = Marca::criar_rotulo(card, LV_SYMBOL_OK "  Os ajustes são salvos automaticamente",
                                       Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(auto_save_hint, 128, 17);
  lv_obj_set_width(auto_save_hint, 268);
  lv_label_set_long_mode(auto_save_hint, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(auto_save_hint, LV_TEXT_ALIGN_RIGHT, 0);

  // teste do papel: no lugar do aviso, o botão que grava a altura
  gravar_btn = Marca::criar_botao(card, LV_SYMBOL_SAVE "  Gravar altura", Marca::Tipo::DESTAQUE);
  lv_obj_set_pos(gravar_btn, 228, 4);
  lv_obj_set_size(gravar_btn, 172, 40);
  lv_obj_add_event_cb(gravar_btn, &ZOffsetPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // raise / lower buttons: full-width, clear physical direction
  up_btn = Marca::criar_botao(panel_cont, LV_SYMBOL_UP "  Subir  (mais longe da mesa)", Marca::Tipo::NORMAL);
  lv_obj_set_pos(up_btn, 8, 108);
  lv_obj_set_size(up_btn, 408, Marca::TOQUE);
  lv_obj_add_event_cb(up_btn, &ZOffsetPanel::_handle_callback, LV_EVENT_CLICKED, this);

  down_btn = Marca::criar_botao(panel_cont, LV_SYMBOL_DOWN "  Descer  (mais perto da mesa)", Marca::Tipo::NORMAL);
  lv_obj_set_pos(down_btn, 8, 160);
  lv_obj_set_size(down_btn, 408, Marca::TOQUE);
  lv_obj_add_event_cb(down_btn, &ZOffsetPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // passo
  lv_obj_t *passo = Marca::criar_rotulo(panel_cont, "Passo (mm)", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(passo, 8, 226);
  lv_obj_set_width(passo, 74);

  step_selector = lv_btnmatrix_create(panel_cont);
  lv_obj_set_pos(step_selector, 84, 212);
  lv_obj_set_size(step_selector, 220, Marca::TOQUE);
  Marca::estilo_segmentado(step_selector);
  lv_obj_add_event_cb(step_selector, &ZOffsetPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  // mesmo lugar, um para cada uso: "Zerar" o ajuste fino / levar o bico ao centro
  reset_btn = Marca::criar_botao(panel_cont, LV_SYMBOL_REFRESH "  Zerar", Marca::Tipo::NORMAL);
  lv_obj_set_pos(reset_btn, 312, 212);
  lv_obj_set_size(reset_btn, 104, Marca::TOQUE);
  lv_obj_add_event_cb(reset_btn, &ZOffsetPanel::_handle_callback, LV_EVENT_CLICKED, this);

  centro_btn = Marca::criar_botao(panel_cont, "Ir ao centro", Marca::Tipo::NORMAL);
  lv_obj_set_pos(centro_btn, 312, 212);
  lv_obj_set_size(centro_btn, 104, Marca::TOQUE);
  lv_obj_add_event_cb(centro_btn, &ZOffsetPanel::_handle_callback, LV_EVENT_CLICKED, this);

  aplicar_modo(false);   // o mapa de passos e os botões certos entram ao abrir a tela

  Navegacao::registrar_acao("altura_do_bico", [this](const std::string &) { foreground(); });

  ws.register_notify_update(this);
}

ZOffsetPanel::~ZOffsetPanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

double ZOffsetPanel::cur_offset() {
  auto v = State::get_instance()->get_data(
    "/printer_state/gcode_move/homing_origin/2"_json_pointer);
  return v.is_number() ? v.template get<double>() : 0.0;
}

// O ajuste fino: número grande enquanto imprime; no teste do papel vai para o
// cabeçalho (o número grande passa a ser a altura do bico).
void ZOffsetPanel::update_value(double z) {
  if (modo == 1) {
    Marca::texto(altura_label, fmt::format("Ajuste fino {:+.3f} mm", z).c_str());
  } else {
    Marca::texto(value_label, fmt::format("{:.3f} mm", z).c_str());
  }
}

void ZOffsetPanel::apply_step(bool raise) {
  // SET_GCODE_OFFSET ... MOVE=1 needs a homed axis: unhomed, Klipper still sets
  // the offset (the readout would change) but the move fails and the Helper
  // Script's save macro aborts before SAVE_VARIABLE - so it'd be applied live,
  // not moved, and never saved. Guard before sending so the three never drift.
  if (!KUtils::is_homed()) {
    KUtils::show_homing_prompt(ws);
    return;
  }

  const char *step = lv_btnmatrix_get_btn_text(step_selector, step_idx);
  if (step == NULL) {
    return;
  }

  if (modo == 1) {
    passo_papel(raise, std::atof(step));
    return;
  }

  // SET_GCODE_OFFSET operates in Klipper's normalized gcode frame, where
  // negative Z_ADJUST always means closer to the bed - independent of the
  // printer's physical stepper wiring. invert_z_direction only corrects the
  // on-screen jog arrows for manual G0 moves (see homing_panel.cpp) and must
  // not be applied here.
  const char *sign = raise ? "+" : "-";
  ws.gcode_script(fmt::format("SET_GCODE_OFFSET Z_ADJUST={}{} MOVE=1", sign, step));
}

void ZOffsetPanel::aplicar_modo(bool papel) {
  if (modo == (papel ? 1 : 0)) {
    return;
  }
  modo = papel ? 1 : 0;
  previsto_valido = false;

  // lv_btnmatrix guarda o ponteiro do mapa (estático) e zera as marcações
  lv_btnmatrix_set_map(step_selector, papel ? MAPA_PAPEL : MAPA_FINO);
  lv_btnmatrix_set_btn_ctrl_all(step_selector, LV_BTNMATRIX_CTRL_CHECKABLE | LV_BTNMATRIX_CTRL_NO_REPEAT);
  lv_btnmatrix_set_one_checked(step_selector, true);
  step_idx = 2;   // 0.1 mm no papel, 0.01 mm no ajuste fino
  lv_btnmatrix_set_btn_ctrl(step_selector, step_idx, LV_BTNMATRIX_CTRL_CHECKED);

  if (papel) {
    lv_obj_add_flag(auto_save_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(reset_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(gravar_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(centro_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(value_label, 210);
    mostrar_altura();
  } else {
    lv_obj_clear_flag(auto_save_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(reset_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(gravar_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(centro_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(value_label, 120);
    lv_label_set_text(altura_label, "");
  }
  update_value(cur_offset());
}

// ---------------------------------------------------------------------------
// Teste do papel
// ---------------------------------------------------------------------------

// Altura do bico sobre o ponto gravado da última vez (Z do gcode). Logo depois
// de um movimento vale a altura mandada: o estado ainda traz a anterior.
double ZOffsetPanel::altura_papel(bool *ok) {
  if (previsto_valido && lv_tick_elaps(previsto_ms) < ESPERA_ESTADO_MS) {
    *ok = true;
    return z_previsto;
  }
  previsto_valido = false;
  auto v = State::get_instance()->get_data(
    "/printer_state/gcode_move/gcode_position/2"_json_pointer);
  *ok = v.is_number();
  return *ok ? v.template get<double>() : 0.0;
}

void ZOffsetPanel::mostrar_altura() {
  if (modo != 1) {
    return;
  }
  bool ok = false;
  double g = altura_papel(&ok);
  if (!ok || !KUtils::is_homed()) {
    Marca::texto(value_label, "Bico a -- mm");   // sem origem não há altura
    return;
  }
  if (std::fabs(g) < 0.005) {
    g = 0.0;   // nada de "-0.00"
  }
  Marca::texto(value_label, fmt::format("Bico a {:.2f} mm", g).c_str());
}

// Todo movimento do teste do papel vai para uma altura EXATA (G90). Dois toques
// seguidos, com o estado atrasado, pedem no máximo a mesma altura duas vezes:
// nunca somam uma descida que ninguém conferiu.
void ZOffsetPanel::mover_papel(const std::string &movimentos, double alvo) {
  ws.gcode_script(fmt::format(
    "SAVE_GCODE_STATE NAME=_guppy_papel\nG90\n{}\nRESTORE_GCODE_STATE NAME=_guppy_papel", movimentos));
  z_previsto = alvo;
  previsto_valido = true;
  previsto_ms = lv_tick_get();
  mostrar_altura();
}

void ZOffsetPanel::passo_papel(bool raise, double passo) {
  bool ok = false;
  double g = altura_papel(&ok);
  if (!ok || passo <= 0.0) {
    return;
  }
  double alvo = raise ? g + passo : g - passo;

  if (raise) {
    auto teto = State::get_instance()->get_data("/printer_state/toolhead/axis_maximum/2"_json_pointer);
    if (teto.is_number() && alvo + cur_offset() > teto.template get<double>() - 1.0) {
      return;   // já está no alto do eixo
    }
  } else {
    // perto da mesa não se desce de passo grande: troca sozinho para 0,1 e espera o próximo toque
    if (passo >= PASSO_GRANDE - 1e-9 && alvo < PAPEL_PERTO - 1e-6) {
      step_idx = PASSO_PAPEL_PADRAO;
      lv_btnmatrix_set_btn_ctrl(step_selector, step_idx, LV_BTNMATRIX_CTRL_CHECKED);
      KUtils::notify_toast("Perto da mesa: passo trocado para 0.1 mm.", 2500);
      return;
    }
    // abaixo disto a macro não grava: não é ajuste fino, é a altura do bico
    if (alvo + cur_offset() < PAPEL_FUNDO - 1e-6) {
      KUtils::avisar("O bico chegou ao limite do ajuste fino.\n"
                     "Se o papel ainda passa solto, refaça a altura do bico em Calibração, Completa, passo 2.");
      return;
    }
  }

  mover_papel(fmt::format("G1 Z{:.3f} F600", alvo), alvo);
}

void ZOffsetPanel::ir_ao_centro() {
  bool ok = false;
  double g = altura_papel(&ok);
  if (!ok) {
    return;
  }
  // sobe antes de andar se estiver baixo; desce para 5 mm só já no centro
  std::string mov;
  if (g < CENTRO_Z) {
    mov += fmt::format("G1 Z{:.3f} F900\n", CENTRO_Z);
  }
  mov += fmt::format("G1 X{:.1f} Y{:.1f} F6000\nG1 Z{:.3f} F900", CENTRO_X, CENTRO_Y, CENTRO_Z);
  mover_papel(mov, CENTRO_Z);
}

void ZOffsetPanel::gravar_altura() {
  bool ok = false;
  double g = altura_papel(&ok);
  if (!ok) {
    return;
  }
  if (g + cur_offset() > PAPEL_TETO) {
    KUtils::avisar(fmt::format("O bico ainda está a {:.2f} mm da mesa.\n"
                               "Desça até o papel raspar de leve e só então grave.", g));
    return;
  }
  // a macro confere de novo a distância, grava e só então sobe o bico
  KUtils::confirmar("O papel está raspando de leve no bico?\nGravo esta altura e subo o bico.",
    [this]() {
      previsto_valido = false;
      ws.gcode_script("GRAVAR_ALTURA_PAPEL");
    });
}

// ---------------------------------------------------------------------------

void ZOffsetPanel::foreground() {
  aplicar_modo(!KUtils::is_printing());
  update_value(cur_offset());
  mostrar_altura();
  Navegacao::abrir(panel_cont);
}

void ZOffsetPanel::consume(json &j) {
  // contains() não cria chaves vazias na mensagem que os outros painéis leem
  const auto p_fino = "/params/0/gcode_move/homing_origin/2"_json_pointer;
  const auto p_estado = "/params/0/print_stats/state"_json_pointer;
  const auto p_altura = "/params/0/gcode_move/gcode_position/2"_json_pointer;
  const auto p_eixos = "/params/0/toolhead/homed_axes"_json_pointer;
  bool tem_fino = j.contains(p_fino);
  bool tem_estado = j.contains(p_estado);
  bool tem_altura = j.contains(p_altura);
  bool tem_eixos = j.contains(p_eixos);
  if (!tem_fino && !tem_estado && !tem_altura && !tem_eixos && !previsto_valido) {
    return;  // nada desta tela: nem prende o lv_lock
  }

  std::lock_guard<std::mutex> lock(lv_lock);
  if (tem_estado && j[p_estado].is_string()) {
    std::string e = j[p_estado].template get<std::string>();
    aplicar_modo(e != "printing" && e != "paused");
  }
  if (tem_fino && j[p_fino].is_number()) {
    update_value(j[p_fino].template get<double>());
  }
  if (modo != 1) {
    return;
  }

  // A altura mandada vale até o estado mostrar a mesma, ou até passar a espera
  // (comando recusado pela impressora: volta a valer o que o estado diz).
  bool venceu = false;
  if (previsto_valido) {
    bool igual = tem_altura && j[p_altura].is_number()
      && std::fabs(j[p_altura].template get<double>() - z_previsto) < 0.0005;
    if (igual || lv_tick_elaps(previsto_ms) >= ESPERA_ESTADO_MS) {
      previsto_valido = false;
      venceu = true;
    }
  }
  if (!previsto_valido && (tem_altura || tem_eixos || venceu)) {
    mostrar_altura();
  }
}

void ZOffsetPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *sel = lv_event_get_target(e);
    if (sel == step_selector) {
      uint16_t idx = lv_btnmatrix_get_selected_btn(sel);
      if (idx != LV_BTNMATRIX_BTN_NONE) {
        step_idx = idx;
      }
    }
    return;
  }

  if (code != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (btn == up_btn) {
    apply_step(true);
  } else if (btn == down_btn) {
    apply_step(false);
  } else if (btn == reset_btn || btn == centro_btn || btn == gravar_btn) {
    if (!KUtils::is_homed()) {
      KUtils::show_homing_prompt(ws);
      return;
    }
    if (btn == reset_btn) {
      ws.gcode_script("SET_GCODE_OFFSET Z=0 MOVE=1");
    } else if (btn == centro_btn) {
      // desce sobre o centro da mesa: uma peça esquecida ali levaria a batida
      KUtils::confirmar("Levo o bico ao centro, a 5 mm da mesa.\nA mesa está vazia?",
        [this]() { ir_ao_centro(); });
    } else {
      gravar_altura();
    }
  }
}
