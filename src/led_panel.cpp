#include "led_panel.h"
#include "state.h"
#include "utils.h"
#include "marca.h"
#include "navegacao.h"
#include "numpad.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>

LV_IMG_DECLARE(light_img);

namespace {
  constexpr int PASSO = 10;                 // % por toque em - / +
  constexpr lv_coord_t CARTAO_L = 408, CARTAO_A = 76;
  constexpr lv_coord_t LISTA_Y = Marca::ABA_A;            // logo abaixo do cabeçalho
  constexpr lv_coord_t LISTA_A = Marca::TELA_A - LISTA_Y; // 228
  constexpr lv_coord_t TOPO = 8, BASE = 8;  // primeiro cartão em y = 52

  int para_pct(double v) {
    int p = static_cast<int>(v * 100);    // mesma conta de antes (trunca)
    return std::clamp(p, 0, 100);
  }

  lv_obj_t *criar_caixa_valor(lv_obj_t *pai) {
    lv_obj_t *cx = lv_obj_create(pai);
    lv_obj_set_size(cx, 92, Marca::TOQUE);
    lv_obj_set_style_bg_color(cx, lv_color_hex(Marca::PRETO), 0);
    lv_obj_set_style_bg_opa(cx, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cx, lv_color_hex(Marca::APERTADO), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(cx, 1, 0);
    lv_obj_set_style_border_color(cx, lv_color_hex(Marca::PRETO_3), 0);
    lv_obj_set_style_radius(cx, Marca::RAIO, 0);
    lv_obj_set_style_pad_all(cx, 0, 0);
    lv_obj_clear_flag(cx, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cx, LV_OBJ_FLAG_CLICKABLE);
    return cx;
  }
}

LedPanel::LedPanel(KWebSocketClient &websocket_client, std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , ledpanel_cont(lv_obj_create(lv_scr_act()))
  , leds_cont(NULL)
{
  // subtela: (56,0) 424 x 272, PRETO, pad 0, registrada e atrás
  Navegacao::subtela(ledpanel_cont);
  Marca::criar_cabecalho(ledpanel_cont, "Luz");

  leds_cont = lv_obj_create(ledpanel_cont);
  lv_obj_set_pos(leds_cont, 0, LISTA_Y);
  lv_obj_set_size(leds_cont, Marca::CONT_L, LISTA_A);
  lv_obj_set_style_bg_opa(leds_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(leds_cont, 0, 0);
  lv_obj_set_style_pad_top(leds_cont, TOPO, 0);
  lv_obj_set_style_pad_bottom(leds_cont, BASE, 0);
  lv_obj_set_style_pad_left(leds_cont, Marca::MARGEM, 0);
  lv_obj_set_style_pad_right(leds_cont, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(leds_cont, Marca::VAO, 0);
  lv_obj_set_flex_flow(leds_cont, LV_FLEX_FLOW_COLUMN);
  // rolagem só se não couber, sem inércia e sem elástico
  lv_obj_clear_flag(leds_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(leds_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scroll_dir(leds_cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(leds_cont, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_clear_flag(leds_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(leds_cont, LV_OBJ_FLAG_CLICKABLE);

  // Início (cartão Luz) chama executar("luz"): roda com o lv_lock preso.
  Navegacao::registrar_acao("luz", [this](const std::string &) { foreground(); });

  ws.register_notify_update(this);
}

LedPanel::~LedPanel() {
  if (ledpanel_cont != NULL) {
    lv_obj_del(ledpanel_cont);
    ledpanel_cont = NULL;
  }

  leds.clear();

  ws.unregister_notify_update(this);
}

void LedPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  for (auto &l : leds) {
    // hack for output_pin leds
    auto value = j[json::json_pointer(fmt::format("/params/0/{}/value", l.id))];
    if (value.is_number()) {
      mostrar(l, para_pct(value.template get<double>()));
    }

    value = j[json::json_pointer(fmt::format("/params/0/{}/color_data", l.id))];
    if (!value.is_null() && value.size() > 0) {
      // color_data = [[r,b,g,w]]
      value = value.at(0);
      if (value.size() == 4 && value.at(3).is_number()) {
        mostrar(l, para_pct(value.at(3).template get<double>()));
      }
    }
  }
}

void LedPanel::init(json &l) {
  std::lock_guard<std::mutex> lock(lv_lock);
  // reconexão: apaga os cartões de antes (só aqui, nunca no consume)
  lv_obj_clean(leds_cont);
  leds.clear();

  for (auto &led : l.items()) {
    std::string key = led.key();
    spdlog::trace("create led {}, {}", l.dump(), led.value().dump());
    std::string display_name = led.value()["display_name"].template get<std::string>();

    Luz z;
    z.id = key;
    z.pino = key.rfind("output_pin ", 0) == 0;
    z.binaria = false;
    if (z.pino) {
      std::string lower = key;
      std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
      auto pwm = State::get_instance()->get_data(
        json::json_pointer("/printer_state/configfile/settings/" + lower + "/pwm"));
      z.binaria = pwm.is_boolean() ? !pwm.template get<bool>() : true;
    }
    z.menos = z.valor_cont = z.valor = z.mais = NULL;
    z.desligar = z.ligar = NULL;
    z.pct = -1;

    z.cartao = lv_obj_create(leds_cont);
    Marca::cartao(z.cartao);
    lv_obj_set_size(z.cartao, CARTAO_L, CARTAO_A);
    lv_obj_clear_flag(z.cartao, LV_OBJ_FLAG_CLICKABLE);

    z.icone = Marca::criar_icone(z.cartao, &light_img, 24, Marca::GRAFITE);
    lv_obj_set_pos(z.icone, 12, 12);

    lv_obj_t *nome = Marca::criar_rotulo(z.cartao, display_name.c_str(), Marca::f14(), Marca::PAPEL);
    lv_obj_set_pos(nome, 42, 15);
    lv_obj_set_width(nome, 140);
    lv_label_set_long_mode(nome, LV_LABEL_LONG_DOT);

    z.estado = Marca::criar_rotulo(z.cartao, "", Marca::f12(), Marca::GRAFITE);
    lv_obj_set_pos(z.estado, 12, 49);
    lv_obj_set_width(z.estado, 170);
    lv_label_set_long_mode(z.estado, LV_LABEL_LONG_DOT);

    if (z.binaria) {
      // os mesmos dois botões de antes ("Desligar" e "Ligar")
      z.desligar = Marca::criar_botao(z.cartao, "Desligar", Marca::Tipo::NORMAL);
      lv_obj_set_pos(z.desligar, 188, 16);
      lv_obj_set_size(z.desligar, 100, Marca::TOQUE);
      lv_obj_add_event_cb(z.desligar, &LedPanel::_handle_callback, LV_EVENT_CLICKED, this);

      z.ligar = Marca::criar_botao(z.cartao, "Ligar", Marca::Tipo::NORMAL);
      lv_obj_set_pos(z.ligar, 292, 16);
      lv_obj_set_size(z.ligar, 100, Marca::TOQUE);
      lv_obj_add_event_cb(z.ligar, &LedPanel::_handle_callback, LV_EVENT_CLICKED, this);
    } else {
      z.menos = Marca::criar_botao(z.cartao, LV_SYMBOL_MINUS, Marca::Tipo::NORMAL, Marca::f20());
      lv_obj_set_pos(z.menos, 188, 16);
      lv_obj_set_size(z.menos, 52, Marca::TOQUE);
      lv_obj_add_event_cb(z.menos, &LedPanel::_handle_callback, LV_EVENT_CLICKED, this);

      z.valor_cont = criar_caixa_valor(z.cartao);
      lv_obj_set_pos(z.valor_cont, 244, 16);
      lv_obj_add_event_cb(z.valor_cont, &LedPanel::_handle_callback, LV_EVENT_CLICKED, this);

      z.valor = Marca::criar_rotulo(z.valor_cont, "--", Marca::f20(), Marca::PAPEL);
      lv_obj_center(z.valor);

      z.mais = Marca::criar_botao(z.cartao, LV_SYMBOL_PLUS, Marca::Tipo::NORMAL, Marca::f20());
      lv_obj_set_pos(z.mais, 340, 16);
      lv_obj_set_size(z.mais, 52, Marca::TOQUE);
      lv_obj_add_event_cb(z.mais, &LedPanel::_handle_callback, LV_EVENT_CLICKED, this);
    }

    leds.push_back(z);
  }

  for (auto &z : leds) {
    ler_estado(z);
  }

  ajustar_rolagem();
}

void LedPanel::ajustar_rolagem() {
  size_t n = leds.size();
  lv_coord_t altura = TOPO + BASE + (lv_coord_t)n * CARTAO_A
    + (n > 0 ? (lv_coord_t)(n - 1) * Marca::VAO : 0);
  if (altura > LISTA_A) {
    lv_obj_add_flag(leds_cont, LV_OBJ_FLAG_SCROLLABLE);
  } else {
    lv_obj_scroll_to_y(leds_cont, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(leds_cont, LV_OBJ_FLAG_SCROLLABLE);
  }
}

// Lê o valor de agora no State (o mesmo que o foreground() antigo lia).
void LedPanel::ler_estado(Luz &l) {
  int pct = l.pct < 0 ? 0 : l.pct;

  // hack for output_pin leds
  auto led_value = State::get_instance()
    ->get_data(json::json_pointer(fmt::format("/printer_state/{}/value", l.id)));
  if (led_value.is_number()) {
    pct = para_pct(led_value.template get<double>());
  }

  led_value = State::get_instance()
    ->get_data(json::json_pointer(fmt::format("/printer_state/{}/color_data", l.id)));
  if (!led_value.is_null() && led_value.size() > 0) {
    // color_data = [[r,b,g,w]]
    led_value = led_value.at(0);
    if (led_value.size() == 4 && led_value.at(3).is_number()) {
      pct = para_pct(led_value.at(3).template get<double>());
    }
  }

  mostrar(l, pct);
}

void LedPanel::mostrar(Luz &l, int pct) {
  pct = std::clamp(pct, 0, 100);
  bool acesa_antes = l.pct > 0;
  bool primeira = l.pct < 0;
  l.pct = pct;

  if (l.valor != NULL) {
    Marca::texto(l.valor, fmt::format("{}%", pct).c_str());
  }
  Marca::texto(l.estado, pct > 0 ? "Ligada" : "Desligada");

  // ícone acende (PAPEL) quando a luz está ligada; só repinta se mudou
  if (primeira || acesa_antes != (pct > 0)) {
    Marca::cor_icone(l.icone, pct > 0 ? Marca::PAPEL : Marca::GRAFITE);
  }
}

// Mesmos comandos de antes:
//  output_pin -> SET_PIN PIN=X VALUE={0..1}   (handle_led_update)
//  outras     -> SET_LED LED=X WHITE={0..1}   (handle_led_update_generic)
// 0% e 100% mandam exatamente o texto dos antigos "Desligar" e "Máximo"/"Ligar";
// o meio usa a mesma conta do controle deslizante antigo.
void LedPanel::enviar(const Luz &l, int pct) {
  std::string led_name = KUtils::get_obj_name(l.id);
  if (l.pino) {
    if (pct <= 0) {
      ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE=0", led_name));
    } else if (pct >= 100) {
      ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE=1", led_name));
    } else {
      double v = (double)pct / 100.0;
      spdlog::trace("update led {} to {}", led_name, v);
      ws.gcode_script(fmt::format("SET_PIN PIN={} VALUE={}", led_name, v));
    }
  } else {
    if (pct <= 0) {
      ws.gcode_script(fmt::format("SET_LED LED={} WHITE=0", led_name));
    } else if (pct >= 100) {
      ws.gcode_script(fmt::format("SET_LED LED={} WHITE=1", led_name));
    } else {
      double v = (double)pct / 100.0;
      spdlog::trace("update led {} to {}", led_name, v);
      ws.gcode_script(fmt::format("SET_LED LED={} WHITE={}", led_name, v));
    }
  }
}

void LedPanel::foreground() {
  for (auto &l : leds) {
    ler_estado(l);
  }
  Navegacao::abrir(ledpanel_cont);
}

lv_obj_t *LedPanel::get_container() {
  return ledpanel_cont;
}

void LedPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *obj = lv_event_get_current_target(event);

  for (auto &l : leds) {
    if (obj == l.desligar || obj == l.ligar) {
      int novo = obj == l.ligar ? 100 : 0;
      enviar(l, novo);
      mostrar(l, novo);
      return;
    }

    if (obj == l.menos || obj == l.mais) {
      int atual = l.pct < 0 ? 0 : l.pct;
      int novo = obj == l.menos ? std::max(0, atual - PASSO) : std::min(100, atual + PASSO);
      if (novo != atual) {
        enviar(l, novo);
        mostrar(l, novo);
      }
      return;
    }

    if (obj == l.valor_cont) {
      Numpad *np = Numpad::compartilhado();
      if (np == NULL) {
        return;
      }
      // procura pelo id na volta (a lista pode ter sido refeita numa reconexão)
      std::string id = l.id;
      np->abrir(l.pct < 0 ? 0 : l.pct, [this, id](double d) {
        int novo = std::clamp(static_cast<int>(std::lround(d)), 0, 100);
        for (auto &g : leds) {
          if (g.id == id) {
            enviar(g, novo);
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
