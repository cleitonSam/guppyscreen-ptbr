#include "sysinfo_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "config.h"
#include "touch_beep.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <map>
#include <unistd.h>

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

#ifdef GUPPYSCREEN_VERSION
#define GS_VERSION GUPPYSCREEN_VERSION
#else
#define GS_VERSION "dev-snapshot"
#endif

// ============================================================================
// ListaCfg: peças da lista das abas de Configurações
// ============================================================================

namespace ListaCfg {

  static inline lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

  void preparar_conteudo(lv_obj_t *obj, Aba a, bool rolavel) {
    Navegacao::colocar(obj, a);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    if (rolavel) {
      // rola só na vertical, sem inércia e sem elástico (nada de animação)
      lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_scroll_dir(obj, LV_DIR_VER);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
      lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
      lv_obj_set_style_pad_bottom(obj, Marca::MARGEM, 0);   // folga no fim da rolagem
    } else {
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    }
  }

  lv_obj_t *criar_cartao(lv_obj_t *pai, lv_coord_t y, int linhas) {
    lv_obj_t *cartao = Marca::criar_cartao(pai, CARTAO_X, y, CARTAO_L, LINHA * linhas);
    for (int i = 1; i < linhas; i++) {
      lv_obj_t *sep = lv_obj_create(cartao);
      lv_obj_set_pos(sep, ROTULO_X, LINHA * i);
      lv_obj_set_size(sep, CARTAO_L - 2 * ROTULO_X, 1);
      lv_obj_set_style_bg_color(sep, c(Marca::PRETO_3), 0);
      lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(sep, 0, 0);
      lv_obj_set_style_radius(sep, 0, 0);
      lv_obj_set_style_pad_all(sep, 0, 0);
      lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    }
    return cartao;
  }

  lv_obj_t *rotulo(lv_obj_t *cartao, int i, const char *texto) {
    lv_obj_t *l = Marca::criar_rotulo(cartao, texto, Marca::f14(), Marca::PAPEL);
    lv_obj_set_pos(l, ROTULO_X, LINHA * i + 13);
    lv_obj_set_width(l, ROTULO_L);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    return l;
  }

