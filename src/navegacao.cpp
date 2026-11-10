#include "navegacao.h"
#include "marca.h"
#include "numpad.h"
#include "utils.h"
#include "websocket_client.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

LV_IMG_DECLARE(fine_tune_img);
LV_IMG_DECLARE(calibration_img);
LV_IMG_DECLARE(emergency);

namespace {
  constexpr int N_PAGINAS = 5;
  constexpr int N_ABAS = 8;

  KWebSocketClient *ws_ = nullptr;
  bool criado = false;

  lv_obj_t *coluna = nullptr;
  lv_obj_t *itens[N_PAGINAS] = {};
  lv_obj_t *icones[N_PAGINAS] = {};
  bool icone_imagem[N_PAGINAS] = {};
  lv_obj_t *marcador = nullptr;

  lv_obj_t *paginas[N_PAGINAS] = {};
  lv_obj_t *abas[N_ABAS] = {};
  lv_obj_t *barra_controle = nullptr;
  lv_obj_t *barra_config = nullptr;

  Pagina atual = Pagina::INICIO;
  Aba aba_controle = Aba::MOVER;
  Aba aba_config = Aba::TELA;

  std::vector<lv_obj_t *> subtelas;
  lv_obj_t *acompanhamento = nullptr;

  std::map<int, std::vector<std::function<void()>>> ganchos_pagina;
  std::map<int, std::vector<std::function<void()>>> ganchos_aba;
  std::vector<std::function<void()>> ganchos_acompanhamento;
  std::map<std::string, std::function<void(const std::string &)>> acoes;

  lv_obj_t *janela_parada = nullptr;

  // mapas ESTÁTICOS (o LVGL guarda o ponteiro)
  const char *mapa_controle[] = {"Mover", "Temperatura", "Filamento", "Ventoinhas", ""};
  const char *mapa_config[] = {"Tela", "Rede", "Sistema", "Avançado", ""};

  inline lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

  bool tem_abas(Pagina p) {
    return p == Pagina::CONTROLE || p == Pagina::CONFIGURACOES;
  }

  Pagina pagina_da_aba(Aba a) {
    return (int)a < (int)Aba::TELA ? Pagina::CONTROLE : Pagina::CONFIGURACOES;
  }

  void tirar_estilo_de_painel(lv_obj_t *obj) {
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  }

