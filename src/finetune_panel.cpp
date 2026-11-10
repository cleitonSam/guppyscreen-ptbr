#include "finetune_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "numpad.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "utils.h"

#include <algorithm>
#include <cmath>

// Ajuste fino (Proposta B, quadro 06). Coordenadas da subtela 424 x 272.

namespace {
  // Mapas dos segmentados: o LVGL guarda o ponteiro, por isso estáticos.
  static const char *MAPA_PASSO_PCT[] = {"1", "5", "10", ""};
  static const int   PASSOS_PCT[]     = {1, 5, 10};
  static const char *MAPA_PASSO_MM[]  = {"0.01", "0.05", ""};

  // Limites só para o que é digitado no teclado numérico (os botões - e +
  // fazem a mesma conta de sempre).
  constexpr int VELOCIDADE_MAX = 300;
  constexpr int FLUXO_MAX = 200;

  lv_obj_t *criar_segmentado(lv_obj_t *pai, const char *mapa[], lv_coord_t x, lv_coord_t y,
                             lv_coord_t w, uint16_t escolhido, lv_event_cb_t cb, void *ud) {
    lv_obj_t *btnm = lv_btnmatrix_create(pai);
    lv_btnmatrix_set_map(btnm, mapa);
    lv_obj_set_pos(btnm, x, y);
    lv_obj_set_size(btnm, w, Marca::TOQUE);
    Marca::estilo_segmentado(btnm);
    lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CHECKABLE | LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_btnmatrix_set_one_checked(btnm, true);
    lv_btnmatrix_set_btn_ctrl(btnm, escolhido, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_add_event_cb(btnm, cb, LV_EVENT_VALUE_CHANGED, ud);
    return btnm;
  }

  int pct_do_estado(const char *ponteiro) {
    auto v = State::get_instance()->get_data(json::json_pointer(ponteiro));
    return v.is_number() ? static_cast<int>(std::lround(v.template get<double>() * 100)) : -1;
  }
}

FineTunePanel::FineTunePanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , passo_pct_idx(1)   // 5%
  , passo_mm_idx(0)    // 0.01 mm
  , tem_objetos(false)
{
  Navegacao::subtela(panel_cont);

  Marca::Cabecalho cab = Marca::criar_cabecalho(panel_cont, "Ajuste fino");
  titulo = cab.titulo;
  resumo = cab.direita;

  // "Objetos": no lugar do resumo quando a impressão tem objetos etiquetados
  objetos_btn = Marca::criar_botao(panel_cont, "Objetos", Marca::Tipo::FANTASMA);
  lv_obj_set_pos(objetos_btn, 320, 0);
  lv_obj_set_size(objetos_btn, 96, Marca::TOQUE);
  lv_obj_add_event_cb(objetos_btn, &FineTunePanel::_handle_callback, LV_EVENT_CLICKED, this);
  lv_obj_add_flag(objetos_btn, LV_OBJ_FLAG_HIDDEN);

  // 4 cartões 200 x 76
  velocidade = criar_linha(8,   52,  "Velocidade", true);
  fluxo      = criar_linha(216, 52,  "Fluxo", true);
  ventoinha  = criar_linha(8,   136, "Ventoinha da peça", true);
  altura     = criar_linha(216, 136, "Altura do bico (Z, mm)", false);

  // passos
  lv_obj_t *l = Marca::criar_rotulo(panel_cont, "Passo (%)", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(l, 8, 234);
  lv_obj_set_width(l, 60);
  passo_pct_sel = criar_segmentado(panel_cont, MAPA_PASSO_PCT, 70, 220, 132, passo_pct_idx,
                                   &FineTunePanel::_handle_callback, this);

  l = Marca::criar_rotulo(panel_cont, "Passo (mm)", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(l, 208, 234);
  lv_obj_set_width(l, 74);
  passo_mm_sel = criar_segmentado(panel_cont, MAPA_PASSO_MM, 284, 220, 132, passo_mm_idx,
                                  &FineTunePanel::_handle_callback, this);

  Navegacao::registrar_acao("ajuste_fino", [this](const std::string &) { foreground(); });

  ws.register_notify_update(this);
}

FineTunePanel::~FineTunePanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

FineTunePanel::Linha FineTunePanel::criar_linha(lv_coord_t x, lv_coord_t y, const char *nome,
                                                bool valor_tocavel) {
  Linha ln;
  ln.cartao = Marca::criar_cartao(panel_cont, x, y, 200, 76);
  lv_obj_clear_flag(ln.cartao, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *t = Marca::criar_rotulo(ln.cartao, nome, Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(t, 10, 8);
  lv_obj_set_width(t, 180);
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);

  ln.menos = Marca::criar_botao(ln.cartao, LV_SYMBOL_MINUS, Marca::Tipo::NORMAL);
  lv_obj_set_pos(ln.menos, 8, 26);
  lv_obj_set_size(ln.menos, 48, Marca::TOQUE);
  lv_obj_add_event_cb(ln.menos, &FineTunePanel::_handle_callback, LV_EVENT_CLICKED, this);

  // caixa do valor: fundo PRETO, borda 1 PRETO_3, raio 12, f20 no meio
  ln.valor = lv_obj_create(ln.cartao);
  lv_obj_set_pos(ln.valor, 56, 26);
  lv_obj_set_size(ln.valor, 88, Marca::TOQUE);
  lv_obj_set_style_bg_color(ln.valor, lv_color_hex(Marca::PRETO), 0);
  lv_obj_set_style_bg_opa(ln.valor, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ln.valor, 1, 0);
  lv_obj_set_style_border_color(ln.valor, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_set_style_radius(ln.valor, Marca::RAIO, 0);
  lv_obj_set_style_pad_all(ln.valor, 0, 0);
  lv_obj_clear_flag(ln.valor, LV_OBJ_FLAG_SCROLLABLE);
  if (valor_tocavel) {
    lv_obj_add_flag(ln.valor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(ln.valor, lv_color_hex(Marca::APERTADO), LV_STATE_PRESSED);
    lv_obj_add_event_cb(ln.valor, &FineTunePanel::_handle_callback, LV_EVENT_CLICKED, this);
  } else {
    lv_obj_clear_flag(ln.valor, LV_OBJ_FLAG_CLICKABLE);
  }

  ln.valor_txt = Marca::criar_rotulo(ln.valor, "--", Marca::f20(), Marca::PAPEL);   // f20: lê de longe
  lv_obj_center(ln.valor_txt);

  ln.mais = Marca::criar_botao(ln.cartao, LV_SYMBOL_PLUS, Marca::Tipo::NORMAL);
  lv_obj_set_pos(ln.mais, 144, 26);
  lv_obj_set_size(ln.mais, 48, Marca::TOQUE);
  lv_obj_add_event_cb(ln.mais, &FineTunePanel::_handle_callback, LV_EVENT_CLICKED, this);

  return ln;
}

void FineTunePanel::set_resumo(const std::string &txt) {
  resumo_txt = txt;
  Marca::texto(resumo, txt.c_str());
}

void FineTunePanel::set_objetos_cb(std::function<void()> cb) {
  objetos_cb = cb;
}

void FineTunePanel::escolher_ventoinha() {
  // A primeira ventoinha editável (as mesmas que a aba Ventoinhas deixa mexer).
  json fans = State::get_instance()->get_display_fans();
  fan_id.clear();
  if (fans.is_object() && !fans.empty()) {
    fan_id = fans.begin().key();
  }

  bool tem = !fan_id.empty();
  lv_obj_t *objs[] = {ventoinha.menos, ventoinha.mais, ventoinha.valor};
  for (lv_obj_t *o : objs) {
    if (tem) {
      lv_obj_clear_state(o, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(o, LV_STATE_DISABLED);
    }
  }
  if (!tem) {
    Marca::texto(ventoinha.valor_txt, "--");
  }
}

int FineTunePanel::ventoinha_atual() {
  if (fan_id.empty()) {
    return -1;
  }
  State *s = State::get_instance();
  auto v = s->get_data(json::json_pointer(fmt::format("/printer_state/{}/speed", fan_id)));
  if (!v.is_number()) {
    // hack for output_pin fans
    v = s->get_data(json::json_pointer(fmt::format("/printer_state/{}/value", fan_id)));
  }
  return v.is_number() ? static_cast<int>(v.template get<double>() * 100) : -1;
}

void FineTunePanel::mostrar_valores() {
  int v = pct_do_estado("/printer_state/gcode_move/speed_factor");
  if (v >= 0) Marca::texto(velocidade.valor_txt, fmt::format("{}%", v).c_str());

  v = pct_do_estado("/printer_state/gcode_move/extrude_factor");
  if (v >= 0) Marca::texto(fluxo.valor_txt, fmt::format("{}%", v).c_str());

  v = ventoinha_atual();
  if (v >= 0) Marca::texto(ventoinha.valor_txt, fmt::format("{}%", v).c_str());

  auto z = State::get_instance()->get_data("/printer_state/gcode_move/homing_origin/2"_json_pointer);
  if (z.is_number()) {
    Marca::texto(altura.valor_txt, fmt::format("{:+.2f}", z.template get<double>()).c_str());
  }
}

void FineTunePanel::atualizar_objetos() {
  auto objs = State::get_instance()->get_data("/printer_state/exclude_object/objects"_json_pointer);
  bool tem = objs.is_array() && !objs.empty();
  if (tem == tem_objetos) {
    return;
  }
  tem_objetos = tem;
  if (tem) {
    lv_obj_clear_flag(objetos_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(resumo, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(objetos_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(resumo, LV_OBJ_FLAG_HIDDEN);
  }
}

void FineTunePanel::foreground() {
  escolher_ventoinha();
  mostrar_valores();
  atualizar_objetos();
  Marca::texto(resumo, KUtils::is_printing() ? resumo_txt.c_str() : "");
  Navegacao::abrir(panel_cont);
}

void FineTunePanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto v = j["/params/0/gcode_move/homing_origin/2"_json_pointer];
  if (v.is_number()) {
    Marca::texto(altura.valor_txt, fmt::format("{:+.2f}", v.template get<double>()).c_str());
  }

  v = j["/params/0/gcode_move/speed_factor"_json_pointer];
  if (v.is_number()) {
    Marca::texto(velocidade.valor_txt, fmt::format("{}%",
      static_cast<int>(std::lround(v.template get<double>() * 100))).c_str());
  }

  v = j["/params/0/gcode_move/extrude_factor"_json_pointer];
  if (v.is_number()) {
    Marca::texto(fluxo.valor_txt, fmt::format("{}%",
      static_cast<int>(std::lround(v.template get<double>() * 100))).c_str());
  }

  if (!fan_id.empty()) {
    auto ptr_speed = json::json_pointer(fmt::format("/params/0/{}/speed", fan_id));
    auto ptr_value = json::json_pointer(fmt::format("/params/0/{}/value", fan_id));
    if (j.contains(ptr_speed) && j[ptr_speed].is_number()) {
      int f = static_cast<int>(j[ptr_speed].template get<double>() * 100);
      Marca::texto(ventoinha.valor_txt, fmt::format("{}%", f).c_str());
    } else if (j.contains(ptr_value) && j[ptr_value].is_number()) {
      int f = static_cast<int>(j[ptr_value].template get<double>() * 100);
      Marca::texto(ventoinha.valor_txt, fmt::format("{}%", f).c_str());
    }
  }

  if (j.contains("/params/0/exclude_object"_json_pointer)) {
    atualizar_objetos();
  }
}

void FineTunePanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *sel = lv_event_get_target(e);
    uint16_t idx = lv_btnmatrix_get_selected_btn(sel);
    if (idx == LV_BTNMATRIX_BTN_NONE) {
      return;
    }
    if (sel == passo_pct_sel) {
      passo_pct_idx = idx;
    } else if (sel == passo_mm_sel) {
      passo_mm_idx = idx;
    }
    return;
  }

  if (code != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (btn == velocidade.menos)       ajustar_velocidade(-1);
  else if (btn == velocidade.mais)   ajustar_velocidade(1);
  else if (btn == fluxo.menos)       ajustar_fluxo(-1);
  else if (btn == fluxo.mais)        ajustar_fluxo(1);
  else if (btn == ventoinha.menos)   ajustar_ventoinha(-1);
  else if (btn == ventoinha.mais)    ajustar_ventoinha(1);
  else if (btn == altura.menos)      ajustar_z(false);
  else if (btn == altura.mais)       ajustar_z(true);
  else if (btn == objetos_btn) {
    if (objetos_cb) objetos_cb();
  } else if (btn == velocidade.valor || btn == fluxo.valor || btn == ventoinha.valor) {
    Numpad *np = Numpad::compartilhado();
    if (np == nullptr) {
      return;
    }
    if (btn == velocidade.valor) {
      np->abrir(pct_do_estado("/printer_state/gcode_move/speed_factor"),
                [this](double v) { mandar_velocidade(std::max(1, std::min((int)v, VELOCIDADE_MAX))); });
    } else if (btn == fluxo.valor) {
      np->abrir(pct_do_estado("/printer_state/gcode_move/extrude_factor"),
                [this](double v) { mandar_fluxo(std::max(1, std::min((int)v, FLUXO_MAX))); });
    } else if (!fan_id.empty()) {
      np->abrir(ventoinha_atual(),
                [this](double v) { mandar_ventoinha(std::max(0, std::min((int)v, 100))); });
    }
  }
}

int FineTunePanel::passo_pct() {
  return PASSOS_PCT[std::min<uint32_t>(passo_pct_idx, 2)];
}

void FineTunePanel::mandar_velocidade(int pct) {
  spdlog::trace("speed {}", pct);
  ws.gcode_script(fmt::format("M220 S{}", pct));
}

void FineTunePanel::mandar_fluxo(int pct) {
  spdlog::trace("flow {}", pct);
  ws.gcode_script(fmt::format("M221 S{}", pct));
}

void FineTunePanel::ajustar_velocidade(int direcao) {
  // a mesma conta do handle_speed de antes
  auto spd_factor = State::get_instance()
    ->get_data("/printer_state/gcode_move/speed_factor"_json_pointer);
  if (spd_factor.is_number()) {
    int32_t step = direcao * passo_pct();
    int32_t new_speed = static_cast<int32_t>(spd_factor.template get<double>() * 100 + step);
    new_speed = std::max(new_speed, 1);
    mandar_velocidade(new_speed);
  }
}

void FineTunePanel::ajustar_fluxo(int direcao) {
  // a mesma conta do handle_flow de antes
  auto extrude_factor = State::get_instance()
    ->get_data("/printer_state/gcode_move/extrude_factor"_json_pointer);
  if (extrude_factor.is_number()) {
    int32_t step = direcao * passo_pct();
    int32_t new_flow = static_cast<int32_t>(extrude_factor.template get<double>() * 100 + step);
    new_flow = std::max(new_flow, 1);
    mandar_fluxo(new_flow);
  }
}

void FineTunePanel::ajustar_ventoinha(int direcao) {
  int atual = ventoinha_atual();
  if (atual < 0) {
    return;
  }
  int novo = std::max(0, std::min(100, atual + direcao * passo_pct()));
  mandar_ventoinha(novo);
}

void FineTunePanel::mandar_ventoinha(int pct) {
  if (fan_id.empty()) {
    return;
  }
  // Comando pelo tipo, igual ao fan_panel.cpp.
  if (fan_id == "fan") {
    double v = 255 * (double)pct / 100.0;
    ws.gcode_script(fmt::format("M106 S{}", v));
  } else if (fan_id.rfind("output_pin ", 0) == 0) {
    std::string fan_name = KUtils::get_obj_name(fan_id);
    double v = 255 * (double)pct / 100.0;
    ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE={}", fan_name, v));
  } else {
    std::string fan_name = KUtils::get_obj_name(fan_id);
    double v = (double)pct / 100.0;
    ws.gcode_script(fmt::format("SET_FAN_SPEED FAN={} SPEED={}", fan_name, v));
  }
  Marca::texto(ventoinha.valor_txt, fmt::format("{}%", pct).c_str());
}

void FineTunePanel::ajustar_z(bool subir) {
  // SET_GCODE_OFFSET ... MOVE=1 needs a homed axis. Unhomed, the offset would
  // change live but the move fails (and the save macro aborts), so block and
  // prompt to home first instead.
  if (!KUtils::is_homed()) {
    KUtils::show_homing_prompt(ws);
    return;
  }

  const char *step = lv_btnmatrix_get_btn_text(passo_mm_sel, passo_mm_idx);
  if (step == NULL) {
    return;
  }
  // SET_GCODE_OFFSET operates in Klipper's normalized gcode frame, where
  // negative Z_ADJUST always means closer to the bed - independent of the
  // printer's physical stepper wiring. invert_z_direction only corrects the
  // on-screen jog arrows for manual G0 moves and must not be applied here
  // (see homing_panel.cpp).
  const char *sign = subir ? "+" : "-";
  spdlog::trace("clicked z-offset {}{}", sign, step);
  ws.gcode_script(fmt::format("SET_GCODE_OFFSET Z_ADJUST={}{} MOVE=1", sign, step));
}
