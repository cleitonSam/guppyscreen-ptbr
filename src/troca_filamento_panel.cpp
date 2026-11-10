#include "troca_filamento_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "spdlog/spdlog.h"

namespace {
  constexpr lv_coord_t LINHA_A = 34;   // altura de cada linha de etapa no cartão

  // texto da etapa 1 é montado com a temperatura (mostrar())
  const char *TEXTOS[4] = {
    "",
    "Descarregar o filamento antigo",
    "Carregar o filamento novo",
    "Purgar até sair limpo",
  };
  const char *NUMEROS[4] = { "1", "2", "3", "4" };
}

TrocaFilamentoPanel::TrocaFilamentoPanel(std::function<void()> esq, std::function<void()> dir)
  : ao_esquerdo(esq)
  , ao_direito(dir)
  , cont(lv_obj_create(lv_scr_act()))
{
  // subtela 424 x 272 (pos 56,0), PRETO opaco, pad 0, fica atrás até abrir()
  Navegacao::subtela(cont);

  // cabeçalho do quadro 10: título à esquerda, "Etapa N de 4" à direita
  lv_obj_t *titulo = Marca::criar_rotulo(cont, "Trocar filamento", Marca::f20(), Marca::PAPEL);
  lv_obj_set_pos(titulo, 8, 10);
  lv_obj_set_width(titulo, 260);
  lv_label_set_long_mode(titulo, LV_LABEL_LONG_DOT);

  etapa_label = Marca::criar_rotulo(cont, "", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(etapa_label, 276, 16);
  lv_obj_set_width(etapa_label, 140);
  lv_label_set_long_mode(etapa_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(etapa_label, LV_TEXT_ALIGN_RIGHT, 0);

  // cartão 408 x 136 com 4 linhas de 34
  cartao = Marca::criar_cartao(cont, 8, 52, 408, 136);

  // faixa da etapa de agora (criada antes dos selos/textos: fica por baixo)
  linha_atual = lv_obj_create(cartao);
  lv_obj_set_size(linha_atual, 408, LINHA_A);
  lv_obj_set_pos(linha_atual, 0, 0);
  lv_obj_set_style_bg_color(linha_atual, lv_color_hex(Marca::PRETO_3), 0);
  lv_obj_set_style_bg_opa(linha_atual, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(linha_atual, 0, 0);
  lv_obj_set_style_radius(linha_atual, 0, 0);
  lv_obj_set_style_pad_all(linha_atual, 0, 0);
  lv_obj_clear_flag(linha_atual, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(linha_atual, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < 4; i++) {
    selos[i] = Marca::criar_selo(cartao, 24, Marca::PRETO_3, Marca::PAPEL, NUMEROS[i]);
    lv_obj_set_pos(selos[i], 12, 5 + LINHA_A * i);
    lv_obj_t *num = lv_obj_get_child(selos[i], 0);
    if (num != NULL) {
      lv_obj_set_style_text_font(num, Marca::f12(), 0);
    }

    textos[i] = Marca::criar_rotulo(cartao, TEXTOS[i], Marca::f14(), Marca::GRAFITE);
    lv_obj_set_pos(textos[i], 46, 8 + LINHA_A * i);
    lv_obj_set_width(textos[i], 350);
    lv_label_set_long_mode(textos[i], LV_LABEL_LONG_DOT);
  }

  // frase do que fazer agora
  frase_label = Marca::criar_rotulo(cont, "", Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(frase_label, 8, 196);
  lv_obj_set_width(frase_label, 408);
  lv_label_set_long_mode(frase_label, LV_LABEL_LONG_DOT);

  // Cancelar (NORMAL 136 x 44) e Continuar (DESTAQUE 264 x 44)
  btn_esquerdo = Marca::criar_botao(cont, "Cancelar", Marca::Tipo::NORMAL);
  lv_obj_set_pos(btn_esquerdo, 8, 220);
  lv_obj_set_size(btn_esquerdo, 136, 44);
  lv_obj_add_event_cb(btn_esquerdo, &TrocaFilamentoPanel::_botao_cb, LV_EVENT_CLICKED, this);

  btn_direito = Marca::criar_botao(cont, LV_SYMBOL_RIGHT " Continuar", Marca::Tipo::DESTAQUE);
  lv_obj_set_pos(btn_direito, 152, 220);
  lv_obj_set_size(btn_direito, 264, 44);
  lv_obj_add_event_cb(btn_direito, &TrocaFilamentoPanel::_botao_cb, LV_EVENT_CLICKED, this);
  direito_tipo = 1;

  Vista inicial;
  mostrar(inicial);
}

TrocaFilamentoPanel::~TrocaFilamentoPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void TrocaFilamentoPanel::abrir() {
  Navegacao::abrir(cont);
}

void TrocaFilamentoPanel::fechar() {
  Navegacao::fechar(cont);
}

void TrocaFilamentoPanel::_botao_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  auto *self = (TrocaFilamentoPanel *)lv_event_get_user_data(e);
  lv_obj_t *alvo = lv_event_get_current_target(e);
  if (alvo == self->btn_esquerdo) {
    if (self->ao_esquerdo) self->ao_esquerdo();
  } else if (alvo == self->btn_direito) {
    if (self->ao_direito) self->ao_direito();
  }
}

static void habilitar(lv_obj_t *obj, bool ok) {
  if (ok) {
    lv_obj_clear_state(obj, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(obj, LV_STATE_DISABLED);
  }
}

void TrocaFilamentoPanel::mostrar(const Vista &v) {
  int atual = v.etapa < 0 ? 0 : (v.etapa > 4 ? 4 : v.etapa);

  if (atual != etapa_mostrada) {
    for (int i = 0; i < 4; i++) {
      lv_obj_t *num = lv_obj_get_child(selos[i], 0);
      uint32_t fundo, cor, cor_texto;
      const char *marca;
      if (i < atual) {            // feita: OK + ✓
        fundo = Marca::OK;
        cor = Marca::PAPEL;
        cor_texto = Marca::PAPEL;
        marca = LV_SYMBOL_OK;
      } else if (i == atual) {    // de agora: limão com número PRETO
        fundo = Marca::LIMAO;
        cor = Marca::PRETO;
        cor_texto = Marca::PAPEL;
        marca = NUMEROS[i];
      } else {                    // próxima: PRETO_3, texto grafite
        fundo = Marca::PRETO_3;
        cor = Marca::PAPEL;
        cor_texto = Marca::GRAFITE;
        marca = NUMEROS[i];
      }
      lv_obj_set_style_bg_color(selos[i], lv_color_hex(fundo), 0);
      lv_obj_set_style_text_color(selos[i], lv_color_hex(cor), 0);
      if (num != NULL) {
        Marca::texto(num, marca);
        lv_obj_center(num);
      }
      lv_obj_set_style_text_color(textos[i], lv_color_hex(cor_texto), 0);
    }

    if (atual <= 3) {
      lv_obj_clear_flag(linha_atual, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_y(linha_atual, LINHA_A * atual);
      // nas pontas acompanha o canto do cartão; no meio, faixa reta
      lv_obj_set_style_radius(linha_atual, (atual == 0 || atual == 3) ? Marca::RAIO : 0, 0);
    } else {
      lv_obj_add_flag(linha_atual, LV_OBJ_FLAG_HIDDEN);
    }

    std::string etapa_txt = fmt::format("Etapa {} de 4", atual < 4 ? atual + 1 : 4);
    Marca::texto(etapa_label, etapa_txt.c_str());
    etapa_mostrada = atual;
  }

  if (v.temp != temp_mostrada) {
    std::string t = fmt::format("Aquecer o bico até {}°C", v.temp);
    Marca::texto(textos[0], t.c_str());
    temp_mostrada = v.temp;
  }

  Marca::texto(frase_label, v.frase.c_str());

  Marca::texto(lv_obj_get_child(btn_esquerdo, 0), v.esquerdo);
  habilitar(btn_esquerdo, v.esquerdo_ok);

  Marca::texto(lv_obj_get_child(btn_direito, 0), v.direito);
  // desabilitado: NORMAL (PRETO_3 com texto grafite), para não ficar limão apagado
  int tipo = v.direito_ok ? 1 : 0;
  if (tipo != direito_tipo) {
    Marca::estilo_botao(btn_direito, v.direito_ok ? Marca::Tipo::DESTAQUE : Marca::Tipo::NORMAL);
    direito_tipo = tipo;
  }
  habilitar(btn_direito, v.direito_ok);
}
