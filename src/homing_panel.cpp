#include "homing_panel.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "config.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
  // Segmentado de distância (o LVGL guarda o ponteiro do mapa: estático).
  static const char *mapa_distancia[] = {"0.1", "1", "10", "30", ""};
  static const double distancias[] = {0.1, 1, 10, 30};
  constexpr uint32_t N_DISTANCIAS = 4;
  constexpr uint32_t DISTANCIA_PADRAO = 2;  // 10 mm

  // Velocidade de antes: 100 mm/s (índice 2 do seletor antigo, que saiu da tela).
  constexpr double VELOCIDADE_MM_S = 100.0;

  constexpr lv_coord_t SETA = 48;

  void habilitar(lv_obj_t *obj, bool sim) {
    if (sim) {
      lv_obj_clear_state(obj, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
  }

  bool tem_eixo(const std::string &eixos, char eixo) {
    return eixos.find(eixo) != std::string::npos;
  }
}

HomingPanel::HomingPanel(KWebSocketClient &websocket_client, std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  // criado direto na aba: o tema deixa transparente (não desenha nada)
  , homing_cont(lv_obj_create(Navegacao::aba(Aba::MOVER)))
  , disco(NULL)
  , y_up_btn(NULL)
  , x_down_btn(NULL)
  , home_xy_btn(NULL)
  , x_up_btn(NULL)
  , y_down_btn(NULL)
  , pilula(NULL)
  , z_down_btn(NULL)
  , home_z_btn(NULL)
  , z_up_btn(NULL)
  , pos_x(NULL)
  , pos_y(NULL)
  , pos_z(NULL)
  , dist_titulo(NULL)
  , dist_sel(NULL)
  , aviso(NULL)
  , home_all_btn(NULL)
  , motoroff_btn(NULL)
  , dist_idx(DISTANCIA_PADRAO)
{
  Navegacao::colocar(homing_cont, Aba::MOVER);
  lv_obj_set_style_pad_all(homing_cont, 0, 0);
  lv_obj_set_style_bg_opa(homing_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(homing_cont, 0, 0);
  lv_obj_clear_flag(homing_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(homing_cont, LV_OBJ_FLAG_CLICKABLE);

  // ---- disco X/Y: 152 x 152 em (8,35) ----
  disco = lv_obj_create(homing_cont);
  Marca::cartao(disco);
  lv_obj_set_style_radius(disco, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_pos(disco, 8, 35);
  lv_obj_set_size(disco, 152, 152);
  lv_obj_clear_flag(disco, LV_OBJ_FLAG_CLICKABLE);

  // Posições iguais às do painel antigo: o botão "y_up" fica em cima e o
  // "y_down" embaixo; símbolo e rótulo seguem invert_y_direction (ajustar_setas).
  y_up_btn   = criar_seta(60, 37, LV_SYMBOL_DOWN, "Y+");
  x_down_btn = criar_seta(10, 87, LV_SYMBOL_LEFT, "X-");
  x_up_btn   = criar_seta(110, 87, LV_SYMBOL_RIGHT, "X+");
  y_down_btn = criar_seta(60, 137, LV_SYMBOL_UP, "Y-");

  home_xy_btn = Marca::criar_botao(homing_cont, LV_SYMBOL_HOME, Marca::Tipo::NORMAL, Marca::f20());
  lv_obj_set_pos(home_xy_btn, 60, 87);
  lv_obj_set_size(home_xy_btn, SETA, SETA);
  lv_obj_set_style_radius(home_xy_btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_event_cb(home_xy_btn, &HomingPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // ---- pílula Z: 52 x 152 em (168,35), raio 26 ----
  pilula = lv_obj_create(homing_cont);
  Marca::cartao(pilula);
  lv_obj_set_style_radius(pilula, 26, 0);
  lv_obj_set_pos(pilula, 168, 35);
  lv_obj_set_size(pilula, 52, 152);
  lv_obj_clear_flag(pilula, LV_OBJ_FLAG_CLICKABLE);

  // Como no painel antigo: "z_down" em cima e "z_up" embaixo; com
  // invert_z_direction (padrão) o de cima é Z+ e o de baixo Z-.
  z_down_btn = criar_seta(170, 37, LV_SYMBOL_DOWN, "Z-");
  home_z_btn = criar_seta(170, 87, LV_SYMBOL_HOME, "Z");
  z_up_btn   = criar_seta(170, 137, LV_SYMBOL_UP, "Z+");

  // ---- posição de agora: cartão 184 x 44 em (232,7) ----
  lv_obj_t *pos_cartao = Marca::criar_cartao(homing_cont, 232, 7, 184, 44);
  lv_obj_clear_flag(pos_cartao, LV_OBJ_FLAG_CLICKABLE);
  const char *nomes[] = {"X", "Y", "Z"};
  const lv_coord_t xs[] = {10, 70, 130};
  const lv_coord_t ls[] = {56, 56, 50};
  lv_obj_t **valores[] = {&pos_x, &pos_y, &pos_z};
  for (int i = 0; i < 3; i++) {
    lv_obj_t *nome = Marca::criar_rotulo(pos_cartao, nomes[i], Marca::f12(), Marca::GRAFITE);
    lv_obj_set_pos(nome, xs[i], 6);
    lv_obj_t *v = Marca::criar_rotulo(pos_cartao, "--", Marca::f14(), Marca::PAPEL);
    lv_obj_set_pos(v, xs[i], 22);
    lv_obj_set_width(v, ls[i]);
    lv_label_set_long_mode(v, LV_LABEL_LONG_CLIP);
    *valores[i] = v;
  }

  // ---- distância ----
  dist_titulo = Marca::criar_rotulo(homing_cont, "Distância (mm)", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(dist_titulo, 232, 58);
  lv_obj_set_width(dist_titulo, 184);

  dist_sel = lv_btnmatrix_create(homing_cont);
  lv_btnmatrix_set_map(dist_sel, mapa_distancia);
  Marca::estilo_segmentado(dist_sel);
  lv_obj_set_pos(dist_sel, 232, 75);
  lv_obj_set_size(dist_sel, 184, Marca::TOQUE);
  lv_btnmatrix_set_btn_ctrl_all(dist_sel, LV_BTNMATRIX_CTRL_CHECKABLE | LV_BTNMATRIX_CTRL_NO_REPEAT);
  lv_btnmatrix_set_one_checked(dist_sel, true);
  lv_btnmatrix_set_btn_ctrl(dist_sel, dist_idx, LV_BTNMATRIX_CTRL_CHECKED);
  lv_obj_add_event_cb(dist_sel, &HomingPanel::_handle_selector_cb, LV_EVENT_VALUE_CHANGED, this);

  // Trava de impressão: no lugar do rótulo e do segmentado de distância.
  // "Indisponível durante a impressão" mede 204 px na f12: vai em 2 linhas.
  aviso = Marca::criar_rotulo(homing_cont, "Indisponível durante a impressão", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(aviso, 232, 66);
  lv_obj_set_width(aviso, 184);
  lv_label_set_long_mode(aviso, LV_LABEL_LONG_WRAP);
  lv_obj_add_flag(aviso, LV_OBJ_FLAG_HIDDEN);

  // ---- botões largos ----
  home_all_btn = Marca::criar_botao(homing_cont, LV_SYMBOL_HOME " Origem XYZ", Marca::Tipo::NORMAL);
  lv_obj_set_pos(home_all_btn, 232, 127);
  lv_obj_set_size(home_all_btn, 184, Marca::TOQUE);
  lv_obj_add_event_cb(home_all_btn, &HomingPanel::_handle_callback, LV_EVENT_CLICKED, this);

  motoroff_btn = Marca::criar_botao(homing_cont, LV_SYMBOL_POWER " Soltar motores", Marca::Tipo::NORMAL);
  lv_obj_set_pos(motoroff_btn, 232, 175);
  lv_obj_set_size(motoroff_btn, 184, Marca::TOQUE);
  lv_obj_add_event_cb(motoroff_btn, &HomingPanel::_handle_callback, LV_EVENT_CLICKED, this);

  ajustar_setas();
  aplicar_estado();

  // O que o foreground() antigo fazia, agora ao mostrar a aba.
  Navegacao::ao_mostrar(Aba::MOVER, [this]() { atualizar(); });

  ws.register_notify_update(this);
}

HomingPanel::~HomingPanel() { }

lv_obj_t *HomingPanel::criar_seta(lv_coord_t x, lv_coord_t y, const char *simbolo, const char *rotulo) {
  lv_obj_t *btn = lv_btn_create(homing_cont);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, SETA, SETA);
  lv_obj_set_style_pad_all(btn, 0, 0);

  // símbolo e rótulo sem cor própria: herdam a do botão (GRAFITE desabilitado)
  lv_obj_t *s = lv_label_create(btn);
  lv_label_set_text(s, simbolo);
  lv_obj_set_style_text_font(s, Marca::f16(), 0);
  lv_obj_align(s, LV_ALIGN_CENTER, 0, -8);

  lv_obj_t *t = lv_label_create(btn);
  lv_label_set_text(t, rotulo);
  lv_obj_set_style_text_font(t, Marca::f12(), 0);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, 11);

  Marca::estilo_botao(btn, Marca::Tipo::NORMAL);
  lv_obj_add_event_cb(btn, &HomingPanel::_handle_callback, LV_EVENT_CLICKED, this);
  return btn;
}

void HomingPanel::consume(json &j) {
  // contains() não cria chaves vazias na mensagem que os outros painéis leem
  const auto p_eixos = "/params/0/toolhead/homed_axes"_json_pointer;
  const auto p_pos = "/params/0/toolhead/position"_json_pointer;
  const auto p_estado = "/params/0/print_stats/state"_json_pointer;
  bool tem_eixos = j.contains(p_eixos);
  bool tem_pos = j.contains(p_pos);
  bool tem_estado = j.contains(p_estado);
  if (!tem_eixos && !tem_pos && !tem_estado) {
    return;  // nada desta aba: nem prende o lv_lock
  }

  std::lock_guard<std::mutex> lock(lv_lock);
  if (tem_eixos && j[p_eixos].is_string()) {
    eixos = j[p_eixos].template get<std::string>();
  }
  if (tem_pos) {
    mostrar_posicao(j[p_pos]);
  }
  if (tem_eixos || tem_estado) {
    aplicar_estado();
  }
}

lv_obj_t *HomingPanel::get_container() {
  return homing_cont;
}

void HomingPanel::foreground() {
  // o gancho ao_mostrar(Aba::MOVER) faz o resto (atualizar)
  Navegacao::ir_para(Aba::MOVER);
}

void HomingPanel::atualizar() {
  ajustar_setas();

  auto v = State::get_instance()
    ->get_data("/printer_state/toolhead/homed_axes"_json_pointer);
  if (v.is_string()) {
    eixos = v.template get<std::string>();
  }

  auto p = State::get_instance()
    ->get_data("/printer_state/toolhead/position"_json_pointer);
  if (!p.is_null()) {
    mostrar_posicao(p);
  }

  aplicar_estado();
}

void HomingPanel::ajustar_setas() {
  bool invert_z = false;
  auto invert_z_json = Config::get_instance()->get_json("/invert_z_direction");
  if (invert_z_json.is_boolean()) { invert_z = invert_z_json.get<bool>(); }

  bool invert_y = false;
  auto invert_y_json = Config::get_instance()->get_json("/invert_y_direction");
  if (invert_y_json.is_boolean()) { invert_y = invert_y_json.get<bool>(); }

  // Mesma troca de antes: a imagem arrow_up/arrow_down virou LV_SYMBOL_UP/DOWN
  // e z_farther/z_closer viraram LV_SYMBOL_UP/DOWN.
  Marca::texto(lv_obj_get_child(y_up_btn, 0),   invert_y ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
  Marca::texto(lv_obj_get_child(y_down_btn, 0), invert_y ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
  Marca::texto(lv_obj_get_child(y_up_btn, 1),   invert_y ? "Y-" : "Y+");
  Marca::texto(lv_obj_get_child(y_down_btn, 1), invert_y ? "Y+" : "Y-");

  Marca::texto(lv_obj_get_child(z_up_btn, 0),   invert_z ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
  Marca::texto(lv_obj_get_child(z_down_btn, 0), invert_z ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
  Marca::texto(lv_obj_get_child(z_up_btn, 1),   invert_z ? "Z-" : "Z+");
  Marca::texto(lv_obj_get_child(z_down_btn, 1), invert_z ? "Z+" : "Z-");
}

void HomingPanel::mostrar_posicao(json &pos) {
  if (!pos.is_array() || pos.size() < 3) {
    return;
  }
  if (!pos[0].is_number() || !pos[1].is_number() || !pos[2].is_number()) {
    return;
  }
  Marca::texto(pos_x, fmt::format("{:.1f}", pos[0].template get<double>()).c_str());
  Marca::texto(pos_y, fmt::format("{:.1f}", pos[1].template get<double>()).c_str());
  Marca::texto(pos_z, fmt::format("{:.2f}", pos[2].template get<double>()).c_str());
}

bool HomingPanel::travado() {
  // Pausada pode (o bico está estacionado), como era antes no botão "Mover".
  return KUtils::is_printing() && !KUtils::is_paused();
}

void HomingPanel::aplicar_estado() {
  bool livre = !travado();

  // Origem (G28) e Soltar motores (M84) só sem impressão nenhuma: na pausa
  // eles tiram a referência dos eixos e o Retomar sai fora do lugar.
  bool sem_impressao = !KUtils::is_printing();
  habilitar(home_all_btn, sem_impressao);
  habilitar(home_xy_btn, sem_impressao);
  habilitar(home_z_btn, sem_impressao);
  habilitar(motoroff_btn, sem_impressao);

  bool x = livre && tem_eixo(eixos, 'x');
  bool y = livre && tem_eixo(eixos, 'y');
  bool z = livre && tem_eixo(eixos, 'z');
  habilitar(x_up_btn, x);
  habilitar(x_down_btn, x);
  habilitar(y_up_btn, y);
  habilitar(y_down_btn, y);
  habilitar(z_up_btn, z);
  habilitar(z_down_btn, z);

  // A troca de bandeira só redesenha quando muda.
  bool aviso_visivel = !lv_obj_has_flag(aviso, LV_OBJ_FLAG_HIDDEN);
  if (aviso_visivel == livre) {
    if (livre) {
      lv_obj_add_flag(aviso, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(dist_titulo, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(dist_sel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(aviso, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(dist_titulo, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(dist_sel, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void HomingPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  std::string move_op;

  // confere de novo no clique (o estado pode ter mudado agora)
  if (travado()) {
    KUtils::notify_locked();
    return;
  }
  // pausada: só as setas; origem e soltar motores continuam travados
  if (KUtils::is_printing()
      && (btn == home_all_btn || btn == home_xy_btn || btn == home_z_btn || btn == motoroff_btn)) {
    KUtils::notify_locked();
    return;
  }

  if (btn == home_all_btn) {
    spdlog::debug("home all pressed");
    ws.gcode_script("G28 X Y Z");
    return;
  } else if (btn == home_xy_btn) {
    spdlog::debug("home xy pressed");
    ws.gcode_script("G28 X Y");
    return;
  } else if (btn == home_z_btn) {
    // G28 Z (o mesmo do botão "Z" do Mainsail) só com X e Y na origem.
    std::string atuais;
    auto v = State::get_instance()
      ->get_data("/printer_state/toolhead/homed_axes"_json_pointer);
    if (v.is_string()) {
      atuais = v.template get<std::string>();
    }
    if (tem_eixo(atuais, 'x') && tem_eixo(atuais, 'y')) {
      spdlog::debug("home z pressed");
      ws.gcode_script("G28 Z");
    } else {
      spdlog::debug("home z pressed without x/y homed");
      KUtils::show_homing_prompt(ws);
    }
    return;
  } else if (btn == motoroff_btn) {
    spdlog::debug("motor off pressed");
    ws.gcode_script("M84");
    return;
  }

  // For movement commands, get current state and UI values
  auto state = State::get_instance()->get_data("/printer_state/toolhead"_json_pointer);
  if (state.is_null() || !state.contains("position") || !state.contains("axis_minimum") || !state.contains("axis_maximum")) {
    spdlog::error("Failed to get valid printer toolhead state for movement.");
    return;
  }

  try {
    double move_dist = distancias[dist_idx < N_DISTANCIAS ? dist_idx : DISTANCIA_PADRAO];
    int speed_mm_min = static_cast<int>(std::round(VELOCIDADE_MM_S * 60));

    auto current_pos = state["position"].get<std::vector<double>>();
    auto min_pos = state["axis_minimum"].get<std::vector<double>>();
    auto max_pos = state["axis_maximum"].get<std::vector<double>>();

    for (auto &pos : min_pos) {
      pos = std::max(pos, 0.0);
    }
    for (auto &pos : max_pos) {
      pos = std::round(pos / 10) * 10;
    }

    double current_x = current_pos[0];
    double current_y = current_pos[1];
    double current_z = current_pos[2];

    bool invert_z = false;
    auto invert_z_json = Config::get_instance()->get_json("/invert_z_direction");
    if (invert_z_json.is_boolean()) {
      invert_z = invert_z_json.get<bool>();
    }

    bool invert_y = false;
    auto invert_y_json = Config::get_instance()->get_json("/invert_y_direction");
    if (invert_y_json.is_boolean()) {
      invert_y = invert_y_json.get<bool>();
    }

    if (btn == x_up_btn) {
      spdlog::debug("x up pressed");
      double target_x = std::clamp(current_x + move_dist, min_pos[0], max_pos[0]);
      move_op = fmt::format("G0 X{:.4f} F{}", target_x, speed_mm_min);
    } else if (btn == x_down_btn) {
      spdlog::debug("x down pressed");
      double target_x = std::clamp(current_x - move_dist, min_pos[0], max_pos[0]);
      move_op = fmt::format("G0 X{:.4f} F{}", target_x, speed_mm_min);
    } else if (btn == y_up_btn) {
      spdlog::debug("y up pressed{}", invert_y ? " (inverted)" : "");
      double y_offset = invert_y ? -move_dist : +move_dist;
      double target_y = std::clamp(current_y + y_offset, min_pos[1], max_pos[1]);
      move_op = fmt::format("G0 Y{:.4f} F{}", target_y, speed_mm_min);
    } else if (btn == y_down_btn) {
      spdlog::debug("y down pressed{}", invert_y ? " (inverted)" : "");
      double y_offset = invert_y ? +move_dist : -move_dist;
      double target_y = std::clamp(current_y + y_offset, min_pos[1], max_pos[1]);
      move_op = fmt::format("G0 Y{:.4f} F{}", target_y, speed_mm_min);
    } else if (btn == z_up_btn) {
      spdlog::debug("z up pressed{}", invert_z ? " (inverted)" : "");
      double z_offset = invert_z ? -move_dist : +move_dist;
      double target_z = std::clamp(current_z + z_offset, min_pos[2], max_pos[2] / 10 * 10 );
      move_op = fmt::format("G0 Z{:.4f} F{}", target_z, speed_mm_min);
    } else if (btn == z_down_btn) {
      spdlog::debug("z down pressed{}", invert_z ? " (inverted)" : "");
      double z_offset = invert_z ? +move_dist : -move_dist;
      double target_z = std::clamp(current_z + z_offset, min_pos[2], max_pos[2] / 10 * 10);
      move_op = fmt::format("G0 Z{:.4f} F{}", target_z, speed_mm_min);
    } else {
      spdlog::debug("Unknown action button pressed");
    }
  } catch (const std::exception &e) {
    spdlog::error("Error processing movement command: {}", e.what());
    return;
  }

  if (!move_op.empty()) {
    std::string gcode = fmt::format(
      "SAVE_GCODE_STATE NAME=_guppy_movement\nG90\n{}\nRESTORE_GCODE_STATE NAME=_guppy_movement",
      move_op);
    ws.gcode_script(gcode);
  }
}

void HomingPanel::handle_selector_cb(lv_event_t *event) {
  lv_obj_t *obj = lv_event_get_target(event);
  uint32_t idx = lv_btnmatrix_get_selected_btn(obj);
  if (idx >= N_DISTANCIAS) {
    return;
  }
  dist_idx = idx;
  lv_btnmatrix_clear_btn_ctrl_all(obj, LV_BTNMATRIX_CTRL_CHECKED);
  lv_btnmatrix_set_btn_ctrl(obj, idx, LV_BTNMATRIX_CTRL_CHECKED);
  spdlog::debug("selector move distance index {}", idx);
}
