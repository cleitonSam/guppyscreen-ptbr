#include "sensor_container.h"
#include "marca.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <cstdlib>
#include <string>

namespace {
  constexpr int PASSO = 5;            // °C por toque em − / +
  constexpr uint32_t ADIAR_MS = 700;  // espera depois do último toque

  enum { EST_DESLIGADO = 0, EST_NO_ALVO, EST_AQUECENDO, EST_ESFRIANDO };
}

SensorContainer::SensorContainer(KWebSocketClient &c,
  lv_obj_t *parent,
  const void *img,
  const char *text,
  lv_color_t color,
  bool can_edit,
  bool show_target,
  Numpad &np,
  std::string name,
  lv_obj_t *chart_chart,
  lv_chart_series_t *chart_series,
  SensorType type)
  : ws(c)
  , sensor_cont(lv_obj_create(parent))
  , sensor_img(NULL)
  , sensor_label(NULL)
  , value_label(NULL)
  , value(0)
  , estado_ponto(NULL)
  , estado_label(NULL)
  , alvo_titulo(NULL)
  , menos_btn(NULL)
  , alvo_cont(NULL)
  , target_label(NULL)
  , mais_btn(NULL)
  , target(-1)
  , pendente(-1)
  , estado_atual(-1)
  , timer_envio(NULL)
  , numpad(np)
  , id(name)
  , chart(chart_chart)
  , series(chart_series)
  , target_series(NULL)
  , last_updated_ts(std::time(nullptr))
  , type(type)
{
  LV_UNUSED(color);  // a cor por sensor saiu: o cartão segue a marca

  // cartão 408 x 76 (quem chama posiciona)
  Marca::cartao(sensor_cont);
  lv_obj_set_size(sensor_cont, 408, 76);
  lv_obj_clear_flag(sensor_cont, LV_OBJ_FLAG_CLICKABLE);

  sensor_img = Marca::criar_icone(sensor_cont, img, 24, Marca::GRAFITE);
  lv_obj_set_pos(sensor_img, 12, 12);

  sensor_label = Marca::criar_rotulo(sensor_cont, text, Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(sensor_label, 42, 15);
  lv_obj_set_width(sensor_label, 140);
  lv_label_set_long_mode(sensor_label, LV_LABEL_LONG_DOT);

  value_label = Marca::criar_rotulo(sensor_cont, "0°C", Marca::f20(), Marca::PAPEL);
  lv_obj_set_pos(value_label, 12, 44);
  lv_obj_set_width(value_label, 80);
  lv_label_set_long_mode(value_label, LV_LABEL_LONG_CLIP);

  // estado com palavra (e bolinha quando faz sentido)
  estado_ponto = lv_obj_create(sensor_cont);
  lv_obj_set_pos(estado_ponto, 92, 53);
  lv_obj_set_size(estado_ponto, 8, 8);
  lv_obj_set_style_radius(estado_ponto, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(estado_ponto, lv_color_hex(Marca::LIMAO), 0);
  lv_obj_set_style_bg_opa(estado_ponto, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(estado_ponto, 0, 0);
  lv_obj_set_style_pad_all(estado_ponto, 0, 0);
  lv_obj_clear_flag(estado_ponto, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(estado_ponto, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(estado_ponto, LV_OBJ_FLAG_HIDDEN);

  estado_label = Marca::criar_rotulo(sensor_cont, "", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(estado_label, 104, 49);
  lv_obj_set_width(estado_label, 80);
  lv_label_set_long_mode(estado_label, LV_LABEL_LONG_CLIP);

  // alvo: "Alvo" + − / valor / +
  alvo_titulo = Marca::criar_rotulo(sensor_cont, "Alvo", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(alvo_titulo, 244, 4);
  lv_obj_set_width(alvo_titulo, 92);
  lv_obj_set_style_text_align(alvo_titulo, LV_TEXT_ALIGN_CENTER, 0);

  menos_btn = Marca::criar_botao(sensor_cont, LV_SYMBOL_MINUS, Marca::Tipo::NORMAL, Marca::f20());
  lv_obj_set_pos(menos_btn, 188, 24);
  lv_obj_set_size(menos_btn, 52, 44);

  alvo_cont = lv_obj_create(sensor_cont);
  lv_obj_set_pos(alvo_cont, 244, 24);
  lv_obj_set_size(alvo_cont, 92, 44);
  lv_obj_set_style_bg_color(alvo_cont, lv_color_hex(Marca::PRETO), 0);
  lv_obj_set_style_bg_opa(alvo_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(alvo_cont, 1, 0);
  lv_obj_set_style_border_color(alvo_cont, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_set_style_radius(alvo_cont, Marca::RAIO, 0);
  lv_obj_set_style_pad_all(alvo_cont, 0, 0);
  lv_obj_clear_flag(alvo_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(alvo_cont, LV_OBJ_FLAG_CLICKABLE);

  target_label = Marca::criar_rotulo(alvo_cont, "0°C", Marca::f20(), Marca::PAPEL);
  lv_obj_center(target_label);

  mais_btn = Marca::criar_botao(sensor_cont, LV_SYMBOL_PLUS, Marca::Tipo::NORMAL, Marca::f20());
  lv_obj_set_pos(mais_btn, 340, 24);
  lv_obj_set_size(mais_btn, 52, 44);

  if (!(show_target || can_edit)) {
    // sensor só de leitura: sem alvo nem estado
    lv_obj_add_flag(alvo_titulo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(alvo_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(estado_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (can_edit) {
    lv_obj_add_flag(alvo_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(alvo_cont, lv_color_hex(Marca::APERTADO), LV_STATE_PRESSED);

    spdlog::debug("sensor cb registered name {}, cont {}, this {}, np {}",
      id, fmt::ptr(sensor_cont), fmt::ptr(this), fmt::ptr(&np));
    lv_obj_add_event_cb(alvo_cont, &SensorContainer::_handle_edit, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(menos_btn, &SensorContainer::_menos_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(mais_btn, &SensorContainer::_mais_cb, LV_EVENT_CLICKED, this);

    // um timer só, criado uma vez e pausado; cada toque o reinicia
    timer_envio = lv_timer_create(&SensorContainer::_timer_cb, ADIAR_MS, this);
    lv_timer_pause(timer_envio);
  } else {
    lv_obj_add_flag(menos_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mais_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

SensorContainer::SensorContainer(KWebSocketClient &c,
  lv_obj_t *parent,
  const void *img,
  uint16_t img_scale,
  const char *text,
  lv_color_t color,
  bool can_edit,
  bool show_target,
  Numpad &np,
  std::string name,
  lv_obj_t *chart,
  lv_chart_series_t *chart_series,
  SensorType type)
  : SensorContainer(c, parent, img, text, color, can_edit, show_target, np, name, chart, chart_series, type)
{
  LV_UNUSED(img_scale);  // ícone fixo de 24 px no cartão novo
}

SensorContainer::~SensorContainer() {
  if (timer_envio != NULL) {
    lv_timer_del(timer_envio);
    timer_envio = NULL;
  }

  if (sensor_cont != NULL) {
    spdlog::debug("deleting sensor {}", id);
    lv_obj_del(sensor_cont);
    sensor_cont = NULL;
  }

  if (series != NULL && chart != NULL) {
    lv_chart_remove_series(chart, series);
    series = NULL;
  }

  if (target_series != NULL && chart != NULL) {
    lv_chart_remove_series(chart, target_series);
    target_series = NULL;
  }
}

lv_obj_t *SensorContainer::get_sensor() {
  return sensor_cont;
}

void SensorContainer::mostrar_alvo(int v) {
  Marca::texto(target_label, fmt::format("{}°C", v).c_str());
}

void SensorContainer::update_target(int new_target) {
  if (new_target >= 0) {
    target = new_target;
    if (pendente < 0) {
      mostrar_alvo(new_target);
    }
    atualizar_estado();
    if (target_series != NULL && chart != NULL) {
      lv_chart_set_all_value(chart, target_series, new_target);
    }
  }
}

void SensorContainer::set_target_series(lv_chart_series_t *ts) {
  target_series = ts;
  if (ts != NULL && chart != NULL && target >= 0) {
    lv_chart_set_all_value(chart, ts, target);
  }
}

void SensorContainer::update_value(int new_value) {
  if (value != new_value) {
    value = new_value;
    Marca::texto(value_label, fmt::format("{}°C", new_value).c_str());
    atualizar_estado();
  }
}

void SensorContainer::atualizar_estado() {
  if (lv_obj_has_flag(estado_label, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }

  int novo;
  if (target <= 0) {
    novo = EST_DESLIGADO;
  } else if (std::abs(value - target) <= 2) {
    novo = EST_NO_ALVO;
  } else if (value < target) {
    novo = EST_AQUECENDO;
  } else {
    novo = EST_ESFRIANDO;
  }

  if (novo == estado_atual) {
    return;
  }
  estado_atual = novo;

  switch (novo) {
  case EST_NO_ALVO:
    Marca::texto(estado_label, "No alvo");
    lv_obj_set_style_bg_color(estado_ponto, lv_color_hex(Marca::OK_PONTO), 0);
    lv_obj_clear_flag(estado_ponto, LV_OBJ_FLAG_HIDDEN);
    break;
  case EST_AQUECENDO:
    Marca::texto(estado_label, "Aquecendo...");
    lv_obj_set_style_bg_color(estado_ponto, lv_color_hex(Marca::LIMAO), 0);
    lv_obj_clear_flag(estado_ponto, LV_OBJ_FLAG_HIDDEN);
    break;
  case EST_ESFRIANDO:
    Marca::texto(estado_label, "Esfriando...");
    lv_obj_add_flag(estado_ponto, LV_OBJ_FLAG_HIDDEN);
    break;
  default:
    Marca::texto(estado_label, "Desligado");
    lv_obj_add_flag(estado_ponto, LV_OBJ_FLAG_HIDDEN);
    break;
  }
}

void SensorContainer::update_series(int v) {
  if (series != NULL && chart != NULL) {
#ifdef SIMULATOR
    /* No throttling in simulator mode - driven entirely by the mock timer. */
    lv_chart_set_next_value(chart, series, v);
#else
    auto delta = std::time(nullptr) - last_updated_ts;
    if (delta > 1) {
      lv_chart_set_next_value(chart, series, v);
      last_updated_ts = std::time(nullptr);
    }
#endif
  }
}

int SensorContainer::alvo_maximo() {
  auto v = State::get_instance()->get_data(
    json::json_pointer(fmt::format("/printer_state/configfile/settings/{}/max_temp", id)));
  if (v.is_number()) {
    int m = (int)v.template get<double>();
    if (m > 0) {
      return m;
    }
  }
  return id.rfind("extruder", 0) == 0 ? 300 : 110;
}

void SensorContainer::ajustar(int delta) {
  int base = pendente >= 0 ? pendente : (target >= 0 ? target : 0);
  int novo = base + delta;
  int maximo = alvo_maximo();
  if (novo > maximo) novo = maximo;
  if (novo < 0) novo = 0;

  pendente = novo;
  mostrar_alvo(novo);
  if (timer_envio != NULL) {
    lv_timer_reset(timer_envio);
    lv_timer_resume(timer_envio);
  }
}

std::string SensorContainer::comando_alvo(int v) const {
  switch (type) {
  case SensorType::Heater:
    return fmt::format("SET_HEATER_TEMPERATURE HEATER={} TARGET={}", id, v);
  case SensorType::TempFan:
    return fmt::format("SET_TEMPERATURE_FAN_TARGET TEMPERATURE_FAN={} TARGET={}", id, v);
  default:
    return "";
  }
}

void SensorContainer::enviar_alvo(int v) {
  std::string cmd = comando_alvo(v);
  if (!cmd.empty()) {
    ws.gcode_script(cmd);
  }
}

void SensorContainer::_menos_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ((SensorContainer *)lv_event_get_user_data(e))->ajustar(-PASSO);
  }
}

void SensorContainer::_mais_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ((SensorContainer *)lv_event_get_user_data(e))->ajustar(PASSO);
  }
}

// roda dentro do lv_timer_handler (lv_lock já preso)
void SensorContainer::_timer_cb(lv_timer_t *t) {
  SensorContainer *s = (SensorContainer *)t->user_data;
  lv_timer_pause(t);
  if (s->pendente >= 0) {
    int v = s->pendente;
    s->pendente = -1;
    if (!KUtils::is_printing()) {
      s->enviar_alvo(v);
      // o Moonraker só manda o alvo quando ele muda; mostra já o novo
      s->update_target(v);
      return;
    }
    // Imprimindo: um toque sem querer no − / + não pode mudar o bico ou a
    // mesa. Volta o número para o alvo atual e pergunta antes. Se confirmar,
    // manda o mesmo comando de sempre; o novo alvo aparece quando o
    // Moonraker avisar. A pergunta guarda só o comando e o ws (não o cartão,
    // que pode ser refeito ao reconectar).
    s->mostrar_alvo(s->target >= 0 ? s->target : 0);
    std::string cmd = s->comando_alvo(v);
    if (cmd.empty()) {
      return;
    }
    KWebSocketClient *ws = &s->ws;
    KUtils::confirm_if_printing("A impressora está imprimindo.\nMudar a temperatura mesmo assim?",
      [ws, cmd]() { ws->gcode_script(cmd); });
  }
}

void SensorContainer::handle_edit(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    spdlog::trace("sensor callback this {}, {}, {}", id, fmt::ptr(this), fmt::ptr(&numpad));
    numpad.set_callback([this](double v) {
      // o número digitado vale mais que um − / + ainda não enviado
      pendente = -1;
      if (timer_envio != NULL) {
        lv_timer_pause(timer_envio);
      }
      std::string cmd;
      switch (type) {
      case SensorType::Heater:
        cmd = fmt::format("SET_HEATER_TEMPERATURE HEATER={} TARGET={}", id, v);
        break;
      case SensorType::TempFan:
        cmd = fmt::format("SET_TEMPERATURE_FAN_TARGET TEMPERATURE_FAN={} TARGET={}", id, v);
        break;
      default:
        return;
      }
      ws.gcode_script(cmd);
      });
    /* Pre-fill the numpad with the current target so the user can adjust
     * from the existing value instead of retyping from scratch. */
    numpad.foreground_reset(pendente >= 0 ? pendente : target);
  }
}
