#include "printer_select_panel.h"
#include "guppyscreen.h"
#include "config.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "hv/json.hpp"
#include "subprocess.hpp"

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;
namespace sp = subprocess;
using json = nlohmann::json;

// Subtela 424 x 272 (coordenadas da subtela):
//   cabeçalho "Impressoras"
//   nome (8,52) 176 x 44 · IP (192,52) 224 x 44
//   porta (8,100) 176 x 44 · "+ Impressora" (192,100) 224 x 44
//   lista das salvas (8,152) 408 x 112, rola na vertical (linhas de 52)
//   teclado (0,148) 424 x 124, por cima da lista enquanto digita
namespace {
  constexpr lv_coord_t LISTA_L = 408;
  constexpr lv_coord_t LINHA_IMPRESSORA = 52;
  constexpr lv_coord_t TECLADO_Y = 148;

  void estilo_campo(lv_obj_t *ta, lv_coord_t x, lv_coord_t y, lv_coord_t w) {
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_style_text_font(ta, Marca::f14(), 0);
    lv_obj_set_style_text_font(ta, Marca::f14(), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_pad_hor(ta, 12, 0);
    lv_obj_set_style_pad_ver(ta, (Marca::TOQUE - 16 - 2) / 2, 0);
    lv_obj_set_pos(ta, x, y);
    lv_obj_set_size(ta, w, Marca::TOQUE);
  }
}

PrinterSelectContainer::PrinterSelectContainer(PrinterSelectPanel &ps,
					       lv_obj_t *parent,
					       const std::string &pname,
					       const std::string &ip,
					       uint32_t port)
  : printer_select_panel(ps)
  , cont(lv_obj_create(parent))
  , name(pname)
{
  lv_obj_set_size(cont, LISTA_L, LINHA_IMPRESSORA);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_radius(cont, 0, 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 1, 0);
  lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(cont, lv_color_hex(Marca::PRETO_3), 0);

  lv_obj_t *l = Marca::criar_rotulo(cont, pname.c_str(), Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(l, 12, 8);
  lv_obj_set_width(l, 236);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);

  l = Marca::criar_rotulo(cont, fmt::format("IP: {} · Porta: {}", ip, port).c_str(),
                          Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(l, 12, 28);
  lv_obj_set_width(l, 236);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);

  lv_obj_t *btn = Marca::criar_botao(cont, "Trocar", Marca::Tipo::NORMAL);
  lv_obj_set_pos(btn, 256, 3);
  lv_obj_set_size(btn, 96, Marca::TOQUE);

  lv_obj_add_event_cb(btn, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
      PrinterSelectContainer *p = (PrinterSelectContainer*)e->user_data;
      lv_obj_t *msgbox = p->prompt(fmt::format("A tela vai reiniciar.\nTrocar para {}?",
					       p->name));
      lv_obj_add_event_cb(msgbox, [](lv_event_t *e) {
	lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));
	uint32_t clicked_btn = lv_msgbox_get_active_btn(obj);
	if(clicked_btn == 0) {


	  Config *conf = Config::get_instance();
	  conf->set<std::string>("/default_printer", ((PrinterSelectContainer*)e->user_data)->name);
	  conf->save();

	  auto init_script = conf->get<std::string>("/guppy_init_script");
	  const fs::path script(init_script);
	  if (fs::exists(script)) {
	    sp::call({init_script, "restart"});
	  }

	}

	lv_msgbox_close(obj);

      }, LV_EVENT_VALUE_CHANGED, p);

    }
  }, LV_EVENT_CLICKED, this);


  btn = Marca::criar_botao(cont, LV_SYMBOL_CLOSE, Marca::Tipo::FANTASMA);
  lv_obj_set_pos(btn, 360, 3);
  lv_obj_set_size(btn, Marca::TOQUE, Marca::TOQUE);

  lv_obj_add_event_cb(btn, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
      PrinterSelectContainer *p = (PrinterSelectContainer*)e->user_data;
      lv_obj_t *msgbox = p->prompt(fmt::format("Excluir {}?", p->name));
      lv_obj_add_event_cb(msgbox, [](lv_event_t *e) {
	lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));
	uint32_t clicked_btn = lv_msgbox_get_active_btn(obj);
	if(clicked_btn == 0) {
	  auto *p = (PrinterSelectContainer*)e->user_data;
	  p->printer_select_panel.remove_printer(p->name);
	}

	lv_msgbox_close(obj);

      }, LV_EVENT_VALUE_CHANGED, p);
    }
  }, LV_EVENT_CLICKED, this);
}

