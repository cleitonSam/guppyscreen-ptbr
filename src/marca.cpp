#include "marca.h"
#include "navegacao.h"

#include <cstring>

namespace Marca {

  const lv_font_t *f12() { return &lv_font_montserrat_12; }
  const lv_font_t *f14() { return &lv_font_montserrat_14; }
  const lv_font_t *f16() { return &lv_font_montserrat_16; }
  const lv_font_t *f20() { return &lv_font_montserrat_20; }
  const lv_font_t *f40() { return &lv_font_montserrat_40; }

  static inline lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

  // ---- Estilos fixos do tema (criados uma vez) -------------------------------
  namespace {
    bool iniciado = false;

    lv_style_t st_tela;           // tela: fundo PRETO, texto PAPEL, f12
    lv_style_t st_pagina;         // página/subtela/janela: PRETO opaco, sem borda, sem raio
    lv_style_t st_obj;            // contêiner sem papel visual: transparente
    lv_style_t st_rolagem;        // barra de rolagem lisa

    lv_style_t st_btn;            // botão normal
    lv_style_t st_btn_apertado;
    lv_style_t st_btn_marcado;    // limão + texto preto
    lv_style_t st_btn_marcado_apertado;
    lv_style_t st_desabilitado;   // texto grafite

    lv_style_t st_btnm;           // btnmatrix: fundo transparente, sem vão
    lv_style_t st_btnm_msgbox;    // btnmatrix de caixa de mensagem: com vão
    lv_style_t st_btnm_item;
    lv_style_t st_btnm_item_apertado;
    lv_style_t st_btnm_item_marcado;

    lv_style_t st_teclado;
    lv_style_t st_teclado_item;

    lv_style_t st_trilho;         // fundo PRETO_3 raio 4 (barra)
    lv_style_t st_indicador;      // LIMAO raio 4
    lv_style_t st_fundo_3;        // só fundo PRETO_3
    lv_style_t st_fundo_limao;    // só fundo LIMAO
    lv_style_t st_botao_slider;   // bolinha do slider PAPEL sem sombra
    lv_style_t st_chave_bolinha;  // bolinha da chave GRAFITE
    lv_style_t st_chave_bolinha_ligada; // bolinha da chave PRETO
    lv_style_t st_sem_anim;       // anim_time 0
    lv_style_t st_sem_trans;      // transição de tempo 0: troca de cor direta ao apertar/soltar

    // O tema padrão do LVGL põe nos botões uma transição de 80 ms (e 70 ms de
    // espera ao soltar) nestas propriedades. O LVGL junta as transições de
    // todos os estilos por propriedade, então a nossa precisa citar as
    // MESMAS, com tempo 0 e espera 0; como entra depois do tema, ela vence.
    const lv_style_prop_t props_trans[] = {
      LV_STYLE_BG_OPA, LV_STYLE_BG_COLOR,
      LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT,
      LV_STYLE_TRANSLATE_Y, LV_STYLE_TRANSLATE_X,
      LV_STYLE_TRANSFORM_ZOOM, LV_STYLE_TRANSFORM_ANGLE,
      LV_STYLE_COLOR_FILTER_OPA, LV_STYLE_COLOR_FILTER_DSC,
      LV_STYLE_PROP_INV
    };
    lv_style_transition_dsc_t sem_trans;

    lv_style_t st_cb_ind;
    lv_style_t st_cb_ind_marcado;

    lv_style_t st_dropdown;
    lv_style_t st_dropdownlist;
    lv_style_t st_lista_marcado;
    lv_style_t st_lista_apertado;
    lv_style_t st_roller;

    lv_style_t st_textarea;
    lv_style_t st_cursor;
    lv_style_t st_placeholder;

    lv_style_t st_tabela;
    lv_style_t st_tabela_item;

    lv_style_t st_msgbox;
    lv_style_t st_transparente;   // bg_opa TRANSP (véu das caixas de mensagem)

    lv_style_t st_arco;
    lv_style_t st_arco_ind;

    lv_style_t st_grafico;
  }

