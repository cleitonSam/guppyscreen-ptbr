#include "firmware_retraction_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "spdlog/spdlog.h"

#include <cmath>

// Medidas (subtela 424 x 272; o corpo começa em y = 44, logo abaixo do
// cabeçalho, e as coordenadas abaixo são relativas ao corpo).
static const lv_coord_t CORPO_Y = 44;
static const lv_coord_t CARTAO_L = 200, CARTAO_A = 76;
static const lv_coord_t PASSO_Y = 176;   // 220 na subtela

FirmwareRetractionPanel::FirmwareRetractionPanel(KWebSocketClient &websocket_client, std::mutex &l)
  : NotifyConsumer(l)
  , ws(websocket_client)
  , panel_cont(lv_obj_create(lv_scr_act()))
  , body(lv_obj_create(panel_cont))
  , empty_cont(lv_obj_create(panel_cont))
  , reset_all_btn(NULL)
  , length_step_selector(body, "Passo do comprimento (mm)", {"0.05", "0.10", "0.25", ""}, 1,
      &FirmwareRetractionPanel::_handle_callback, this)
  , speed_step_selector(body, "Passo da velocidade (mm/s)", {"1", "5", "10", ""}, 1,
      &FirmwareRetractionPanel::_handle_callback, this)
{
  fields[0] = {"Compr. da retração",     "RETRACT_LENGTH",         "retract_length",         "mm",   false, nullptr, nullptr, nullptr};
  fields[1] = {"Velocidade da retração", "RETRACT_SPEED",          "retract_speed",          "mm/s", true,  nullptr, nullptr, nullptr};
  fields[2] = {"Extra ao voltar",        "UNRETRACT_EXTRA_LENGTH", "unretract_extra_length", "mm",   false, nullptr, nullptr, nullptr};
  fields[3] = {"Velocidade ao voltar",   "UNRETRACT_SPEED",        "unretract_speed",        "mm/s", true,  nullptr, nullptr, nullptr};

  // subtela com cabeçalho "Retração" (voltar = fechar a subtela)
  Navegacao::subtela(panel_cont);
  Marca::Cabecalho cab = Marca::criar_cabecalho(panel_cont, "Retração");
  lv_obj_set_width(cab.titulo, 200);

  // "Restaurar tudo" à direita do cabeçalho (fantasma 140 x 44)
  reset_all_btn = Marca::criar_botao(panel_cont, LV_SYMBOL_REFRESH " Restaurar tudo", Marca::Tipo::FANTASMA);
  lv_obj_set_pos(reset_all_btn, 276, 0);
  lv_obj_set_size(reset_all_btn, 140, Marca::TOQUE);
  lv_obj_add_event_cb(reset_all_btn, &FirmwareRetractionPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // ---- corpo (controles ao vivo) ----
  lv_obj_set_pos(body, 0, CORPO_Y);
  lv_obj_set_size(body, Marca::CONT_L, Marca::TELA_A - CORPO_Y);
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

  // 4 cartões 200 x 76 em (8,8), (216,8), (8,92), (216,92) do corpo
  build_card(fields[0], 8, 8);
  build_card(fields[1], 216, 8);
  build_card(fields[2], 8, 92);
  build_card(fields[3], 216, 92);

  // ---- passos: "Passo (mm)" + segmentado 132 x 44; "Passo (mm/s)" + segmentado ----
  lv_obj_t *rot = Marca::criar_rotulo(body, "Passo (mm)", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(rot, 8, PASSO_Y + 7);
  lv_obj_set_width(rot, 58);

  length_step_selector.sem_titulo();
  lv_obj_set_pos(length_step_selector.get_container(), 70, PASSO_Y);
  lv_obj_set_size(length_step_selector.get_container(), 132, Marca::TOQUE);
  lv_obj_set_size(length_step_selector.get_selector(), 132, Marca::TOQUE);

  rot = Marca::criar_rotulo(body, "Passo (mm/s)", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(rot, 216, PASSO_Y + 7);
  lv_obj_set_width(rot, 58);

  speed_step_selector.sem_titulo();
  lv_obj_set_pos(speed_step_selector.get_container(), 278, PASSO_Y);
  lv_obj_set_size(speed_step_selector.get_container(), 132, Marca::TOQUE);
  lv_obj_set_size(speed_step_selector.get_selector(), 132, Marca::TOQUE);

  // ---- empty state ----
  lv_obj_set_pos(empty_cont, 0, CORPO_Y);
  lv_obj_set_size(empty_cont, Marca::CONT_L, Marca::TELA_A - CORPO_Y);
  lv_obj_set_style_pad_all(empty_cont, 16, 0);
  lv_obj_clear_flag(empty_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(empty_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(empty_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *empty_lbl = Marca::criar_rotulo(empty_cont,
    "A retração pelo firmware não está ativada.\n\n"
    "Adicione a seção [firmware_retraction]\n"
    "ao printer.cfg e reinicie o Klipper.", Marca::f14(), Marca::PAPEL);
  lv_obj_set_width(empty_lbl, LV_PCT(100));
  lv_obj_set_style_text_align(empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(empty_cont, LV_OBJ_FLAG_HIDDEN);

  ws.register_notify_update(this);
}

FirmwareRetractionPanel::~FirmwareRetractionPanel() {
  if (panel_cont != NULL) {
    lv_obj_del(panel_cont);
    panel_cont = NULL;
  }
  ws.unregister_notify_update(this);
}

void FirmwareRetractionPanel::build_card(Field &f, lv_coord_t x, lv_coord_t y) {
  // cartão como o do Ajuste fino: nome 12 G em (10,8); "-" 48 x 44 em (8,26);
  // valor 88 x 44 em (56,26) (fundo PRETO, borda 1 PRETO_3, f16); "+" em (144,26)
  lv_obj_t *card = Marca::criar_cartao(body, x, y, CARTAO_L, CARTAO_A);

  lv_obj_t *name = Marca::criar_rotulo(card, f.name, Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(name, 10, 8);
  lv_obj_set_width(name, 180);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

  f.minus = Marca::criar_botao(card, LV_SYMBOL_MINUS, Marca::Tipo::NORMAL, Marca::f16());
  lv_obj_set_pos(f.minus, 8, 26);
  lv_obj_set_size(f.minus, 48, Marca::TOQUE);
  lv_obj_add_event_cb(f.minus, &FirmwareRetractionPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // o valor é um rótulo só, com a moldura no próprio rótulo (um objeto a menos)
  f.value = Marca::criar_rotulo(card, "-", Marca::f16(), Marca::PAPEL);
  lv_obj_set_pos(f.value, 56, 26);
  lv_obj_set_size(f.value, 88, Marca::TOQUE);
  lv_label_set_long_mode(f.value, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(f.value, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(f.value, lv_color_hex(Marca::PRETO), 0);
  lv_obj_set_style_bg_opa(f.value, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(f.value, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_set_style_border_width(f.value, 1, 0);
  lv_obj_set_style_radius(f.value, Marca::RAIO, 0);
  lv_obj_set_style_pad_hor(f.value, 0, 0);
  lv_obj_set_style_pad_top(f.value, 12, 0);   // (44 - 2 de borda - 18 da linha) / 2

  f.plus = Marca::criar_botao(card, LV_SYMBOL_PLUS, Marca::Tipo::NORMAL, Marca::f16());
  lv_obj_set_pos(f.plus, 144, 26);
  lv_obj_set_size(f.plus, 48, Marca::TOQUE);
  lv_obj_add_event_cb(f.plus, &FirmwareRetractionPanel::_handle_callback, LV_EVENT_CLICKED, this);
}

double FirmwareRetractionPanel::cur_value(const Field &f) {
  auto v = State::get_instance()->get_data(
    json::json_pointer(std::string("/printer_state/firmware_retraction/") + f.key));
  return v.is_number() ? v.template get<double>() : NAN;
}

double FirmwareRetractionPanel::config_default(const Field &f) {
  auto v = State::get_instance()->get_data(
    json::json_pointer(std::string("/printer_state/configfile/settings/firmware_retraction/") + f.key));
  return v.is_number() ? v.template get<double>() : NAN;
}

void FirmwareRetractionPanel::send_field(const Field &f, double value) {
  value = value < 0 ? 0 : value;
  if (f.is_speed) {
    ws.gcode_script(fmt::format("SET_RETRACTION {}={}", f.param, static_cast<int>(std::lround(value))));
  } else {
    ws.gcode_script(fmt::format("SET_RETRACTION {}={:.2f}", f.param, value));
  }
}

void FirmwareRetractionPanel::apply_step(Field &f, bool up) {
  double cur = cur_value(f);
  if (std::isnan(cur)) return;
  Selector &sel = f.is_speed ? speed_step_selector : length_step_selector;
  double step = std::stod(lv_btnmatrix_get_btn_text(sel.get_selector(), sel.get_selected_idx()));
  send_field(f, cur + (up ? step : -step));
}

void FirmwareRetractionPanel::update_from(json &fr) {
  if (fr.is_null()) return;
  for (auto &f : fields) {
    auto v = fr[f.key];
    if (!v.is_number()) continue;
    if (f.is_speed) {
      Marca::texto(f.value,
        fmt::format("{} {}", static_cast<int>(std::lround(v.template get<double>())), f.unit).c_str());
    } else {
      Marca::texto(f.value,
        fmt::format("{:.2f} {}", v.template get<double>(), f.unit).c_str());
    }
  }
}

void FirmwareRetractionPanel::refresh_values() {
  auto fr = State::get_instance()->get_data("/printer_state/firmware_retraction"_json_pointer);
  update_from(fr);
}

void FirmwareRetractionPanel::foreground() {
  auto fr = State::get_instance()->get_data("/printer_state/firmware_retraction"_json_pointer);
  if (!fr.is_null()) {
    lv_obj_clear_flag(body, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(reset_all_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(empty_cont, LV_OBJ_FLAG_HIDDEN);
    refresh_values();
  } else {
    lv_obj_add_flag(body, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(reset_all_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(empty_cont, LV_OBJ_FLAG_HIDDEN);
  }
  Navegacao::abrir(panel_cont);
}

void FirmwareRetractionPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto fr = j["/params/0/firmware_retraction"_json_pointer];
  update_from(fr);
}

void FirmwareRetractionPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *sel = lv_event_get_target(e);
    uint32_t idx = lv_btnmatrix_get_selected_btn(sel);
    if (sel == length_step_selector.get_selector()) {
      length_step_selector.set_selected_idx(idx);
    } else if (sel == speed_step_selector.get_selector()) {
      speed_step_selector.set_selected_idx(idx);
    }
    return;
  }

  if (code != LV_EVENT_CLICKED) return;
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (btn == reset_all_btn) {
    for (auto &f : fields) {
      double d = config_default(f);
      if (!std::isnan(d)) send_field(f, d);
    }
    return;
  }

  for (auto &f : fields) {
    if (btn == f.minus) { apply_step(f, false); return; }
    if (btn == f.plus)  { apply_step(f, true);  return; }
  }
}
