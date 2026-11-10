#include "tmc_status_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "spdlog/spdlog.h"

TmcStatusPanel::TmcStatusPanel(KWebSocketClient &c,
			      std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , lista(lv_obj_create(cont))
  , top(lv_obj_create(lista))
  , toggle(lv_switch_create(top))
{
  // subtela 424 x 272 com cabeçalho "Medições TMC" (voltar = fechar a subtela)
  Navegacao::subtela(cont);
  Marca::criar_cabecalho(cont, "Medições TMC");

  // lista abaixo do cabeçalho: aviso + chave, depois um bloco por motor
  lv_obj_set_pos(lista, 0, Marca::TOQUE);
  lv_obj_set_size(lista, Marca::CONT_L, Marca::TELA_A - Marca::TOQUE);
  lv_obj_set_style_pad_all(lista, 0, 0);
  lv_obj_set_style_pad_hor(lista, Marca::MARGEM, 0);
  lv_obj_set_style_pad_bottom(lista, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(lista, 0, 0);
  lv_obj_set_flex_flow(lista, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(lista, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(lista, LV_SCROLLBAR_MODE_ACTIVE);
  lv_obj_clear_flag(lista, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(lista, LV_OBJ_FLAG_SCROLL_ELASTIC);

  // cartão com o aviso (12 G) e a chave 44 x 24 à direita
  Marca::cartao(top);
  lv_obj_set_size(top, LV_PCT(100), 68);   // 3 linhas de 15 + 2 x 10 de margem
  lv_obj_set_style_pad_all(top, 10, 0);
  lv_obj_t *l = Marca::criar_rotulo(top,
    "As medições TMC são experimentais e vêm desligadas. "
    "Ligue quando precisar. Para mais detalhes, veja a folha de "
    "dados (datasheet) do driver TMC.", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_width(l, 336);   // 3 linhas (a maior mede 335 px)
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_set_size(toggle, 44, 24);
  lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_ext_click_area(toggle, 10);
  lv_obj_clear_state(toggle, LV_STATE_CHECKED);

  lv_obj_add_event_cb(toggle, [](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
      TmcStatusPanel *p = (TmcStatusPanel*)e->user_data;
      if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
	p->ws.gcode_script("_GUPPY_LOAD_MODULE SECTION=tmcstatus");
      } else {
	p->ws.gcode_script("_GUPPY_UNLOAD_MODULE SECTION=tmcstatus");
      }
    }
  }, LV_EVENT_VALUE_CHANGED, this);

  ws.register_notify_update(this);
}

TmcStatusPanel::~TmcStatusPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void TmcStatusPanel::foreground() {
  Navegacao::abrir(cont);
}

void TmcStatusPanel::background() {
  Navegacao::fechar(cont);
}

void TmcStatusPanel::init(json &j) {
  auto tmc_status = j["/result/status/tmcstatus"_json_pointer];
  if (!tmc_status.is_null()) {
    if (!tmc_status.empty()) {
      lv_obj_add_state(toggle, LV_STATE_CHECKED);
    }
    for (auto &el : tmc_status.items()) {
      const auto &s = metrics.find(el.key());
      if (s != metrics.end()) {

	s->second->update(el.value());
      } else {
	spdlog::debug("tmc stepper created {}", el.key());
	auto tmc_status_cont = std::make_shared<TmcStatusContainer>(ws, lista, el.key());
	metrics.insert({el.key(), tmc_status_cont});
	tmc_status_cont->update(el.value());
      }
    }
  }

}

void TmcStatusPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto tmc_status = j["/params/0/tmcstatus"_json_pointer];
  if (!tmc_status.is_null()) {
    for (auto &el : tmc_status.items()) {
      const auto &s = metrics.find(el.key());
      if (s != metrics.end()) {
	// spdlog::debug("tmc stepper found {}", el.key());

	s->second->update(el.value());
      } else {
	// criado uma vez só por motor (na primeira medição dele)
	spdlog::debug("tmc stepper created {}", el.key());
	auto tmc_status_cont = std::make_shared<TmcStatusContainer>(ws, lista, el.key());
	metrics.insert({el.key(), tmc_status_cont});
	tmc_status_cont->update(el.value());
      }
    }
  }
}