  lv_obj_t *linha_seta(lv_obj_t *cartao, int i, const char *texto, lv_obj_t **valor) {
    lv_obj_t *linha = lv_obj_create(cartao);
    lv_obj_set_pos(linha, 0, LINHA * i);
    lv_obj_set_size(linha, CARTAO_L, LINHA);
    lv_obj_set_style_pad_all(linha, 0, 0);
    lv_obj_set_style_border_width(linha, 0, 0);
    lv_obj_set_style_radius(linha, Marca::RAIO, 0);
    lv_obj_set_style_bg_opa(linha, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(linha, c(Marca::APERTADO), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(linha, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_clear_flag(linha, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(linha, LV_OBJ_FLAG_CLICKABLE);

    // filho 0: rótulo (habilitar_linha troca a cor dele)
    lv_obj_t *l = Marca::criar_rotulo(linha, texto, Marca::f14(), Marca::PAPEL);
    lv_obj_set_pos(l, ROTULO_X, 13);
    lv_obj_set_width(l, valor != nullptr ? 230 : ROTULO_L);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);

    lv_obj_t *seta = Marca::criar_rotulo(linha, LV_SYMBOL_RIGHT, Marca::f14(), Marca::GRAFITE);
    lv_obj_set_pos(seta, CTRL_X + CTRL_L - 14, 13);
    lv_obj_set_width(seta, 14);
    lv_obj_set_style_text_align(seta, LV_TEXT_ALIGN_RIGHT, 0);

    if (valor != nullptr) {
      lv_obj_t *v = Marca::criar_rotulo(linha, "", Marca::f14(), Marca::GRAFITE);
      lv_obj_set_pos(v, CTRL_X, 13);
      lv_obj_set_width(v, CTRL_L - 18);
      lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
      *valor = v;
    }
    return linha;
  }

  void habilitar_linha(lv_obj_t *linha, bool sim) {
    if (linha == nullptr) {
      return;
    }
    if (sim) {
      lv_obj_add_flag(linha, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_clear_flag(linha, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_t *l = lv_obj_get_child(linha, 0);
    if (l != NULL) {
      lv_obj_set_style_text_color(l, c(sim ? Marca::PAPEL : Marca::GRAFITE), 0);
    }
  }

  void lista(lv_obj_t *dd, int i) {
    lv_obj_set_pos(dd, CTRL_X, LINHA * i);
    lv_obj_set_size(dd, CTRL_L, LINHA);
    lv_obj_set_style_bg_opa(dd, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dd, 0, 0);
    lv_obj_set_style_shadow_width(dd, 0, 0);
    lv_obj_set_style_outline_width(dd, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_radius(dd, Marca::RAIO, 0);
    lv_obj_set_style_pad_top(dd, (LINHA - 16) / 2, 0);   // f14 tem 16 de linha
    lv_obj_set_style_pad_bottom(dd, 0, 0);
    lv_obj_set_style_pad_right(dd, 0, 0);
    lv_obj_set_style_text_font(dd, Marca::f14(), 0);
    lv_obj_set_style_text_color(dd, c(Marca::PAPEL), 0);
    // a seta usa a parte INDICATOR
    lv_obj_set_style_text_font(dd, Marca::f14(), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(dd, c(Marca::GRAFITE), LV_PART_INDICATOR);
    lv_dropdown_set_symbol(dd, LV_SYMBOL_DOWN);

    lv_obj_t *lst = lv_dropdown_get_list(dd);
    if (lst != NULL) {
      lv_obj_set_style_text_font(lst, Marca::f14(), 0);
    }
    alinhar_lista(dd);
  }

  void alinhar_lista(lv_obj_t *dd) {
    // O LVGL desenha o texto a partir do recuo da esquerda e a seta colada à
    // direita: o recuo é calculado para o texto terminar 6 px antes da seta
    // (como na maquete), mantendo a área de toque com os 144 px inteiros.
    char buf[64];
    lv_dropdown_get_selected_str(dd, buf, sizeof(buf));
    lv_point_t txt;
    lv_txt_get_size(&txt, buf, Marca::f14(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_point_t sim;
    lv_txt_get_size(&sim, LV_SYMBOL_DOWN, Marca::f14(), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_coord_t recuo = CTRL_L - sim.x - 6 - txt.x;
    if (recuo < 0) {
      recuo = 0;
    }
    lv_obj_set_style_pad_left(dd, recuo, 0);
  }

  void chave(lv_obj_t *sw, int i) {
    lv_obj_set_size(sw, 44, 24);
    lv_obj_set_pos(sw, CTRL_X + CTRL_L - 44, LINHA * i + (LINHA - 24) / 2);
    lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);   // bolinha de 18 com 3 px de folga
    lv_obj_set_ext_click_area(sw, 10);                 // toque de 64 x 44
  }

  lv_obj_t *texto_direita(lv_obj_t *cartao, int i, const char *texto) {
    lv_obj_t *l = Marca::criar_rotulo(cartao, texto, Marca::f14(), Marca::GRAFITE);
    lv_obj_set_pos(l, CTRL_X, LINHA * i + 13);
    lv_obj_set_width(l, CTRL_L);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    return l;
  }

  lv_obj_t *rodape(lv_obj_t *pai, lv_coord_t x, lv_coord_t largura, const char *texto) {
    lv_obj_t *l = Marca::criar_rotulo(pai, texto, Marca::f12(), Marca::GRAFITE);
    lv_obj_set_pos(l, x, RODAPE_Y);
    lv_obj_set_width(l, largura);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    return l;
  }

  std::string ip_atual(bool so_wifi) {
    std::string outro;
    for (const auto &iface : KUtils::get_interfaces()) {
      if (iface.empty()) {
        continue;
      }
      bool wifi = iface[0] == 'w';
      if (so_wifi && !wifi) {
        continue;
      }
      std::string ip = KUtils::interface_ip(iface);
      if (ip.empty() || ip == "0.0.0.0") {
        continue;
      }
      if (wifi) {
        return ip;
      }
      if (outro.empty()) {
        outro = ip;
      }
    }
    return outro;
  }
}

// ============================================================================
// SysInfoPanel
// ============================================================================

std::vector<std::string> SysInfoPanel::log_levels = {
  "trace",
  "debug",
  "info",
  "warn"
};

struct SleepOption {
  const char *label;
  int32_t seconds;
};

static const SleepOption sleep_options[] = {
  {"Nunca", -1},
  {"30 s", 30},
  {"1 min", 60},
  {"5 min", 300},
  {"10 min", 600},
  {"30 min", 1800},
  {"1 h", 3600}
};

// Brightness presets as percentages of the device's max_brightness. 10% is
// the floor (chosen to keep the screen readable enough to navigate back to
// this menu and recover from a too-dark setting).
struct BrightnessOption {
  const char *label;
  int percent;
};

static const BrightnessOption brightness_options[] = {
  {"Mín. (10%)",   10},
  {"Baixo (25%)",  25},
  {"Médio (50%)",  50},
  {"Alto (75%)",   75},
  {"Máx. (100%)", 100},
};

static void set_switch(lv_obj_t *sw, bool on) {
  if (on) {
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(sw, LV_STATE_CHECKED);
  }
}

SysInfoPanel::SysInfoPanel(lv_obj_t *sistema, lv_coord_t y)
  : tela_cont(lv_obj_create(Navegacao::aba(Aba::TELA)))
  , tela_cartao(NULL)
  , brightness_dd(NULL)
  , display_sleep_dd(NULL)
  , touch_beep_toggle(NULL)
  , versao_label(NULL)
  , wifi_label(NULL)
  , ajustes_cartao(NULL)
  , loglevel_dd(NULL)
  , loglevel(1)
  , def_temp_dd(NULL)
  , z_icon_toggle(NULL)
  , y_icon_toggle(NULL)
  , reset_row(NULL)
  , reset_janela(NULL)
{
  Config *conf = Config::get_instance();

  // ---------------------------------------------------------------- aba Tela
  ListaCfg::preparar_conteudo(tela_cont, Aba::TELA, false);

  // Brilho só aparece se o aparelho tem luz de fundo controlável.
  int b_max = KUtils::backlight_max();
  int linhas = b_max > 0 ? 4 : 3;
  tela_cartao = ListaCfg::criar_cartao(tela_cont, 7, linhas);
  int i = 0;

  if (b_max > 0) {
    ListaCfg::rotulo(tela_cartao, i, "Brilho");
    brightness_dd = lv_dropdown_create(tela_cartao);

    std::string b_dd_options;
    for (const auto &opt : brightness_options) {
      b_dd_options += opt.label;
      b_dd_options += '\n';
    }
    b_dd_options.pop_back();
    lv_dropdown_set_options(brightness_dd, b_dd_options.c_str());

    // Pick the saved value's closest preset (or Max if nothing saved).
    int saved_pct = 100;
    auto v_b = conf->get_json("/display_brightness");
    if (!v_b.is_null()) {
      saved_pct = (v_b.template get<int>() * 100) / b_max;
    }
    size_t best_idx = std::size(brightness_options) - 1;
    int best_diff = std::abs(saved_pct - brightness_options[best_idx].percent);
    for (size_t k = 0; k < std::size(brightness_options); ++k) {
      int d = std::abs(saved_pct - brightness_options[k].percent);
      if (d < best_diff) {
        best_diff = d;
        best_idx = k;
      }
    }
    lv_dropdown_set_selected(brightness_dd, best_idx);
    ListaCfg::lista(brightness_dd, i);

    lv_obj_add_event_cb(brightness_dd, &SysInfoPanel::_handle_callback,
      LV_EVENT_VALUE_CHANGED, this);
    i++;
  }

  // Apagar tela
  ListaCfg::rotulo(tela_cartao, i, "Apagar tela");
  display_sleep_dd = lv_dropdown_create(tela_cartao);
  {
    std::string dd_options;
    for (const auto &opt : sleep_options) {
      dd_options += opt.label;
      dd_options += '\n';
    }
    dd_options.pop_back();
    lv_dropdown_set_options(display_sleep_dd, dd_options.c_str());

    auto v = conf->get_json("/display_sleep_sec");
    if (!v.is_null()) {
      auto sleep_sec = v.template get<int32_t>();
      for (size_t k = 0; k < std::size(sleep_options); ++k) {
        if (sleep_options[k].seconds == sleep_sec) {
          lv_dropdown_set_selected(display_sleep_dd, k);
          break;
        }
      }
    }
  }
  ListaCfg::lista(display_sleep_dd, i);
  lv_obj_add_event_cb(display_sleep_dd, &SysInfoPanel::_handle_callback,
    LV_EVENT_VALUE_CHANGED, this);
  i++;

  // Som ao tocar: opt-in audible click feedback on the hardware buzzer. Off by
  // default; mirrors the stock Creality KE click sound.
  ListaCfg::rotulo(tela_cartao, i, "Som ao tocar");
  touch_beep_toggle = lv_switch_create(tela_cartao);
  {
    auto v = conf->get_json("/touch_beep");
    set_switch(touch_beep_toggle, !v.is_null() && v.template get<bool>());
  }
  ListaCfg::chave(touch_beep_toggle, i);
  lv_obj_add_event_cb(touch_beep_toggle, &SysInfoPanel::_handle_callback,
    LV_EVENT_VALUE_CHANGED, this);
  i++;

  // Confirmar parada de emergência: a tela SEMPRE pergunta antes (a chave
  // /prompt_emergency_stop continua no arquivo, mas não desliga mais a
  // pergunta). Por isso aqui não tem chave, só o texto.
  ListaCfg::rotulo(tela_cartao, i, "Confirmar parada de emergência");
  ListaCfg::texto_direita(tela_cartao, i, "Sempre");

  // rodapé: versão e Wi-Fi
  versao_label = ListaCfg::rodape(tela_cont, 8, 260, "Versão: GuppyScreen " GS_VERSION);
  wifi_label = ListaCfg::rodape(tela_cont, 276, 140, "");
  lv_obj_set_style_text_align(wifi_label, LV_TEXT_ALIGN_RIGHT, 0);

  Navegacao::ao_mostrar(Aba::TELA, [this]() { atualizar(); });

  // ------------------------------------------------------- aba Sistema (cartão)
  ajustes_cartao = ListaCfg::criar_cartao(sistema, y, 5);

  // Nível de registro
  ListaCfg::rotulo(ajustes_cartao, 0, "Nível de registro");
  loglevel_dd = lv_dropdown_create(ajustes_cartao);
  lv_dropdown_set_options(loglevel_dd, fmt::format("{}", fmt::join(log_levels, "\n")).c_str());
  {
    auto df = conf->get_json("/default_printer");
    json j_null;
    auto v = !df.empty() ? conf->get_json(conf->df() + "log_level") : j_null;
    if (!v.is_null()) {
      auto it = std::find(log_levels.begin(), log_levels.end(), v.template get<std::string>());
      if (it != std::end(log_levels)) {
        loglevel = std::distance(log_levels.begin(), it);
        lv_dropdown_set_selected(loglevel_dd, loglevel);
      }
    } else {
      lv_dropdown_set_selected(loglevel_dd, loglevel);
    }
  }
  ListaCfg::lista(loglevel_dd, 0);
  lv_obj_add_event_cb(loglevel_dd, &SysInfoPanel::_handle_callback,
    LV_EVENT_VALUE_CHANGED, this);

  // Temp. padrão (bico)
  ListaCfg::rotulo(ajustes_cartao, 1, "Temp. padrão");
  def_temp_dd = lv_dropdown_create(ajustes_cartao);
  lv_dropdown_set_options(def_temp_dd, "180\n200\n220\n240\n260\n280\n300");
  {
    auto def_ext_temp = conf->get_json("/default_extruder_temp");
    if (!def_ext_temp.is_null()) {
      std::string val = std::to_string(def_ext_temp.template get<int>());
      int32_t idx = lv_dropdown_get_option_index(def_temp_dd, val.c_str());
      if (idx != -1) {
        lv_dropdown_set_selected(def_temp_dd, idx);
      }
    }
  }
  ListaCfg::lista(def_temp_dd, 1);
  lv_obj_add_event_cb(def_temp_dd, &SysInfoPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  // Inverter direção do Z
  ListaCfg::rotulo(ajustes_cartao, 2, "Inverter direção do Z");
  z_icon_toggle = lv_switch_create(ajustes_cartao);
  {
    auto v = conf->get_json("/invert_z_direction");
    set_switch(z_icon_toggle, !v.is_null() && v.template get<bool>());   // padrão: desligada
  }
  ListaCfg::chave(z_icon_toggle, 2);
  lv_obj_add_event_cb(z_icon_toggle, &SysInfoPanel::_handle_callback,
    LV_EVENT_VALUE_CHANGED, this);

  /* Y direction (bed-slinger): the up-arrow/Y+ jog moves the bed, which on a
   * bed-slinger travels opposite to the toolhead frame, so the arrows can feel
   * reversed. This flips the homing-panel Y jog like Invert Z does for Z. */
  ListaCfg::rotulo(ajustes_cartao, 3, "Inverter direção do Y");
  y_icon_toggle = lv_switch_create(ajustes_cartao);
  {
    auto v = conf->get_json("/invert_y_direction");
    set_switch(y_icon_toggle, !v.is_null() && v.template get<bool>());   // padrão: desligada
  }
  ListaCfg::chave(y_icon_toggle, 3);
  lv_obj_add_event_cb(y_icon_toggle, &SysInfoPanel::_handle_callback,
    LV_EVENT_VALUE_CHANGED, this);

  // Opções de redefinição (abre a janela)
  reset_row = ListaCfg::linha_seta(ajustes_cartao, 4, "Opções de redefinição");
  lv_obj_add_event_cb(reset_row, &SysInfoPanel::_handle_callback, LV_EVENT_CLICKED, this);
}

SysInfoPanel::~SysInfoPanel() {
  // ajustes_cartao é filho do conteúdo da aba Sistema (do SettingPanel), que
  // apaga tudo junto; aqui só o que é deste painel.
  if (reset_janela != NULL) {
    lv_obj_del(reset_janela);
    reset_janela = NULL;
  }
  if (tela_cont != NULL) {
    lv_obj_del(tela_cont);
    tela_cont = NULL;
  }
}

void SysInfoPanel::foreground() {
  atualizar();
  Navegacao::ir_para(Aba::TELA);
}

void SysInfoPanel::atualizar() {
  std::string ip = ListaCfg::ip_atual(true);
  Marca::texto(wifi_label, ip.empty() ? "Wi-Fi desconectado" : "Wi-Fi conectado");
}

void SysInfoPanel::handle_callback(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(e);

    if (btn == reset_row) {
      show_reset_options();
    }
  } else if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *obj = lv_event_get_target(e);
    Config *conf = Config::get_instance();
    if (obj == loglevel_dd) {
      ListaCfg::alinhar_lista(loglevel_dd);
      auto idx = lv_dropdown_get_selected(loglevel_dd);
      if (idx != loglevel) {
        if (loglevel < log_levels.size()) {
          loglevel = idx;
          auto ll = spdlog::level::from_str(log_levels[loglevel]);

          spdlog::set_level(ll);
          spdlog::flush_on(ll);
          spdlog::debug("setting log_level to {}", log_levels[loglevel]);
          conf->set<std::string>(conf->df() + "log_level", log_levels[loglevel]);
          conf->save();
        }
      }
    } else if (obj == display_sleep_dd) {
      ListaCfg::alinhar_lista(display_sleep_dd);
      char buf[64];
      lv_dropdown_get_selected_str(display_sleep_dd, buf, sizeof(buf));
      std::string selected_label = buf;

      for (const auto &opt : sleep_options) {
        if (opt.label == selected_label) {
          conf->set<int32_t>("/display_sleep_sec", opt.seconds);
          conf->save();
          break;
        }
      }
    } else if (obj == z_icon_toggle) {
      bool inverted = lv_obj_has_state(z_icon_toggle, LV_STATE_CHECKED);
      conf->set<bool>("/invert_z_direction", inverted);
      conf->save();
    } else if (obj == y_icon_toggle) {
      bool inverted = lv_obj_has_state(y_icon_toggle, LV_STATE_CHECKED);
      conf->set<bool>("/invert_y_direction", inverted);
      conf->save();
    } else if (obj == touch_beep_toggle) {
      bool enabled = lv_obj_has_state(touch_beep_toggle, LV_STATE_CHECKED);
      conf->set<bool>("/touch_beep", enabled);
      conf->save();
      TouchBeep::set_enabled(enabled);
      if (enabled) {
        TouchBeep::beep(); // immediate confirmation that it works
      }
    } else if (obj == def_temp_dd) {
      ListaCfg::alinhar_lista(def_temp_dd);
      char buf[64];
      lv_dropdown_get_selected_str(def_temp_dd, buf, sizeof(buf));
      conf->set<int>("/default_extruder_temp", std::stoi(buf));
      conf->save();
    } else if (brightness_dd != NULL && obj == brightness_dd) {
      ListaCfg::alinhar_lista(brightness_dd);
      // Resolve preset → absolute brightness, apply, and persist.
      auto idx = lv_dropdown_get_selected(brightness_dd);
      if (idx < std::size(brightness_options)) {
        int b_max = KUtils::backlight_max();
        int value = (b_max * brightness_options[idx].percent) / 100;
        if (value < 1) value = 1;  // never write 0 from settings (sleep path does that)
        KUtils::backlight_set(value);
        conf->set<int>("/display_brightness", value);
        conf->save();
      }
    }
  }
}

// Janela "Opções de redefinição": tela inteira em lv_layer_top(), lisa e
// opaca, criada uma vez só e depois só mostrada/escondida.
void SysInfoPanel::show_reset_options() {
  if (reset_janela != NULL) {
    lv_obj_clear_flag(reset_janela, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(reset_janela);
    return;
  }

  reset_janela = lv_obj_create(lv_layer_top());
  KUtils::style_dialog_overlay(reset_janela);
  lv_obj_set_pos(reset_janela, 0, 0);
  lv_obj_set_size(reset_janela, Marca::TELA_L, Marca::TELA_A);
  lv_obj_add_flag(reset_janela, LV_OBJ_FLAG_CLICKABLE);   // bloqueia o que está atrás

  lv_obj_t *title = Marca::criar_rotulo(reset_janela, "Opções de redefinição", Marca::f20(), Marca::PAPEL);
  lv_obj_set_pos(title, 16, 11);
  lv_obj_set_width(title, 340);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

  lv_obj_t *close_btn = Marca::criar_botao(reset_janela, "Fechar", Marca::Tipo::NORMAL);
  lv_obj_set_pos(close_btn, 368, 4);
  lv_obj_set_size(close_btn, 104, Marca::TOQUE);
  lv_obj_add_event_cb(close_btn, [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      auto *self = (SysInfoPanel *)lv_event_get_user_data(e);
      lv_obj_add_flag(self->reset_janela, LV_OBJ_FLAG_HIDDEN);
    }
  }, LV_EVENT_CLICKED, this);

  // cada opção: botão 464 x 66 com título f14 e explicação f12 GRAFITE
  auto make_opt_btn = [&](lv_coord_t y, const char *lbl_text, const char *desc_text,
                          Marca::Tipo tipo) -> lv_obj_t * {
    lv_obj_t *btn = lv_btn_create(reset_janela);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_pos(btn, 8, y);
    lv_obj_set_size(btn, 464, 66);

    lv_obj_t *lbl = Marca::criar_rotulo(btn, lbl_text, Marca::f14(), Marca::PAPEL);
    lv_obj_set_pos(lbl, 12, 9);
    lv_obj_set_width(lbl, 440);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);

    lv_obj_t *desc = Marca::criar_rotulo(btn, desc_text, Marca::f12(), Marca::GRAFITE);
    lv_obj_set_pos(desc, 12, 29);
    lv_obj_set_width(desc, 440);
    lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);

    Marca::estilo_botao(btn, tipo);
    return btn;
  };

  lv_obj_t *btn1 = make_opt_btn(52,
    "Redefinir as configurações da tela",
    "Apaga as configurações da tela e a disposição dos sensores.\nA tela reinicia com os valores padrão.",
    Marca::Tipo::NORMAL
  );

  lv_obj_t *btn2 = make_opt_btn(124,
    "Restaurar a impressora de fábrica",
    "Apaga o OpenKE, a configuração do Klipper, os arquivos de impressão\ne as calibrações. Reinicia com o firmware original da Creality.",
    Marca::Tipo::CONTORNO_PERIGO
  );

  lv_obj_t *btn3 = make_opt_btn(196,
    "Refazer a calibração do toque",
    "Apaga os valores salvos da calibração do toque.\nA tela reinicia e pede para calibrar de novo.",
    Marca::Tipo::NORMAL
  );

  lv_obj_add_event_cb(btn1, [](lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ((SysInfoPanel *)e->user_data)->show_reset_confirm(
      "Redefinir as configurações da tela?",
      "Apaga o guppyconfig.json e reinicia a tela.\nO Klipper continua funcionando.\n\nIsso não pode ser desfeito.",
      []() {
        Config *conf = Config::get_instance();
        fs::remove(conf->get_path());
        spdlog::warn("GuppyScreen config reset.");
        _exit(0);
      }
    );
  }, LV_EVENT_CLICKED, this);

  lv_obj_add_event_cb(btn2, [](lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ((SysInfoPanel *)e->user_data)->show_reset_confirm(
      "Restaurar a impressora de fábrica?",
      "Apaga o OpenKE, a configuração do Klipper,\nos arquivos de impressão e as calibrações.\nReinicia com o firmware original.\n\nIsso não pode ser desfeito.",
      []() {
        spdlog::warn("Factory reset printer: executing S58factoryreset reset");
        system("/etc/init.d/S58factoryreset reset");
      }
    );
  }, LV_EVENT_CLICKED, this);

  lv_obj_add_event_cb(btn3, [](lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ((SysInfoPanel *)e->user_data)->show_reset_confirm(
      "Refazer a calibração do toque?",
      "Apaga a calibração salva.\nA tela reinicia e pede para calibrar de novo.",
      []() {
        spdlog::warn("Touch calibration reset requested.");
        Config *conf = Config::get_instance();
        conf->set<bool>("/touch_calibrated", true);
        conf->set<json>("/touch_calibration_coeff", json());
        conf->save();
        _exit(0);
      }
    );
  }, LV_EVENT_CLICKED, this);
}

void SysInfoPanel::show_reset_confirm(const char *title, const char *detail,
                                       const std::function<void()> &cb) {
  static const char *btns[] = {"Confirmar", "Cancelar", ""};

  lv_obj_t *mbox = lv_msgbox_create(NULL, NULL,
    fmt::format("{}\n\n{}", title, detail).c_str(), btns, false);
  KUtils::style_dialog_msgbox(mbox);
  lv_obj_set_style_pad_all(mbox, 16, 0);

  lv_obj_t *msg = ((lv_msgbox_t *)mbox)->text;
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(msg, LV_PCT(100));
  lv_obj_center(msg);

  // sem LV_BTNMATRIX_CTRL_CHECKED (seria limão); "Confirmar" (esquerda) em vermelho
  lv_obj_t *btnm = lv_msgbox_get_btns(mbox);
  lv_obj_add_flag(btnm, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_size(btnm, LV_PCT(100), Marca::TOQUE);
  lv_obj_set_size(mbox, 432, 248);

  lv_obj_add_event_cb(btnm, [](lv_event_t *e) {
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part == LV_PART_ITEMS && dsc->id == 0) {
      dsc->rect_dsc->bg_color = lv_color_hex(Marca::ERRO);
      dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
      dsc->label_dsc->color   = lv_color_hex(Marca::PAPEL);
    }
  }, LV_EVENT_DRAW_PART_BEGIN, NULL);

  auto *pcb = new std::function<void()>(cb);
  lv_obj_add_event_cb(mbox, [](lv_event_t *e) {
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));
    auto *pcb = (std::function<void()> *)e->user_data;
    if (lv_msgbox_get_active_btn(obj) == 0) {
      (*pcb)();
    }
    delete pcb;
    lv_msgbox_close(obj);
  }, LV_EVENT_VALUE_CHANGED, pcb);

  lv_obj_center(mbox);
}
