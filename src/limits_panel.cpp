#include "limits_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

LV_IMG_DECLARE(refresh_img);

// Botão "Restaurar" do SliderContainer em forma compacta (sem a imagem de
// 46 px, 104 x 44, símbolo + texto lado a lado), e o botão "máximo" (que aqui
// não existe) fora do layout, para a barra ganhar a largura.
static void compactar_linha(SliderContainer &s) {
  lv_obj_t *off = s.get_off();
  if (off != NULL) {
    lv_obj_t *img = lv_obj_get_child(off, 0);
    if (img != NULL && lv_obj_check_type(img, &lv_imgbtn_class)) {
      lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_t *txt = lv_obj_get_child(off, 1);
    if (txt != NULL && lv_obj_check_type(txt, &lv_label_class)) {
      lv_label_set_text(txt, LV_SYMBOL_REFRESH " Restaurar");
    }
    lv_obj_set_size(off, 104, Marca::TOQUE);
  }
  lv_obj_t *max = s.get_max();
  if (max != NULL) {
    lv_obj_add_flag(max, LV_OBJ_FLAG_HIDDEN);
  }
}

LimitsPanel::LimitsPanel(KWebSocketClient &c, std::mutex &l)
  : NotifyConsumer(l)
  , ws(c)
  , cont(lv_obj_create(lv_scr_act()))
  , limit_cont(lv_obj_create(cont))
  , velocity(limit_cont, "Velocidade máx. (mm/s)", &refresh_img, "Restaurar", &refresh_img, NULL, &LimitsPanel::_handle_callback, this, "")
  , acceleration(limit_cont, "Aceleração máx. (mm/s²)", &refresh_img, "Restaurar", &refresh_img, NULL, &LimitsPanel::_handle_callback, this, "")
  , square_corner(limit_cont, "Velocidade nos cantos (mm/s)", &refresh_img, "Restaurar", &refresh_img, NULL, &LimitsPanel::_handle_callback, this, "")
  , accel_to_decel(limit_cont, "Aceleração para desaceleração (mm/s²)", &refresh_img, "Restaurar", &refresh_img, NULL, &LimitsPanel::_handle_callback, this, "")
  , max_velocity_default(1000)
  , max_accel_default(20000)
  , max_accel_to_decel_default(10000)
  , square_corner_default(5)
{
  // subtela 424 x 272 com cabeçalho "Limites" (voltar = fechar a subtela)
  Navegacao::subtela(cont);
  Marca::criar_cabecalho(cont, "Limites");

  // lista das 4 linhas abaixo do cabeçalho; rola na vertical se precisar,
  // sem inércia e sem elástico
  lv_obj_set_pos(limit_cont, 0, Marca::TOQUE);
  lv_obj_set_size(limit_cont, Marca::CONT_L, Marca::TELA_A - Marca::TOQUE);
  lv_obj_set_style_pad_all(limit_cont, 0, 0);
  lv_obj_set_style_pad_hor(limit_cont, Marca::MARGEM, 0);
  lv_obj_set_style_pad_top(limit_cont, 4, 0);
  lv_obj_set_style_pad_bottom(limit_cont, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(limit_cont, 6, 0);
  lv_obj_set_flex_flow(limit_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(limit_cont, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(limit_cont, LV_SCROLLBAR_MODE_ACTIVE);
  lv_obj_clear_flag(limit_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(limit_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);

  compactar_linha(velocity);
  compactar_linha(acceleration);
  compactar_linha(square_corner);
  compactar_linha(accel_to_decel);

  ws.register_notify_update(this);
}

LimitsPanel::~LimitsPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void LimitsPanel::init(json &j) {
  State *s = State::get_instance();
  auto v = s->get_data("/printer_state/configfile/settings/printer"_json_pointer);
  if (!v.is_null()) {
    if (v.contains("max_velocity")) {
      max_velocity_default = v["max_velocity"].template get<int>();      
      velocity.set_range(1, max_velocity_default);
    }

    if (v.contains("max_accel")) {
      max_accel_default = v["max_accel"].template get<int>();
      acceleration.set_range(1, max_accel_default);
    }

    if (v.contains("max_accel_to_decel")) {
      max_accel_to_decel_default = v["max_accel_to_decel"].template get<int>();
      accel_to_decel.set_range(1, max_accel_to_decel_default);
    }

    if (v.contains("square_corner_velocity")) {
      square_corner_default = v["square_corner_velocity"].template get<int>();
      square_corner.set_range(0, square_corner_default);
    }
    
  }
  
  v = j["/result/status/toolhead/max_velocity"_json_pointer];
  if (!v.is_null()) {
    velocity.update_value(v.template get<int>());
  }

  v = j["/result/status/toolhead/max_accel"_json_pointer];
  if (!v.is_null()) {
    acceleration.update_value(v.template get<int>());
  }

  v = j["/result/status/toolhead/max_accel_to_decel"_json_pointer];
  if (!v.is_null()) {
    accel_to_decel.update_value(v.template get<int>());
  }

  v = j["/result/status/toolhead/square_corner_velocity"_json_pointer];
  if (!v.is_null()) {
    square_corner.update_value(v.template get<int>());
  }
}

void LimitsPanel::foreground() {
  lv_obj_scroll_to_y(limit_cont, 0, LV_ANIM_OFF);
  Navegacao::abrir(cont);
}

void LimitsPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  auto v = j["/params/0/toolhead/max_velocity"_json_pointer];
  if (!v.is_null()) {
    velocity.update_value(v.template get<int>());
  }

  v = j["/params/0/toolhead/max_accel"_json_pointer];
  if (!v.is_null()) {
    acceleration.update_value(v.template get<int>());
  }

  v = j["/params/0/toolhead/max_accel_to_decel"_json_pointer];
  if (!v.is_null()) {
    accel_to_decel.update_value(v.template get<int>());
  }

  v = j["/params/0/toolhead/square_corner_velocity"_json_pointer];
  if (!v.is_null()) {
    square_corner.update_value(v.template get<int>());
  }
  
}

void LimitsPanel::handle_callback(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_current_target(e);

  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    lv_obj_t *obj = lv_event_get_target(e);
    int v = lv_slider_get_value(obj);

    if (obj == velocity.get_slider()) {
      enviar_limite(fmt::format("SET_VELOCITY_LIMIT VELOCITY={}", v));

    } else if (obj == acceleration.get_slider()) {
      enviar_limite(fmt::format("SET_VELOCITY_LIMIT ACCEL={}", v));

    } else if (obj == accel_to_decel.get_slider()) {
      enviar_limite(fmt::format("SET_VELOCITY_LIMIT ACCEL_TO_DECEL={}", v));
      
    } else if (obj == square_corner.get_slider()) {
      enviar_limite(fmt::format("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY={}", v));

    }
    
  } else if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (btn == velocity.get_off()) {
      enviar_limite(fmt::format("SET_VELOCITY_LIMIT VELOCITY={}", max_velocity_default));

    } else if (btn == acceleration.get_off()) {
      enviar_limite(fmt::format("SET_VELOCITY_LIMIT ACCEL={}", max_accel_default));

    } else if (btn == accel_to_decel.get_off()) {
      enviar_limite(fmt::format("SET_VELOCITY_LIMIT ACCEL_TO_DECEL={}", max_accel_to_decel_default));
      
    } else if (btn == square_corner.get_off()) {
      enviar_limite(fmt::format("SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY={}", square_corner_default));

    }
  }
}

void LimitsPanel::enviar_limite(const std::string &cmd) {
  // imprimindo: pergunta antes de mudar (antes perguntava so ao abrir a tela)
  KWebSocketClient *wsp = &ws;
  KUtils::confirm_if_printing("A impressora está imprimindo.\nMudar este limite mesmo assim?",
    [wsp, cmd]() { wsp->gcode_script(cmd); });
}