  void iniciar() {
    if (iniciado) {
      return;
    }
    iniciado = true;

    lv_style_init(&st_tela);
    lv_style_set_bg_color(&st_tela, c(PRETO));
    lv_style_set_bg_opa(&st_tela, LV_OPA_COVER);
    lv_style_set_text_color(&st_tela, c(PAPEL));
    lv_style_set_text_font(&st_tela, f12());

    lv_style_init(&st_pagina);
    lv_style_set_bg_color(&st_pagina, c(PRETO));
    lv_style_set_bg_opa(&st_pagina, LV_OPA_COVER);
    lv_style_set_border_width(&st_pagina, 0);
    lv_style_set_radius(&st_pagina, 0);
    lv_style_set_shadow_width(&st_pagina, 0);
    lv_style_set_text_color(&st_pagina, c(PAPEL));

    lv_style_init(&st_obj);
    lv_style_set_bg_opa(&st_obj, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_obj, 0);
    lv_style_set_border_color(&st_obj, c(PRETO_3));
    lv_style_set_radius(&st_obj, 0);
    lv_style_set_shadow_width(&st_obj, 0);
    lv_style_set_text_color(&st_obj, c(PAPEL));

    lv_style_init(&st_rolagem);
    lv_style_set_bg_color(&st_rolagem, c(PRETO_3));
    lv_style_set_bg_opa(&st_rolagem, LV_OPA_COVER);
    lv_style_set_width(&st_rolagem, 4);

    // botões
    lv_style_init(&st_btn);
    lv_style_set_bg_color(&st_btn, c(PRETO_3));
    lv_style_set_bg_opa(&st_btn, LV_OPA_COVER);
    lv_style_set_text_color(&st_btn, c(PAPEL));
    lv_style_set_radius(&st_btn, RAIO);
    lv_style_set_shadow_width(&st_btn, 0);
    lv_style_set_border_width(&st_btn, 0);

    lv_style_init(&st_btn_apertado);
    lv_style_set_bg_color(&st_btn_apertado, c(APERTADO));
    lv_style_set_color_filter_opa(&st_btn_apertado, LV_OPA_TRANSP);  // sem o escurecer do tema padrão

    lv_style_init(&st_btn_marcado);
    lv_style_set_bg_color(&st_btn_marcado, c(LIMAO));
    lv_style_set_text_color(&st_btn_marcado, c(PRETO));

    lv_style_init(&st_btn_marcado_apertado);
    lv_style_set_bg_color(&st_btn_marcado_apertado, lv_color_darken(c(LIMAO), LV_OPA_20));
    lv_style_set_text_color(&st_btn_marcado_apertado, c(PRETO));

    lv_style_init(&st_desabilitado);
    lv_style_set_text_color(&st_desabilitado, c(GRAFITE));

    // btnmatrix (seletores, botões das caixas de mensagem)
    lv_style_init(&st_btnm);
    lv_style_set_bg_opa(&st_btnm, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_btnm, 0);
    lv_style_set_pad_all(&st_btnm, 0);
    lv_style_set_pad_gap(&st_btnm, 0);
    lv_style_set_shadow_width(&st_btnm, 0);

    lv_style_init(&st_btnm_msgbox);
    lv_style_set_bg_opa(&st_btnm_msgbox, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_btnm_msgbox, 0);
    lv_style_set_pad_all(&st_btnm_msgbox, 0);
    lv_style_set_pad_gap(&st_btnm_msgbox, VAO);

    lv_style_init(&st_btnm_item);
    lv_style_set_bg_color(&st_btnm_item, c(PRETO_3));
    lv_style_set_bg_opa(&st_btnm_item, LV_OPA_COVER);
    lv_style_set_text_color(&st_btnm_item, c(PAPEL));
    lv_style_set_radius(&st_btnm_item, RAIO);
    lv_style_set_shadow_width(&st_btnm_item, 0);
    lv_style_set_border_width(&st_btnm_item, 0);

    lv_style_init(&st_btnm_item_apertado);
    lv_style_set_bg_color(&st_btnm_item_apertado, c(APERTADO));
    lv_style_set_color_filter_opa(&st_btnm_item_apertado, LV_OPA_TRANSP);

    lv_style_init(&st_btnm_item_marcado);
    lv_style_set_bg_color(&st_btnm_item_marcado, c(LIMAO));
    lv_style_set_text_color(&st_btnm_item_marcado, c(PRETO));

    // teclado
    lv_style_init(&st_teclado);
    lv_style_set_bg_color(&st_teclado, c(PRETO_2));
    lv_style_set_bg_opa(&st_teclado, LV_OPA_COVER);
    lv_style_set_pad_all(&st_teclado, 4);
    lv_style_set_pad_gap(&st_teclado, 4);
    lv_style_set_border_width(&st_teclado, 0);

    lv_style_init(&st_teclado_item);
    lv_style_set_bg_color(&st_teclado_item, c(PRETO_3));
    lv_style_set_bg_opa(&st_teclado_item, LV_OPA_COVER);
    lv_style_set_text_color(&st_teclado_item, c(PAPEL));
    lv_style_set_radius(&st_teclado_item, RAIO_MINI);
    lv_style_set_shadow_width(&st_teclado_item, 0);
    lv_style_set_border_width(&st_teclado_item, 0);

    // barra, slider, chave, arco
    lv_style_init(&st_trilho);
    lv_style_set_bg_color(&st_trilho, c(PRETO_3));
    lv_style_set_bg_opa(&st_trilho, LV_OPA_COVER);
    lv_style_set_radius(&st_trilho, RAIO_BARRA);

    lv_style_init(&st_indicador);
    lv_style_set_bg_color(&st_indicador, c(LIMAO));
    lv_style_set_bg_opa(&st_indicador, LV_OPA_COVER);
    lv_style_set_radius(&st_indicador, RAIO_BARRA);

    lv_style_init(&st_fundo_3);
    lv_style_set_bg_color(&st_fundo_3, c(PRETO_3));

    lv_style_init(&st_fundo_limao);
    lv_style_set_bg_color(&st_fundo_limao, c(LIMAO));

    lv_style_init(&st_botao_slider);
    lv_style_set_bg_color(&st_botao_slider, c(PAPEL));
    lv_style_set_bg_opa(&st_botao_slider, LV_OPA_COVER);
    lv_style_set_shadow_width(&st_botao_slider, 0);

    lv_style_init(&st_chave_bolinha);
    lv_style_set_bg_color(&st_chave_bolinha, c(GRAFITE));

    lv_style_init(&st_chave_bolinha_ligada);
    lv_style_set_bg_color(&st_chave_bolinha_ligada, c(PRETO));

    lv_style_init(&st_sem_anim);
    lv_style_set_anim_time(&st_sem_anim, 0);

    lv_style_transition_dsc_init(&sem_trans, props_trans, lv_anim_path_linear, 0, 0, NULL);
    lv_style_init(&st_sem_trans);
    lv_style_set_transition(&st_sem_trans, &sem_trans);

    lv_style_init(&st_cb_ind);
    lv_style_set_border_color(&st_cb_ind, c(PRETO_3));
    lv_style_set_bg_color(&st_cb_ind, c(PRETO));

    lv_style_init(&st_cb_ind_marcado);
    lv_style_set_bg_color(&st_cb_ind_marcado, c(LIMAO));
    lv_style_set_border_color(&st_cb_ind_marcado, c(LIMAO));
    lv_style_set_text_color(&st_cb_ind_marcado, c(PRETO));

    // listas
    lv_style_init(&st_dropdown);
    lv_style_set_bg_color(&st_dropdown, c(PRETO_3));
    lv_style_set_bg_opa(&st_dropdown, LV_OPA_COVER);
    lv_style_set_text_color(&st_dropdown, c(PAPEL));
    lv_style_set_radius(&st_dropdown, RAIO);
    lv_style_set_border_width(&st_dropdown, 0);
    lv_style_set_shadow_width(&st_dropdown, 0);

    lv_style_init(&st_dropdownlist);
    lv_style_set_bg_color(&st_dropdownlist, c(PRETO_2));
    lv_style_set_bg_opa(&st_dropdownlist, LV_OPA_COVER);
    lv_style_set_border_width(&st_dropdownlist, 1);
    lv_style_set_border_color(&st_dropdownlist, c(PRETO_3));
    lv_style_set_radius(&st_dropdownlist, RAIO);
    lv_style_set_text_color(&st_dropdownlist, c(PAPEL));
    lv_style_set_shadow_width(&st_dropdownlist, 0);
    // cada opção com 44 px de toque: f14 tem 16 px de linha + 28 de espaço.
    // O recuo de cima/baixo é metade do espaço, para a faixa marcada da 1ª e
    // da última opção não ser cortada. Se não couber, a lista rola.
    lv_style_set_text_line_space(&st_dropdownlist, 28);
    lv_style_set_pad_ver(&st_dropdownlist, 14);

    lv_style_init(&st_lista_marcado);
    lv_style_set_bg_color(&st_lista_marcado, c(LIMAO));
    lv_style_set_bg_opa(&st_lista_marcado, LV_OPA_COVER);
    lv_style_set_text_color(&st_lista_marcado, c(PRETO));

    lv_style_init(&st_lista_apertado);
    lv_style_set_bg_color(&st_lista_apertado, c(APERTADO));
    lv_style_set_bg_opa(&st_lista_apertado, LV_OPA_COVER);
    lv_style_set_text_color(&st_lista_apertado, c(PAPEL));
    lv_style_set_color_filter_opa(&st_lista_apertado, LV_OPA_TRANSP);

    lv_style_init(&st_roller);
    lv_style_set_bg_color(&st_roller, c(PRETO_2));
    lv_style_set_bg_opa(&st_roller, LV_OPA_COVER);
    lv_style_set_border_width(&st_roller, 0);
    lv_style_set_text_color(&st_roller, c(PAPEL));

    // campo de texto
    lv_style_init(&st_textarea);
    lv_style_set_bg_color(&st_textarea, c(PRETO));
    lv_style_set_bg_opa(&st_textarea, LV_OPA_COVER);
    lv_style_set_border_width(&st_textarea, 1);
    lv_style_set_border_color(&st_textarea, c(PRETO_3));
    lv_style_set_radius(&st_textarea, RAIO);
    lv_style_set_text_color(&st_textarea, c(PAPEL));
    lv_style_set_shadow_width(&st_textarea, 0);

    lv_style_init(&st_cursor);
    lv_style_set_border_color(&st_cursor, c(LIMAO));
    lv_style_set_anim_time(&st_cursor, 0);  // cursor parado: sem piscar (sem redesenho periódico)

    lv_style_init(&st_placeholder);
    lv_style_set_text_color(&st_placeholder, c(GRAFITE));

    // tabela
    lv_style_init(&st_tabela);
    lv_style_set_bg_color(&st_tabela, c(PRETO_2));
    lv_style_set_bg_opa(&st_tabela, LV_OPA_COVER);
    lv_style_set_border_width(&st_tabela, 0);

    lv_style_init(&st_tabela_item);
    lv_style_set_bg_opa(&st_tabela_item, LV_OPA_TRANSP);
    lv_style_set_border_width(&st_tabela_item, 1);
    lv_style_set_border_color(&st_tabela_item, c(PRETO_3));
    lv_style_set_border_side(&st_tabela_item, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_text_color(&st_tabela_item, c(PAPEL));

    // caixa de mensagem
    lv_style_init(&st_msgbox);
    lv_style_set_bg_color(&st_msgbox, c(PRETO_2));
    lv_style_set_bg_opa(&st_msgbox, LV_OPA_COVER);
    lv_style_set_border_width(&st_msgbox, 0);
    lv_style_set_radius(&st_msgbox, RAIO);
    lv_style_set_text_color(&st_msgbox, c(PAPEL));
    lv_style_set_text_font(&st_msgbox, f14());
    lv_style_set_shadow_width(&st_msgbox, 0);

    lv_style_init(&st_transparente);
    lv_style_set_bg_opa(&st_transparente, LV_OPA_TRANSP);

    lv_style_init(&st_arco);
    lv_style_set_arc_color(&st_arco, c(PRETO_3));

    lv_style_init(&st_arco_ind);
    lv_style_set_arc_color(&st_arco_ind, c(LIMAO));

    lv_style_init(&st_grafico);
    lv_style_set_bg_color(&st_grafico, c(PRETO_2));
    lv_style_set_bg_opa(&st_grafico, LV_OPA_COVER);
    lv_style_set_line_color(&st_grafico, c(PRETO_3));
    lv_style_set_border_width(&st_grafico, 0);
  }

  void aplicar_tema(lv_obj_t *obj) {
    if (!iniciado) {
      iniciar();
    }

    lv_obj_t *pai = lv_obj_get_parent(obj);

    // tela
    if (pai == NULL) {
      lv_obj_add_style(obj, &st_tela, 0);
      lv_obj_add_style(obj, &st_rolagem, LV_PART_SCROLLBAR);
      return;
    }

    lv_obj_add_style(obj, &st_rolagem, LV_PART_SCROLLBAR);

    if (lv_obj_check_type(obj, &lv_obj_class)) {
      if (pai == lv_scr_act() || pai == lv_layer_top()) {
        // página, subtela ou janela: cobre o que está atrás
        lv_obj_add_style(obj, &st_pagina, 0);
      } else {
        lv_obj_add_style(obj, &st_obj, 0);
      }
    } else if (lv_obj_check_type(obj, &lv_btn_class)) {
      lv_obj_add_style(obj, &st_btn, 0);
      lv_obj_add_style(obj, &st_btn_apertado, LV_STATE_PRESSED);
      lv_obj_add_style(obj, &st_btn_marcado, LV_STATE_CHECKED);
      lv_obj_add_style(obj, &st_btn_marcado_apertado, LV_STATE_CHECKED | LV_STATE_PRESSED);
      lv_obj_add_style(obj, &st_desabilitado, LV_STATE_DISABLED);
      // por último: vence as transições do tema padrão (sem animação ao tocar)
      lv_obj_add_style(obj, &st_sem_trans, 0);
      lv_obj_add_style(obj, &st_sem_trans, LV_STATE_PRESSED);
    } else if (lv_obj_check_type(obj, &lv_btnmatrix_class)) {
      if (lv_obj_check_type(pai, &lv_msgbox_class)) {
        lv_obj_add_style(obj, &st_btnm_msgbox, 0);
      } else {
        lv_obj_add_style(obj, &st_btnm, 0);
      }
      lv_obj_add_style(obj, &st_btnm_item, LV_PART_ITEMS);
      lv_obj_add_style(obj, &st_btnm_item_marcado, LV_PART_ITEMS | LV_STATE_CHECKED);
      lv_obj_add_style(obj, &st_btnm_item_apertado, LV_PART_ITEMS | LV_STATE_PRESSED);
      lv_obj_add_style(obj, &st_desabilitado, LV_PART_ITEMS | LV_STATE_DISABLED);
    } else if (lv_obj_check_type(obj, &lv_keyboard_class)) {
      lv_obj_add_style(obj, &st_teclado, 0);
      lv_obj_add_style(obj, &st_teclado_item, LV_PART_ITEMS);
      lv_obj_add_style(obj, &st_btnm_item_marcado, LV_PART_ITEMS | LV_STATE_CHECKED);
      lv_obj_add_style(obj, &st_btnm_item_apertado, LV_PART_ITEMS | LV_STATE_PRESSED);
    } else if (lv_obj_check_type(obj, &lv_bar_class)) {
      lv_obj_add_style(obj, &st_trilho, 0);
      lv_obj_add_style(obj, &st_indicador, LV_PART_INDICATOR);
    } else if (lv_obj_check_type(obj, &lv_slider_class)) {
      lv_obj_add_style(obj, &st_fundo_3, 0);
      lv_obj_add_style(obj, &st_fundo_limao, LV_PART_INDICATOR);
      lv_obj_add_style(obj, &st_botao_slider, LV_PART_KNOB);
    } else if (lv_obj_check_type(obj, &lv_switch_class)) {
      lv_obj_add_style(obj, &st_fundo_3, 0);
      lv_obj_add_style(obj, &st_sem_anim, 0);
      lv_obj_add_style(obj, &st_fundo_limao, LV_PART_INDICATOR | LV_STATE_CHECKED);
      lv_obj_add_style(obj, &st_chave_bolinha, LV_PART_KNOB);
      lv_obj_add_style(obj, &st_chave_bolinha_ligada, LV_PART_KNOB | LV_STATE_CHECKED);
    } else if (lv_obj_check_type(obj, &lv_checkbox_class)) {
      lv_obj_add_style(obj, &st_cb_ind, LV_PART_INDICATOR);
      lv_obj_add_style(obj, &st_cb_ind_marcado, LV_PART_INDICATOR | LV_STATE_CHECKED);
    } else if (lv_obj_check_type(obj, &lv_dropdown_class)) {
      lv_obj_add_style(obj, &st_dropdown, 0);
    } else if (lv_obj_check_type(obj, &lv_dropdownlist_class)) {
      lv_obj_add_style(obj, &st_dropdownlist, 0);
      lv_obj_add_style(obj, &st_lista_marcado, LV_PART_SELECTED | LV_STATE_CHECKED);
      lv_obj_add_style(obj, &st_lista_apertado, LV_PART_SELECTED | LV_STATE_PRESSED);
    } else if (lv_obj_check_type(obj, &lv_roller_class)) {
      lv_obj_add_style(obj, &st_roller, 0);
      lv_obj_add_style(obj, &st_lista_marcado, LV_PART_SELECTED);
    } else if (lv_obj_check_type(obj, &lv_textarea_class)) {
      lv_obj_add_style(obj, &st_textarea, 0);
      lv_obj_add_style(obj, &st_cursor, LV_PART_CURSOR | LV_STATE_FOCUSED);
      lv_obj_add_style(obj, &st_placeholder, LV_PART_TEXTAREA_PLACEHOLDER);
    } else if (lv_obj_check_type(obj, &lv_table_class)) {
      lv_obj_add_style(obj, &st_tabela, 0);
      lv_obj_add_style(obj, &st_tabela_item, LV_PART_ITEMS);
    } else if (lv_obj_check_type(obj, &lv_msgbox_class)) {
      lv_obj_add_style(obj, &st_msgbox, 0);
    } else if (lv_obj_check_type(obj, &lv_msgbox_backdrop_class)) {
      // sem véu: nada é desenhado por cima da tela; continua bloqueando o toque
      lv_obj_add_style(obj, &st_transparente, 0);
    } else if (lv_obj_check_type(obj, &lv_spinner_class) || lv_obj_check_type(obj, &lv_arc_class)) {
      lv_obj_add_style(obj, &st_arco, 0);
      lv_obj_add_style(obj, &st_arco_ind, LV_PART_INDICATOR);
    } else if (lv_obj_check_type(obj, &lv_chart_class)) {
      lv_obj_add_style(obj, &st_grafico, 0);
    }
  }

  // ---- Peças -----------------------------------------------------------------

  void cartao(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, c(PRETO_2), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, RAIO, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  }

  lv_obj_t *criar_cartao(lv_obj_t *pai, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t *obj = lv_obj_create(pai);
    cartao(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    return obj;
  }

  void cartao_tocavel(lv_obj_t *obj) {
    cartao(obj);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    // o tema padrão não põe transição em lv_obj: a troca de cor já é direta
    lv_obj_set_style_bg_color(obj, c(APERTADO), LV_STATE_PRESSED);
  }

  // Ícones (lv_img) e contêineres (lv_obj, que o tema pinta com texto PAPEL)
  // filhos diretos seguem a cor do texto do botão.
  static void pintar_icones_filhos(lv_obj_t *btn, uint32_t cor) {
    uint32_t n = lv_obj_get_child_cnt(btn);
    for (uint32_t i = 0; i < n; i++) {
      lv_obj_t *filho = lv_obj_get_child(btn, i);
      if (lv_obj_check_type(filho, &lv_img_class)) {
        lv_obj_set_style_img_recolor(filho, c(cor), 0);
        lv_obj_set_style_img_recolor_opa(filho, LV_OPA_COVER, 0);
      } else if (lv_obj_check_type(filho, &lv_obj_class)) {
        lv_obj_set_style_text_color(filho, c(cor), 0);
      }
    }
  }

  void estilo_botao(lv_obj_t *btn, Tipo t) {
    lv_obj_set_style_radius(btn, RAIO, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_opa(btn, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, c(GRAFITE), LV_STATE_DISABLED);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    uint32_t fundo = PRETO_3;
    uint32_t apertado = APERTADO;
    uint32_t texto_cor = PAPEL;

    switch (t) {
    case Tipo::NORMAL:
      break;
    case Tipo::DESTAQUE:
      fundo = LIMAO;
      texto_cor = PRETO;
      break;
    case Tipo::PERIGO:
      fundo = ERRO;
      break;
    case Tipo::CONTORNO_PERIGO:
      lv_obj_set_style_border_width(btn, 2, 0);
      lv_obj_set_style_border_color(btn, c(ERRO), 0);
      lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_FULL, 0);
      break;
    case Tipo::FANTASMA:
      lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
      break;
    }

    lv_color_t cor_fundo = c(fundo);
    lv_color_t cor_apertado = (t == Tipo::DESTAQUE || t == Tipo::PERIGO)
      ? lv_color_darken(cor_fundo, LV_OPA_20)
      : c(apertado);

    lv_obj_set_style_bg_color(btn, cor_fundo, 0);
    lv_obj_set_style_bg_color(btn, cor_apertado, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, c(texto_cor), 0);
    lv_obj_set_style_text_color(btn, c(texto_cor), LV_STATE_PRESSED);

    // desabilitado: sempre PRETO_3 com texto GRAFITE (nunca limão com texto
    // cinza) e sem o filtro cinza do tema padrão
    lv_obj_set_style_bg_color(btn, c(PRETO_3), LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(btn, t == Tipo::FANTASMA ? LV_OPA_TRANSP : LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_color_filter_opa(btn, LV_OPA_TRANSP, LV_STATE_DISABLED);

    pintar_icones_filhos(btn, t == Tipo::DESTAQUE ? PRETO : PAPEL);
  }

  lv_obj_t *criar_botao(lv_obj_t *pai, const char *txt, Tipo t, const lv_font_t *f) {
    lv_obj_t *btn = lv_btn_create(pai);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, txt != nullptr ? txt : "");
    lv_obj_set_style_text_font(label, f != nullptr ? f : f14(), 0);
    lv_obj_center(label);
    estilo_botao(btn, t);
    return btn;
  }

  lv_obj_t *criar_rotulo(lv_obj_t *pai, const char *txt, const lv_font_t *f, uint32_t cor) {
    lv_obj_t *label = lv_label_create(pai);
    lv_label_set_text(label, txt != nullptr ? txt : "");
    lv_obj_set_style_text_font(label, f != nullptr ? f : f14(), 0);
    lv_obj_set_style_text_color(label, c(cor), 0);
    return label;
  }

  void estilo_segmentado(lv_obj_t *btnm) {
    lv_obj_set_style_bg_color(btnm, c(PRETO_2), 0);
    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btnm, RAIO, 0);
    lv_obj_set_style_border_width(btnm, 0, 0);
    lv_obj_set_style_pad_all(btnm, 0, 0);
    lv_obj_set_style_pad_gap(btnm, 0, 0);
    lv_obj_set_style_shadow_width(btnm, 0, 0);
    lv_obj_set_style_outline_width(btnm, 0, LV_STATE_FOCUS_KEY);

    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, c(GRAFITE), LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, f14(), LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, RAIO, LV_PART_ITEMS);
    lv_obj_set_style_border_width(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(btnm, 0, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);

    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btnm, c(LIMAO), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btnm, c(PRETO), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_bg_opa(btnm, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btnm, c(APERTADO), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btnm, c(PAPEL), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_opa(btnm, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_PRESSED);
  }

  // Traço limão da aba escolhida, como na maquete: 3 px de altura, do 20% ao
  // 80% da largura da aba, encostado embaixo, cantos arredondados. Só um
  // retângulo liso a mais no desenho da aba marcada.
  static void traco_aba_cb(lv_event_t *e) {
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc == NULL || dsc->class_p != &lv_btnmatrix_class
        || dsc->type != LV_BTNMATRIX_DRAW_PART_BTN || dsc->draw_area == NULL) {
      return;
    }
    lv_obj_t *btnm = lv_event_get_target(e);
    if (!lv_btnmatrix_has_btn_ctrl(btnm, (uint16_t)dsc->id, LV_BTNMATRIX_CTRL_CHECKED)) {
      return;
    }
    lv_area_t a = *dsc->draw_area;
    lv_coord_t w = lv_area_get_width(&a);
    a.x1 += w / 5;
    a.x2 -= w / 5;
    a.y1 = a.y2 - 2;

    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = c(LIMAO);
    rd.bg_opa = LV_OPA_COVER;
    rd.radius = 3;
    rd.border_width = 0;
    rd.shadow_width = 0;
    lv_draw_rect(dsc->draw_ctx, &rd, &a);
  }

  lv_obj_t *criar_barra_abas(lv_obj_t *pai, const char *mapa[], lv_coord_t x, lv_coord_t largura) {
    lv_obj_t *linha = lv_obj_create(pai);
    lv_obj_set_pos(linha, 0, ABA_A);
    lv_obj_set_size(linha, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(linha, c(PRETO_3), 0);
    lv_obj_set_style_bg_opa(linha, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(linha, 0, 0);
    lv_obj_set_style_radius(linha, 0, 0);
    lv_obj_set_style_pad_all(linha, 0, 0);
    lv_obj_clear_flag(linha, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(linha, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btnm = lv_btnmatrix_create(pai);
    lv_btnmatrix_set_map(btnm, mapa);
    lv_obj_set_pos(btnm, x, 0);
    lv_obj_set_size(btnm, largura, ABA_A);

    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnm, 0, 0);
    lv_obj_set_style_radius(btnm, 0, 0);
    lv_obj_set_style_pad_all(btnm, 0, 0);
    lv_obj_set_style_pad_gap(btnm, 0, 0);
    lv_obj_set_style_outline_width(btnm, 0, LV_STATE_FOCUS_KEY);

    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm, c(GRAFITE), LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm, f14(), LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(btnm, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(btnm, 0, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);

    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btnm, c(PAPEL), LV_PART_ITEMS | LV_STATE_CHECKED);
    // o traço limão da aba escolhida é desenhado em traco_aba_cb (60% do meio)
    lv_obj_add_event_cb(btnm, traco_aba_cb, LV_EVENT_DRAW_PART_END, NULL);

    lv_obj_set_style_bg_opa(btnm, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btnm, c(PAPEL), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_color_filter_opa(btnm, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_PRESSED);

    // NO_REPEAT: com o dedo parado na aba, não repete a troca a cada 100 ms
    lv_btnmatrix_set_btn_ctrl_all(btnm, LV_BTNMATRIX_CTRL_CHECKABLE | LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_btnmatrix_set_one_checked(btnm, true);
    lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CHECKED);
    return btnm;
  }

  static void voltar_padrao_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      Navegacao::fechar((lv_obj_t *)lv_event_get_user_data(e));
    }
  }

  Cabecalho criar_cabecalho(lv_obj_t *subtela, const char *titulo, lv_event_cb_t voltar_cb, void *ud) {
    Cabecalho cab;

    cab.voltar = criar_botao(subtela, LV_SYMBOL_LEFT, Tipo::FANTASMA, f20());
    lv_obj_set_pos(cab.voltar, 2, 0);
    lv_obj_set_size(cab.voltar, TOQUE, TOQUE);
    if (voltar_cb != nullptr) {
      lv_obj_add_event_cb(cab.voltar, voltar_cb, LV_EVENT_CLICKED, ud);
    } else {
      lv_obj_add_event_cb(cab.voltar, voltar_padrao_cb, LV_EVENT_CLICKED, subtela);
    }

    cab.titulo = criar_rotulo(subtela, titulo, f20(), PAPEL);
    lv_obj_set_pos(cab.titulo, 50, 10);
    lv_obj_set_width(cab.titulo, 236);
    lv_label_set_long_mode(cab.titulo, LV_LABEL_LONG_DOT);

    cab.direita = criar_rotulo(subtela, "", f12(), GRAFITE);
    lv_obj_set_pos(cab.direita, 256, 16);
    lv_obj_set_width(cab.direita, 160);
    lv_label_set_long_mode(cab.direita, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(cab.direita, LV_TEXT_ALIGN_RIGHT, 0);

    return cab;
  }

  lv_obj_t *criar_chip(lv_obj_t *pai) {
    lv_obj_t *chip = lv_obj_create(pai);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_radius(chip, RAIO, 0);
    lv_obj_set_style_bg_color(chip, c(PRETO_2), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_pad_all(chip, 0, 0);
    lv_obj_set_style_pad_hor(chip, 10, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *ponto = lv_obj_create(chip);
    lv_obj_set_pos(ponto, 0, 8);
    lv_obj_set_size(ponto, 8, 8);
    lv_obj_set_style_radius(ponto, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ponto, c(OK_PONTO), 0);
    lv_obj_set_style_bg_opa(ponto, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ponto, 0, 0);
    lv_obj_set_style_pad_all(ponto, 0, 0);
    lv_obj_clear_flag(ponto, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ponto, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *label = criar_rotulo(chip, "", f12(), PAPEL);
    lv_obj_set_pos(label, 14, 4);
    return chip;
  }

  void chip_estado(lv_obj_t *chip, Estado e, const char *txt) {
    uint32_t fundo = PRETO_2;
    uint32_t ponto = GRAFITE;
    switch (e) {
    case Estado::PRONTA:     ponto = OK_PONTO; break;
    case Estado::IMPRIMINDO: ponto = LIMAO; break;
    case Estado::PAUSADA:    fundo = ALERTA; ponto = PAPEL; break;
    case Estado::ERRO:       fundo = ERRO; ponto = PAPEL; break;
    case Estado::NEUTRO:     break;
    }
    lv_obj_set_style_bg_color(chip, c(fundo), 0);
    lv_obj_t *p = lv_obj_get_child(chip, 0);
    if (p != NULL) {
      lv_obj_set_style_bg_color(p, c(ponto), 0);
    }
    lv_obj_t *label = lv_obj_get_child(chip, 1);
    if (label != NULL) {
      texto(label, txt);
    }
  }

  lv_obj_t *criar_selo(lv_obj_t *pai, lv_coord_t d, uint32_t fundo, uint32_t cor_texto, const char *txt) {
    lv_obj_t *selo = lv_obj_create(pai);
    lv_obj_set_size(selo, d, d);
    lv_obj_set_style_radius(selo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(selo, c(fundo), 0);
    lv_obj_set_style_bg_opa(selo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(selo, 0, 0);
    lv_obj_set_style_pad_all(selo, 0, 0);
    lv_obj_set_style_text_color(selo, c(cor_texto), 0);
    lv_obj_clear_flag(selo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(selo, LV_OBJ_FLAG_CLICKABLE);

    if (txt != nullptr) {
      const lv_font_t *f = d < 20 ? f12() : (d < 40 ? f14() : f20());
      lv_obj_t *label = lv_label_create(selo);
      lv_label_set_text(label, txt);
      lv_obj_set_style_text_font(label, f, 0);
      lv_obj_center(label);
    }
    return selo;
  }

  lv_obj_t *criar_icone(lv_obj_t *pai, const void *img46, lv_coord_t px, uint32_t cor) {
    lv_obj_t *img = lv_img_create(pai);
    lv_img_set_src(img, img46);
    lv_img_set_size_mode(img, LV_IMG_SIZE_MODE_REAL);
    lv_img_set_zoom(img, (uint16_t)((px * 256 + 23) / 46));
    // com suavização: reduzir o desenho de 46 px para 20 a 32 px sem ela deixa
    // os traços serrilhados. Os ícones são fixos (só redesenham quando o
    // botão muda de estado), então o custo na placa é pequeno.
    lv_img_set_antialias(img, true);
    lv_obj_set_style_img_recolor(img, c(cor), 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
  }

  void cor_icone(lv_obj_t *img, uint32_t cor) {
    lv_obj_set_style_img_recolor(img, c(cor), 0);
  }

  void texto(lv_obj_t *label, const char *txt) {
    if (label == NULL || txt == nullptr) {
      return;
    }
    const char *atual = lv_label_get_text(label);
    if (atual == NULL || std::strcmp(atual, txt) != 0) {
      lv_label_set_text(label, txt);
    }
  }
}