PrinterSelectContainer::~PrinterSelectContainer() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *PrinterSelectContainer::prompt(const std::string &prompt_text) {
  static const char * btns[] = {"Confirmar", "Cancelar", ""};

  // caixa da marca (336 x 160, botões de 44, sem botão marcado em limão)
  lv_obj_t *mbox1 = lv_msgbox_create(NULL, NULL, prompt_text.c_str(), btns, false);
  KUtils::style_lock_mbox(mbox1, 90);
  return mbox1;
}

PrinterSelectPanel::PrinterSelectPanel()
  : cont(lv_obj_create(lv_scr_act()))
  , printer_name(lv_textarea_create(cont))
  , moonraker_ip(lv_textarea_create(cont))
  , moonraker_port(lv_textarea_create(cont))
  , add_btn(Marca::criar_botao(cont, LV_SYMBOL_PLUS " Impressora", Marca::Tipo::NORMAL))
  , right(lv_obj_create(cont))
  , kb(lv_keyboard_create(cont))
{
  Navegacao::subtela(cont);

  Marca::criar_cabecalho(cont, "Impressoras", [](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      PrinterSelectPanel *panel = (PrinterSelectPanel*)lv_event_get_user_data(e);
      panel->fechar_teclado();
      Navegacao::fechar(panel->cont);
    }
  }, this);

  estilo_campo(printer_name, 8, 52, 176);
  lv_textarea_set_placeholder_text(printer_name, "Nome da impressora");
  lv_textarea_set_max_length(printer_name, 128);
  lv_textarea_set_accepted_chars(printer_name,
				 "abcdefghijklmnopqrstuvwxyz"
				 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				 "0123456789"
				 " -_");
  lv_obj_add_event_cb(printer_name, [](lv_event_t *e) {
    PrinterSelectPanel *p = (PrinterSelectPanel*)e->user_data;
    p->handle_input(e);
  }, LV_EVENT_ALL, this);

  estilo_campo(moonraker_ip, 192, 52, 224);
  lv_textarea_set_placeholder_text(moonraker_ip, "Endereço IP do Moonraker");
  lv_textarea_set_max_length(moonraker_ip, 45); // ipv6??
  lv_textarea_set_accepted_chars(moonraker_ip, "0123456789abcdef.:");
  lv_obj_add_event_cb(moonraker_ip, [](lv_event_t *e) {
    PrinterSelectPanel *p = (PrinterSelectPanel*)e->user_data;
    p->handle_input(e);
  }, LV_EVENT_ALL, this);

  estilo_campo(moonraker_port, 8, 100, 176);
  lv_textarea_set_text(moonraker_port, "7125");
  lv_textarea_set_placeholder_text(moonraker_port, "Porta do Moonraker");
  lv_textarea_set_max_length(moonraker_port, 5);
  lv_textarea_set_accepted_chars(moonraker_port, "0123456789");
  lv_obj_add_event_cb(moonraker_port, [](lv_event_t *e) {
    PrinterSelectPanel *p = (PrinterSelectPanel*)e->user_data;
    p->handle_input(e);
  }, LV_EVENT_ALL, this);

  lv_obj_set_pos(add_btn, 192, 100);
  lv_obj_set_size(add_btn, 224, Marca::TOQUE);

  // lv_obj_set_style_bg_img_src(btn, LV_SYMBOL_PLUS, 0);
  lv_obj_add_event_cb(add_btn, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
      PrinterSelectPanel *p = (PrinterSelectPanel*)e->user_data;
      Config *conf = Config::get_instance();

      auto pname = std::string(lv_textarea_get_text(p->printer_name));
      auto ip = std::string(lv_textarea_get_text(p->moonraker_ip));
      // sem nome ou sem IP não há o que gravar (evita uma impressora vazia)
      if (pname.empty() || ip.empty()) {
        return;
      }

      const char *mp = lv_textarea_get_text(p->moonraker_port);
      // porta vazia: a padrão (std::stoi("") derrubava a tela)
      auto port = (mp != NULL && mp[0] != '\0') ? std::stoi(mp) : 7125;

      json new_printer = {
	      {"moonraker_api_key", false},
	      {"moonraker_host", ip},
	      {"moonraker_port", port},
	      {"monitored_sensors", {} },
	      {"fans", {} },
	      {"default_macros", {
		  {"load_filament", "_GUPPY_LOAD_MATERIAL"},
		  {"unload_filament", "_GUPPY_QUIT_MATERIAL"},
		  { "cooldown", "SET_HEATER_TEMPERATURE HEATER=extruder TARGET=0\nSET_HEATER_TEMPERATURE HEATER=heater_bed TARGET=0"}
		}
	      }
      };

      auto printers = conf->get<json>("/printers");
      printers[pname] = new_printer;
      conf->set<json>("/printers", printers);
      conf->save();

      p->add_printer(pname, ip, port);

      p->fechar_teclado();

      if (conf->get_json("/default_printer").is_null()) {
        // connect to the one and only added printer
        conf->set<std::string>("/default_printer", pname);
        conf->save();
        conf->init(conf->get<std::string>("/config_path"), conf->get<std::string>("/thumbnail_path"));
        std::string ws_url = fmt::format("ws://{}:{}/websocket", ip, port);
        GuppyScreen::get()->connect_ws(ws_url);
      }
    }
  }, LV_EVENT_CLICKED, this);

  // lista das impressoras salvas
  Marca::cartao(right);
  lv_obj_set_pos(right, 8, 152);
  lv_obj_set_size(right, LISTA_L, Marca::TELA_A - 152 - 8);
  lv_obj_add_flag(right, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(right, LV_DIR_VER);
  lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scrollbar_mode(right, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(right, 0, 0);

  Config *conf = Config::get_instance();
  const auto &configured_printers = conf->get<json>("/printers");

  for (auto &el : configured_printers.items()) {
    auto &v = el.value();
    printers.insert({el.key(), std::make_shared<PrinterSelectContainer>(
			*this,
			right,
			el.key(),
			v["moonraker_host"].template get<std::string>(),
			v["moonraker_port"].template get<uint32_t>())});
  }

  // teclado dentro da subtela
  lv_obj_align(kb, LV_ALIGN_TOP_LEFT, 0, TECLADO_Y);
  lv_obj_set_size(kb, Marca::CONT_L, Marca::TELA_A - TECLADO_Y);
  lv_obj_set_style_text_font(kb, Marca::f14(), LV_PART_ITEMS);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

PrinterSelectPanel::~PrinterSelectPanel() {
  // as linhas apagam o próprio objeto: primeiro elas, depois a subtela
  printers.clear();
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void PrinterSelectPanel::fechar_teclado() {
  lv_obj_t *ta = lv_keyboard_get_textarea(kb);
  if (ta != NULL) {
    lv_obj_clear_state(ta, LV_STATE_FOCUSED);
  }
  lv_keyboard_set_textarea(kb, NULL);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

void PrinterSelectPanel::handle_input(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * ta = lv_event_get_target(e);

  if(code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
    if (ta == moonraker_ip || ta == moonraker_port) {
      lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    } else {
      lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    }

    lv_keyboard_set_textarea(kb, ta);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  }

  // OK ou fechar do teclado: esconde o teclado (o "+ Impressora" e a lista
  // ficam embaixo dele)
  if (code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
    fechar_teclado();
  }

}

void PrinterSelectPanel::remove_printer(const std::string &n) {
  const auto &el = printers.find(n);
  if (el != printers.end()) {
    printers.erase(el);

    Config *conf = Config::get_instance();
    auto printers = conf->get<json>("/printers");
    printers.erase(n);
    conf->set<json>("/printers", printers);
    conf->save();
  }
}

void PrinterSelectPanel::add_printer(const std::string &n,
				     const std::string &ip,
				     uint32_t port) {
  printers.insert({n, std::make_shared<PrinterSelectContainer>(*this, right, n, ip, port)});
}

void PrinterSelectPanel::foreground() {
  Navegacao::abrir(cont);
}
