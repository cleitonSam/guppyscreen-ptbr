#include "fan_panel.h"
#include "state.h"
#include "utils.h"
#include "marca.h"
#include "navegacao.h"
#include "numpad.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>

LV_IMG_DECLARE(fan_on);

namespace {
  constexpr int PASSO = 10;                 // % por toque em - / +
  constexpr lv_coord_t CARTAO_L = 408, CARTAO_A = 76;
  constexpr lv_coord_t LINHA_A = 36;        // linha só de leitura, dentro do cartão
  constexpr lv_coord_t TITULO_A = 26;       // título do cartão das automáticas
  constexpr lv_coord_t TOPO = 7, BASE = 8;  // respiro em cima e embaixo da lista

  int para_pct(double v) {
    int p = static_cast<int>(std::lround(v * 100.0));
    return std::clamp(p, 0, 100);
  }
}

FanPanel::FanPanel(KWebSocketClient &websocket_client, std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  // criado direto na aba: o tema deixa transparente (não desenha nada)
  , fanpanel_cont(lv_obj_create(Navegacao::aba(Aba::VENTOINHAS)))
  , fans_cont(lv_obj_create(fanpanel_cont))
{
  Navegacao::colocar(fanpanel_cont, Aba::VENTOINHAS);
  lv_obj_set_style_pad_all(fanpanel_cont, 0, 0);
  lv_obj_set_style_bg_opa(fanpanel_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(fanpanel_cont, 0, 0);
  lv_obj_clear_flag(fanpanel_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(fanpanel_cont, LV_OBJ_FLAG_CLICKABLE);

  // lista: cartões 408 de largura em x = 8, 8 px entre eles
  lv_obj_set_pos(fans_cont, 0, 0);
  lv_obj_set_size(fans_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(fans_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(fans_cont, 0, 0);
  lv_obj_set_style_pad_top(fans_cont, TOPO, 0);
  lv_obj_set_style_pad_bottom(fans_cont, BASE, 0);
  lv_obj_set_style_pad_left(fans_cont, Marca::MARGEM, 0);
  lv_obj_set_style_pad_right(fans_cont, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(fans_cont, Marca::VAO, 0);
  lv_obj_set_flex_flow(fans_cont, LV_FLEX_FLOW_COLUMN);
  // rolagem só se não couber, sem inércia e sem elástico
  lv_obj_clear_flag(fans_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(fans_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scroll_dir(fans_cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(fans_cont, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_clear_flag(fans_cont, LV_OBJ_FLAG_SCROLLABLE);

  // O que o foreground() antigo fazia, agora ao mostrar a aba.
  Navegacao::ao_mostrar(Aba::VENTOINHAS, [this]() { atualizar(); });

  ws.register_notify_update(this);
}

FanPanel::~FanPanel() {
  if (fanpanel_cont != NULL) {
    lv_obj_del(fanpanel_cont);
    fanpanel_cont = NULL;
  }

  fans.clear();

  ws.unregister_notify_update(this);
}

void FanPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  for (auto &f : fans) {
    bool mudou = false;
    int pct = f.pct;

    // hack for output_pin fans
    auto fan_value = j[json::json_pointer(fmt::format("/params/0/{}/value", f.id))];
    if (fan_value.is_number()) {
      pct = para_pct(fan_value.template get<double>());
      mudou = true;
    }

    fan_value = j[json::json_pointer(fmt::format("/params/0/{}/speed", f.id))];
    if (fan_value.is_number()) {
      pct = para_pct(fan_value.template get<double>());
      mudou = true;
    }

    auto rpm = j[json::json_pointer(fmt::format("/params/0/{}/rpm", f.id))];
    if (!rpm.is_null()) {
      mudou = true;
    }

    if (mudou) {
      mostrar(f, pct);
    }
  }
  update_readonly();
}

void FanPanel::create_fans(json &f) {
  std::lock_guard<std::mutex> lock(lv_lock);
  // reconexão: apaga os cartões e as linhas de antes (só aqui, nunca no consume)
  lv_obj_clean(fans_cont);
  fans.clear();
  ro_labels.clear();
  ro_cartao = nullptr;

  for (auto &fan : f.items()) {
    std::string key = fan.key();
    spdlog::trace("create fan {}, {}", f.dump(), fan.value().dump());
    std::string display_name = fan.value()["display_name"].template get<std::string>();
    // pt-BR: nomes padrão antigos (em inglês) gravados no guppyconfig.json.
    // Traduz só o que aparece; o arquivo fica igual.
    if (display_name == "Toolhead Fan" || display_name == "Part Cooling Fan") display_name = "Ventoinha da peça";
    else if (display_name == "Back Fan") display_name = "Ventoinha traseira";
    else if (display_name == "Side Fan") display_name = "Ventoinha lateral";

    Ventoinha v;
    v.id = key;
    v.pct = -1;

    // cartão 408 x 76 (posição pela lista)
    v.cartao = lv_obj_create(fans_cont);
    Marca::cartao(v.cartao);
    lv_obj_set_size(v.cartao, CARTAO_L, CARTAO_A);
    lv_obj_clear_flag(v.cartao, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icone = Marca::criar_icone(v.cartao, &fan_on, 24, Marca::GRAFITE);
    lv_obj_set_pos(icone, 12, 12);

    lv_obj_t *nome = Marca::criar_rotulo(v.cartao, display_name.c_str(), Marca::f14(), Marca::PAPEL);
    lv_obj_set_pos(nome, 42, 15);
    lv_obj_set_width(nome, 140);
    lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);

    v.estado = Marca::criar_rotulo(v.cartao, "", Marca::f12(), Marca::GRAFITE);
    lv_obj_set_pos(v.estado, 12, 49);
    lv_obj_set_width(v.estado, 170);
    lv_label_set_long_mode(v.estado, LV_LABEL_LONG_DOT);

    // "-" 52 x 44, valor 92 x 44, "+" 52 x 44 (centrados na altura do cartão)
    v.menos = Marca::criar_botao(v.cartao, LV_SYMBOL_MINUS, Marca::Tipo::NORMAL, Marca::f20());
    lv_obj_set_pos(v.menos, 188, 16);
    lv_obj_set_size(v.menos, 52, Marca::TOQUE);
    lv_obj_add_event_cb(v.menos, &FanPanel::_handle_callback, LV_EVENT_CLICKED, this);

    v.valor_cont = lv_obj_create(v.cartao);
    lv_obj_set_pos(v.valor_cont, 244, 16);
    lv_obj_set_size(v.valor_cont, 92, Marca::TOQUE);
    lv_obj_set_style_bg_color(v.valor_cont, lv_color_hex(Marca::PRETO), 0);
    lv_obj_set_style_bg_opa(v.valor_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(v.valor_cont, lv_color_hex(Marca::APERTADO), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(v.valor_cont, 1, 0);
    lv_obj_set_style_border_color(v.valor_cont, lv_color_hex(Marca::PRETO_3), 0);
    lv_obj_set_style_radius(v.valor_cont, Marca::RAIO, 0);
    lv_obj_set_style_pad_all(v.valor_cont, 0, 0);
    lv_obj_clear_flag(v.valor_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(v.valor_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v.valor_cont, &FanPanel::_handle_callback, LV_EVENT_CLICKED, this);

    v.valor = Marca::criar_rotulo(v.valor_cont, "--", Marca::f20(), Marca::PAPEL);
    lv_obj_center(v.valor);

    v.mais = Marca::criar_botao(v.cartao, LV_SYMBOL_PLUS, Marca::Tipo::NORMAL, Marca::f20());
    lv_obj_set_pos(v.mais, 340, 16);
    lv_obj_set_size(v.mais, 52, Marca::TOQUE);
    lv_obj_add_event_cb(v.mais, &FanPanel::_handle_callback, LV_EVENT_CLICKED, this);

    fans.push_back(v);
  }

  for (auto &v : fans) {
    ler_estado(v);
  }

  create_readonly_fans();
  ajustar_rolagem();
}

// Read-only fans: heater_fan / controller_fan (auto-managed by Klipper) and any
// fan-named output_pin that isn't already an editable card. Shown as a simple
// name + value row (12 grafite, no controls).
void FanPanel::create_readonly_fans() {
  State *s = State::get_instance();
  std::vector<std::string> candidates = s->get_fans();
  for (auto &op : s->get_output_pins()) {
    std::string lower = op;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.find("fan") != std::string::npos) {
      candidates.push_back(op);
    }
  }

  for (auto &key : candidates) {
    bool editavel = std::any_of(fans.begin(), fans.end(),
      [&key](const Ventoinha &v) { return v.id == key; });
    if (editavel || ro_labels.count(key)) {
      continue;  // already shown as an editable card, or already added
    }
    // design 11/11: as automáticas ficam num cartão (como as listas de
    // Ajustes), com título, em vez de linhas soltas no fundo
    if (ro_cartao == nullptr) {
      ro_cartao = Marca::criar_cartao(fans_cont, 0, 0, CARTAO_L, TITULO_A + 8);
      lv_obj_set_style_pad_all(ro_cartao, 0, 0);
      lv_obj_set_style_pad_bottom(ro_cartao, 4, 0);
      lv_obj_clear_flag(ro_cartao, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(ro_cartao, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_flex_flow(ro_cartao, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_height(ro_cartao, LV_SIZE_CONTENT);
      lv_obj_t *tit = Marca::criar_rotulo(ro_cartao, "Automáticas (a impressora controla sozinha)",
        Marca::f12(), Marca::GRAFITE);
      lv_obj_set_width(tit, CARTAO_L - 24);
      lv_obj_set_height(tit, TITULO_A);
      lv_obj_set_style_pad_left(tit, 12, 0);
      lv_obj_set_style_pad_top(tit, 10, 0);
    }
    lv_obj_t *row = lv_obj_create(ro_cartao);
    lv_obj_set_size(row, CARTAO_L, LINHA_A);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(Marca::PRETO_3), 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *name = Marca::criar_rotulo(row, KUtils::fan_display_name(key).c_str(),
      Marca::f14(), Marca::PAPEL);
    lv_obj_set_width(name, 250);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t *val = Marca::criar_rotulo(row, "-", Marca::f14(), Marca::GRAFITE);
    lv_obj_set_width(val, 130);
    lv_label_set_long_mode(val, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -12, 0);
    ro_labels[key] = val;
  }

  update_readonly();
}

void FanPanel::ajustar_rolagem() {
  // Conta fixa (cartões 76, linhas 20, 8 entre eles): rola só se não couber.
  size_t n = fans.size() + (ro_labels.empty() ? 0 : 1);
  lv_coord_t altura = TOPO + BASE
    + (lv_coord_t)fans.size() * CARTAO_A
    + (ro_labels.empty() ? 0 : (lv_coord_t)(TITULO_A + 4 + ro_labels.size() * LINHA_A))
    + (n > 0 ? (lv_coord_t)(n - 1) * Marca::VAO : 0);

  if (altura > Marca::ABA_CONT_A) {
    lv_obj_add_flag(fans_cont, LV_OBJ_FLAG_SCROLLABLE);
  } else {
    lv_obj_scroll_to_y(fans_cont, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(fans_cont, LV_OBJ_FLAG_SCROLLABLE);
  }
}

bool FanPanel::is_binary_fan(const std::string &key) {
  if (key.rfind("heater_fan ", 0) == 0 || key.rfind("controller_fan ", 0) == 0) {
    return true;  // auto-managed, effectively on/off
  }
  if (key.rfind("output_pin ", 0) == 0) {
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto pwm = State::get_instance()->get_data(
      json::json_pointer("/printer_state/configfile/settings/" + lower + "/pwm"));
    // output_pin defaults to non-PWM (binary) unless pwm: True is configured
    return pwm.is_boolean() ? !pwm.template get<bool>() : true;
  }
  return false;  // fan_generic etc. -> show a percentage
}

std::string FanPanel::fmt_ro_value(const std::string &key) {
  State *s = State::get_instance();
  double v = 0.0;
  bool have = false;
  auto sp = s->get_data(json::json_pointer("/printer_state/" + key + "/speed"));
  if (sp.is_number()) {
    v = sp.template get<double>();
    have = true;
  } else {
    auto val = s->get_data(json::json_pointer("/printer_state/" + key + "/value"));
    if (val.is_number()) {
      v = val.template get<double>();
      have = true;
    }
  }
  if (!have) {
    return "-";
  }
  if (is_binary_fan(key)) {
    return v > 0.0 ? "Ligada" : "Desligada";
  }
  std::string out = fmt::format("{}%", (int)std::lround(v * 100.0));
  auto rpm = s->get_data(json::json_pointer("/printer_state/" + key + "/rpm"));
  if (rpm.is_number()) {
    out += fmt::format("  {} RPM", (int)std::lround(rpm.template get<double>()));
  }
  return out;
}

void FanPanel::update_readonly() {
  for (auto &kv : ro_labels) {
    Marca::texto(kv.second, fmt_ro_value(kv.first).c_str());
  }
}

// Lê o valor de agora no State (o mesmo que o foreground() antigo lia).
void FanPanel::ler_estado(Ventoinha &v) {
  int pct = v.pct < 0 ? 0 : v.pct;

  // hack for output_pin fans
  auto fan_value = State::get_instance()
    ->get_data(json::json_pointer(fmt::format("/printer_state/{}/value", v.id)));
  if (fan_value.is_number()) {
    pct = para_pct(fan_value.template get<double>());
  }

  fan_value = State::get_instance()
    ->get_data(json::json_pointer(fmt::format("/printer_state/{}/speed", v.id)));
  if (fan_value.is_number()) {
    pct = para_pct(fan_value.template get<double>());
  }

  mostrar(v, pct);
}

void FanPanel::mostrar(Ventoinha &v, int pct) {
  v.pct = std::clamp(pct, 0, 100);
  Marca::texto(v.valor, fmt::format("{}%", v.pct).c_str());

  std::string estado = v.pct > 0 ? "Ligada" : "Desligada";
  auto rpm = State::get_instance()
    ->get_data(json::json_pointer(fmt::format("/printer_state/{}/rpm", v.id)));
  if (rpm.is_number()) {
    estado += fmt::format(" · {} RPM", (int)std::lround(rpm.template get<double>()));
  }
  Marca::texto(v.estado, estado.c_str());
}

void FanPanel::atualizar() {
  for (auto &v : fans) {
    ler_estado(v);
  }
  update_readonly();
}

lv_obj_t *FanPanel::get_container() {
  return fanpanel_cont;
}

void FanPanel::foreground() {
  // o gancho ao_mostrar(Aba::VENTOINHAS) faz o resto (atualizar)
  Navegacao::ir_para(Aba::VENTOINHAS);
}

// Mesmos comandos de antes, por tipo de ventoinha:
//  "fan"          -> M106 S{0..255}                (handle_fan_update_part_fan)
//  "output_pin X" -> SET_PIN PIN=X VALUE={0..255}  (handle_fan_update)
//  outras         -> SET_FAN_SPEED FAN=X SPEED={0..1} (handle_fan_update_generic)
// 0% e 100% mandam exatamente o texto dos antigos "Desligar" e "Máximo";
// o meio usa a mesma conta do controle deslizante antigo.
void FanPanel::enviar(const std::string &id, int pct) {
  if (id == "fan") {
    if (pct <= 0) {
      ws.gcode_script("M106 S0");
    } else if (pct >= 100) {
      ws.gcode_script("M106 S255");
    } else {
      double v = 255 * (double)pct / 100.0;
      spdlog::trace("updating part fan speed to {}", v);
      ws.gcode_script(fmt::format("M106 S{}", v));
    }
  } else if (id.rfind("output_pin ", 0) == 0) {
    std::string fan_name = KUtils::get_obj_name(id);
    if (pct <= 0) {
      ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE=0", fan_name));
    } else if (pct >= 100) {
      ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE=255", fan_name));
    } else {
      double v = 255 * (double)pct / 100.0;
      spdlog::trace("update fan {} to {}", fan_name, v);
      ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE={}", fan_name, v));
    }
  } else {
    std::string fan_name = KUtils::get_obj_name(id);
    if (pct <= 0) {
      ws.gcode_script(fmt::format("SET_FAN_SPEED FAN={} SPEED=0", fan_name));
    } else if (pct >= 100) {
      ws.gcode_script(fmt::format("SET_FAN_SPEED FAN={} SPEED=1", fan_name));
    } else {
      double v = (double)pct / 100.0;
      spdlog::trace("update fan {} to {}", fan_name, v);
      ws.gcode_script(fmt::format("SET_FAN_SPEED FAN={} SPEED={}", fan_name, v));
    }
  }
}

void FanPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *obj = lv_event_get_current_target(event);

  for (auto &f : fans) {
    if (obj == f.menos || obj == f.mais) {
      int atual = f.pct < 0 ? 0 : f.pct;
      int novo = obj == f.menos ? std::max(0, atual - PASSO) : std::min(100, atual + PASSO);
      if (novo != atual) {
        enviar(f.id, novo);
        mostrar(f, novo);
      }
      return;
    }

    if (obj == f.valor_cont) {
      Numpad *np = Numpad::compartilhado();
      if (np == NULL) {
        return;
      }
      // procura pelo id na volta (a lista pode ter sido refeita numa reconexão)
      std::string id = f.id;
      np->abrir(f.pct < 0 ? 0 : f.pct, [this, id](double d) {
        int novo = std::clamp(static_cast<int>(std::lround(d)), 0, 100);
        for (auto &g : fans) {
          if (g.id == id) {
            enviar(id, novo);
            mostrar(g, novo);
            break;
          }
        }
      });
      return;
    }
  }

  spdlog::debug("Unknown action button pressed");
}