  void marcar_coluna(Pagina p) {
    for (int i = 0; i < N_PAGINAS; i++) {
      bool ativa = i == (int)p;
      lv_obj_set_style_bg_opa(itens[i], ativa ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
      uint32_t cor = ativa ? Marca::LIMAO : Marca::GRAFITE;
      if (icone_imagem[i]) {
        Marca::cor_icone(icones[i], cor);
      } else {
        lv_obj_set_style_text_color(icones[i], c(cor), 0);
      }
    }
    lv_obj_set_y(marcador, 44 * (int)p + 8);
  }

  void marcar_aba(Aba a) {
    lv_obj_t *barra = pagina_da_aba(a) == Pagina::CONTROLE ? barra_controle : barra_config;
    int base = pagina_da_aba(a) == Pagina::CONTROLE ? (int)Aba::MOVER : (int)Aba::TELA;
    for (int i = base; i < base + 4; i++) {
      if (i == (int)a) {
        lv_obj_clear_flag(abas[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(abas[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    lv_btnmatrix_set_btn_ctrl(barra, (uint16_t)((int)a - base), LV_BTNMATRIX_CTRL_CHECKED);
  }

  Aba aba_lembrada(Pagina p) {
    return p == Pagina::CONTROLE ? aba_controle : aba_config;
  }

  void chamar(std::map<int, std::vector<std::function<void()>>> &m, int chave) {
    auto it = m.find(chave);
    if (it == m.end()) {
      return;
    }
    for (auto &cb : it->second) {
      if (cb) {
        cb();
      }
    }
  }

  void criar();

  void garantir() {
    if (!criado) {
      criar();
    }
  }

  // mostra a página p sem mexer nas subtelas
  void mostrar(Pagina p, bool ganchos) {
    for (int i = 0; i < N_PAGINAS; i++) {
      if (i == (int)p) {
        lv_obj_clear_flag(paginas[i], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(paginas[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    atual = p;
    marcar_coluna(p);
    if (tem_abas(p)) {
      marcar_aba(aba_lembrada(p));
    }

    if (ganchos) {
      chamar(ganchos_pagina, (int)p);
      if (tem_abas(p)) {
        chamar(ganchos_aba, (int)aba_lembrada(p));
      }
    }
  }

  // o teclado numérico mora em lv_layer_top(): ao trocar de página, de aba
  // ou quando o acompanhamento abre sozinho, ele fecha (o número digitado
  // não é enviado)
  void fechar_numpad() {
    Numpad *np = Numpad::compartilhado();
    if (np != nullptr) {
      np->fechar();
    }
  }

  // A lista de um lv_dropdown aberto é filha da TELA (não da página): fecha
  // as que estiverem abertas para não ficarem soltas por cima do
  // acompanhamento. Só esconde a lista (não muda a escolha).
  void fechar_listas() {
    lv_obj_t *scr = lv_scr_act();
    uint32_t n = lv_obj_get_child_cnt(scr);
    for (uint32_t i = 0; i < n; i++) {
      lv_obj_t *f = lv_obj_get_child(scr, i);
      if (f == NULL || !lv_obj_check_type(f, &lv_dropdownlist_class)
          || lv_obj_has_flag(f, LV_OBJ_FLAG_HIDDEN)) {
        continue;
      }
      lv_obj_t *dd = ((lv_dropdown_list_t *)f)->dropdown;
      if (dd != NULL && lv_dropdown_is_open(dd)) {
        lv_dropdown_close(dd);
      }
    }
  }

  // subtela fechada: atrás das páginas E escondida (assim o que muda nela
  // não manda redesenhar a página que está na frente)
  void esconder_subtela(lv_obj_t *s) {
    lv_obj_move_background(s);
    lv_obj_add_flag(s, LV_OBJ_FLAG_HIDDEN);
  }

  void mostrar_subtela(lv_obj_t *s) {
    lv_obj_clear_flag(s, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s);
  }

  void coluna_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
      return;
    }
    fechar_numpad();
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    Navegacao::ir_para((Pagina)i);
  }

  void parada_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
      Navegacao::confirmar_parada();
    }
  }

  void barra_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
      return;
    }
    lv_obj_t *btnm = lv_event_get_target(e);
    uint16_t sel = lv_btnmatrix_get_selected_btn(btnm);
    if (sel == LV_BTNMATRIX_BTN_NONE || sel > 3) {
      return;
    }
    fechar_numpad();
    int base = (int)(intptr_t)lv_event_get_user_data(e);
    Navegacao::ir_para((Aba)(base + sel));
  }

  lv_obj_t *criar_pagina() {
    lv_obj_t *p = lv_obj_create(lv_scr_act());
    tirar_estilo_de_painel(p);
    lv_obj_set_style_bg_color(p, c(Marca::PRETO), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_align(p, LV_ALIGN_TOP_LEFT, Marca::NAV_L, 0);
    lv_obj_set_size(p, Marca::CONT_L, Marca::TELA_A);
    lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    return p;
  }

  lv_obj_t *criar_aba(lv_obj_t *pagina) {
    lv_obj_t *a = lv_obj_create(pagina);
    tirar_estilo_de_painel(a);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(a, 0, Marca::ABA_Y);
    lv_obj_set_size(a, Marca::CONT_L, Marca::ABA_CONT_A);
    lv_obj_add_flag(a, LV_OBJ_FLAG_HIDDEN);
    return a;
  }

  void criar() {
    criado = true;
    Marca::iniciar();

    // ---- coluna ----
    coluna = lv_obj_create(lv_scr_act());
    tirar_estilo_de_painel(coluna);
    lv_obj_set_style_bg_color(coluna, c(Marca::PRETO_2), 0);
    lv_obj_set_style_bg_opa(coluna, LV_OPA_COVER, 0);
    lv_obj_align(coluna, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_size(coluna, Marca::NAV_L, Marca::TELA_A);
    lv_obj_clear_flag(coluna, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < N_PAGINAS; i++) {
      lv_obj_t *item = lv_obj_create(coluna);
      tirar_estilo_de_painel(item);
      lv_obj_set_pos(item, 0, 44 * i);
      lv_obj_set_size(item, Marca::NAV_L, 44);
      lv_obj_set_style_bg_color(item, c(Marca::PRETO_3), 0);
      lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
      lv_obj_set_style_bg_color(item, c(Marca::APERTADO), LV_STATE_PRESSED);
      lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_STATE_PRESSED);
      lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(item, coluna_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
      itens[i] = item;

      lv_obj_t *icone = nullptr;
      switch ((Pagina)i) {
      case Pagina::CONTROLE:
        icone = Marca::criar_icone(item, &fine_tune_img, 24, Marca::GRAFITE);
        icone_imagem[i] = true;
        break;
      case Pagina::CALIBRACAO:
        icone = Marca::criar_icone(item, &calibration_img, 24, Marca::GRAFITE);
        icone_imagem[i] = true;
        break;
      case Pagina::INICIO:
        icone = Marca::criar_rotulo(item, LV_SYMBOL_HOME, Marca::f20(), Marca::GRAFITE);
        break;
      case Pagina::ARQUIVOS:
        icone = Marca::criar_rotulo(item, LV_SYMBOL_DIRECTORY, Marca::f20(), Marca::GRAFITE);
        break;
      case Pagina::CONFIGURACOES:
        icone = Marca::criar_rotulo(item, LV_SYMBOL_SETTINGS, Marca::f20(), Marca::GRAFITE);
        break;
      }
      lv_obj_center(icone);
      icones[i] = icone;
    }

    marcador = lv_obj_create(coluna);
    tirar_estilo_de_painel(marcador);
    lv_obj_set_pos(marcador, 0, 8);
    lv_obj_set_size(marcador, 3, 28);
    lv_obj_set_style_radius(marcador, 3, 0);
    lv_obj_set_style_bg_color(marcador, c(Marca::LIMAO), 0);
    lv_obj_set_style_bg_opa(marcador, LV_OPA_COVER, 0);
    lv_obj_clear_flag(marcador, LV_OBJ_FLAG_CLICKABLE);

    // botão vermelho: sempre abre a confirmação
    lv_obj_t *parar = lv_btn_create(coluna);
    lv_obj_set_style_pad_all(parar, 0, 0);
    lv_obj_set_pos(parar, 6, 222);
    lv_obj_set_size(parar, 44, 44);
    lv_obj_t *icone_parar = Marca::criar_icone(parar, &emergency, 24, Marca::PAPEL);
    lv_obj_center(icone_parar);
    Marca::estilo_botao(parar, Marca::Tipo::PERIGO);
    lv_obj_add_event_cb(parar, parada_cb, LV_EVENT_CLICKED, NULL);

    // ---- páginas ----
    for (int i = 0; i < N_PAGINAS; i++) {
      paginas[i] = criar_pagina();
    }

    lv_obj_t *controle = paginas[(int)Pagina::CONTROLE];
    barra_controle = Marca::criar_barra_abas(controle, mapa_controle, 8, 408);
    lv_obj_add_event_cb(barra_controle, barra_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)Aba::MOVER);

    lv_obj_t *config = paginas[(int)Pagina::CONFIGURACOES];
    barra_config = Marca::criar_barra_abas(config, mapa_config, 8, 408);
    lv_obj_add_event_cb(barra_config, barra_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)Aba::TELA);

    for (int i = 0; i < N_ABAS; i++) {
      abas[i] = criar_aba(i < (int)Aba::TELA ? controle : config);
    }

    mostrar(Pagina::INICIO, false);
  }

  bool registrada(lv_obj_t *cont) {
    return std::find(subtelas.begin(), subtelas.end(), cont) != subtelas.end();
  }

  // está aberta? (as subtelas fechadas ficam atrás da coluna e escondidas)
  bool na_frente(lv_obj_t *sub) {
    return !lv_obj_has_flag(sub, LV_OBJ_FLAG_HIDDEN);
  }

  void esconder_parada_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && janela_parada != nullptr) {
      lv_obj_add_flag(janela_parada, LV_OBJ_FLAG_HIDDEN);
    }
  }

  void parar_tudo_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
      return;
    }
    spdlog::debug("emergency stop pressed");
    if (ws_ != nullptr) {
      ws_->send_jsonrpc("printer.emergency_stop");
    }
    if (janela_parada != nullptr) {
      lv_obj_add_flag(janela_parada, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void Navegacao::definir_ws(KWebSocketClient *ws) {
  ws_ = ws;
}

lv_obj_t *Navegacao::pagina(Pagina p) {
  garantir();
  return paginas[(int)p];
}

lv_obj_t *Navegacao::aba(Aba a) {
  garantir();
  return abas[(int)a];
}

void Navegacao::colocar(lv_obj_t *obj, Pagina p) {
  lv_obj_set_parent(obj, pagina(p));
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
}

void Navegacao::colocar(lv_obj_t *obj, Aba a) {
  lv_obj_set_parent(obj, aba(a));
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
}

void Navegacao::subtela(lv_obj_t *cont) {
  garantir();
  if (cont == nullptr) {
    return;
  }
  if (lv_obj_get_parent(cont) != lv_scr_act()) {
    lv_obj_set_parent(cont, lv_scr_act());
  }
  tirar_estilo_de_painel(cont);
  lv_obj_set_style_bg_color(cont, c(Marca::PRETO), 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
  lv_obj_align(cont, LV_ALIGN_TOP_LEFT, Marca::NAV_L, 0);
  lv_obj_set_size(cont, Marca::CONT_L, Marca::TELA_A);
  if (!registrada(cont)) {
    subtelas.push_back(cont);
  }
  esconder_subtela(cont);
}

void Navegacao::abrir(lv_obj_t *sub) {
  garantir();
  if (sub != nullptr) {
    mostrar_subtela(sub);
  }
}

void Navegacao::fechar(lv_obj_t *sub) {
  garantir();
  if (sub != nullptr) {
    esconder_subtela(sub);
  }
}

void Navegacao::fechar_subtelas() {
  garantir();
  for (lv_obj_t *s : subtelas) {
    if (na_frente(s)) {
      esconder_subtela(s);
    }
  }
}

void Navegacao::ir_para(Pagina p) {
  garantir();
  fechar_subtelas();
  mostrar(p, true);
  if (p == Pagina::INICIO && acompanhamento != nullptr && KUtils::is_printing()) {
    mostrar_subtela(acompanhamento);
  }
}

void Navegacao::ir_para(Aba a) {
  garantir();
  if (pagina_da_aba(a) == Pagina::CONTROLE) {
    aba_controle = a;
  } else {
    aba_config = a;
  }
  fechar_subtelas();
  mostrar(pagina_da_aba(a), true);
}

Pagina Navegacao::pagina_atual() {
  return atual;
}

void Navegacao::ao_mostrar(Pagina p, std::function<void()> cb) {
  ganchos_pagina[(int)p].push_back(cb);
}

void Navegacao::ao_mostrar(Aba a, std::function<void()> cb) {
  ganchos_aba[(int)a].push_back(cb);
}

void Navegacao::ao_abrir_acompanhamento(std::function<void()> cb) {
  ganchos_acompanhamento.push_back(cb);
}

void Navegacao::registrar_acompanhamento(lv_obj_t *sub) {
  if (sub == nullptr) {
    return;
  }
  if (!registrada(sub)) {
    subtela(sub);
  }
  acompanhamento = sub;
}

void Navegacao::abrir_acompanhamento() {
  garantir();
  if (acompanhamento == nullptr) {
    spdlog::warn("abrir_acompanhamento: acompanhamento ainda não registrado");
    return;
  }
  fechar_numpad();
  fechar_listas();
  for (auto &cb : ganchos_acompanhamento) {
    if (cb) {
      cb();
    }
  }
  fechar_subtelas();
  mostrar(Pagina::INICIO, false);
  mostrar_subtela(acompanhamento);
  lv_disp_trig_activity(NULL);
}

void Navegacao::registrar_acao(const std::string &nome, std::function<void(const std::string &)> cb) {
  acoes[nome] = cb;
}

bool Navegacao::executar(const std::string &nome, const std::string &arg) {
  auto it = acoes.find(nome);
  if (it == acoes.end() || !it->second) {
    spdlog::debug("navegacao: ação '{}' não registrada", nome);
    return false;
  }
  it->second(arg);
  return true;
}

void Navegacao::confirmar_parada() {
  if (janela_parada != nullptr) {
    lv_obj_clear_flag(janela_parada, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(janela_parada);
    return;
  }

  Marca::iniciar();

  janela_parada = lv_obj_create(lv_layer_top());
  tirar_estilo_de_painel(janela_parada);
  lv_obj_set_style_bg_color(janela_parada, c(Marca::PRETO), 0);
  lv_obj_set_style_bg_opa(janela_parada, LV_OPA_COVER, 0);
  lv_obj_align(janela_parada, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_size(janela_parada, Marca::TELA_L, Marca::TELA_A);
  lv_obj_add_flag(janela_parada, LV_OBJ_FLAG_CLICKABLE);  // bloqueia o resto

  lv_obj_t *selo = Marca::criar_selo(janela_parada, 56, Marca::ERRO, Marca::PAPEL, nullptr);
  lv_obj_set_pos(selo, 212, 24);
  lv_obj_t *icone = Marca::criar_icone(selo, &emergency, 32, Marca::PAPEL);
  lv_obj_center(icone);

  lv_obj_t *titulo = Marca::criar_rotulo(janela_parada, "Fazer parada de emergência?", Marca::f20(), Marca::PAPEL);
  lv_obj_set_pos(titulo, 20, 92);
  lv_obj_set_width(titulo, 440);
  lv_obj_set_style_text_align(titulo, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *corpo = Marca::criar_rotulo(janela_parada,
    "Desliga na hora o aquecimento e os motores.\nA impressão é perdida e o firmware\nprecisa ser reiniciado.",
    Marca::f14(), Marca::GRAFITE);
  lv_obj_set_pos(corpo, 48, 126);
  lv_obj_set_width(corpo, 384);
  lv_obj_set_style_text_align(corpo, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *cancelar = Marca::criar_botao(janela_parada, "Cancelar", Marca::Tipo::NORMAL, Marca::f16());
  lv_obj_set_pos(cancelar, 40, 196);
  lv_obj_set_size(cancelar, 192, 56);
  lv_obj_add_event_cb(cancelar, esconder_parada_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *parar = lv_btn_create(janela_parada);
  lv_obj_set_style_pad_all(parar, 0, 0);
  lv_obj_set_pos(parar, 248, 196);
  lv_obj_set_size(parar, 192, 56);
  lv_obj_set_flex_flow(parar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(parar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(parar, 6, 0);
  Marca::criar_icone(parar, &emergency, 24, Marca::PAPEL);
  lv_obj_t *parar_txt = lv_label_create(parar);
  lv_label_set_text(parar_txt, "Parar tudo");
  lv_obj_set_style_text_font(parar_txt, Marca::f16(), 0);
  Marca::estilo_botao(parar, Marca::Tipo::PERIGO);
  lv_obj_add_event_cb(parar, parar_tudo_cb, LV_EVENT_CLICKED, NULL);
}
