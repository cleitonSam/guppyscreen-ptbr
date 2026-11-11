#ifndef __MARCA_H__
#define __MARCA_H__

// Marca: cores, fontes, medidas e peças visuais comuns da tela (Proposta B,
// tela deitada 480 x 272). Tudo aqui é só aparência: nenhuma função manda
// comando para a impressora.
//
// Regras rápidas:
//  - Limão só como fundo com texto PRETO (use Tipo::DESTAQUE), na barra de
//    progresso, no traço da aba escolhida, no item escolhido do segmentado e no
//    ícone da página ativa da coluna. Nunca texto limão sobre preto.
//  - OK, alerta e erro só como fundo com texto PAPEL. Estado sempre com palavra.
//  - Cores lisas: nada de degradê, sombra ou transparência sobre conteúdo.

#include "lvgl/lvgl.h"

#include <cstdint>

namespace Marca {

  // ---- 1.1 Cores ------------------------------------------------------------
  constexpr uint32_t PRETO    = 0x0a0a0b;  // fundo da tela
  constexpr uint32_t PRETO_2  = 0x1a1a1c;  // cartões, coluna da esquerda, segmentado
  constexpr uint32_t PRETO_3  = 0x26262b;  // botões, bordas, linhas, trilho da barra
  constexpr uint32_t APERTADO = 0x34343a;  // fundo do botão normal enquanto apertado
  constexpr uint32_t PAPEL    = 0xf5f4f0;  // texto principal
  constexpr uint32_t GRAFITE  = 0xa6a4a2;  // texto secundário, ícone inativo
  constexpr uint32_t LIMAO    = 0xd8ff3e;  // destaque: SEMPRE com texto/ícone PRETO por cima
  constexpr uint32_t OK       = 0x2f7d32;  // selo "concluída" (fundo, texto papel)
  constexpr uint32_t OK_PONTO = 0x3f9e43;  // bolinha de 8 px de "Pronta" / "No alvo"
  constexpr uint32_t ALERTA   = 0x953d00;  // chip "Pausada" (fundo, texto papel)
  constexpr uint32_t ERRO     = 0xb3261e;  // parada de emergência, contorno do Cancelar (fundo, texto papel)
  // uso: lv_color_hex(Marca::LIMAO)

  // ---- Preaquecer (Temperatura e Início) -------------------------------------
  // Tabela de filamentos da KE (10/11/2026, docs/filamentos-ke.md do repo
  // impressora-3d): PETG do dono 240-250 / mesa 70; PLA 220 / mesa 55.
  constexpr int PETG_BICO = 245, PETG_MESA = 70;
  constexpr int PLA_BICO  = 220, PLA_MESA  = 55;

  // ---- 1.3 Medidas ----------------------------------------------------------
  constexpr lv_coord_t TELA_L = 480, TELA_A = 272;
  constexpr lv_coord_t NAV_L = 56;            // coluna da esquerda
  constexpr lv_coord_t CONT_L = 424;          // largura de página e de subtela (480 - 56)
  constexpr lv_coord_t ABA_A = 44;            // barra de abas: 0..43, linha de 1 px em y = 44
  constexpr lv_coord_t ABA_Y = 45;            // conteúdo da aba começa em y = 45
  constexpr lv_coord_t ABA_CONT_A = 227;      // altura do conteúdo da aba (272 - 45)
  constexpr lv_coord_t MARGEM = 8, VAO = 8;   // margem lateral e espaço entre cartões
  constexpr lv_coord_t RAIO = 12, RAIO_MINI = 8, RAIO_BARRA = 4;
  constexpr lv_coord_t TOQUE = 44;            // nada tocável menor que 44

  // ---- 1.2 Fontes -----------------------------------------------------------
  // 12: legenda/secundário · 14: texto normal/botões · 16: valor em cartão
  // 20: título/temperatura grande · 40: SÓ a porcentagem (sem acento: só dígitos e %).
  const lv_font_t *f12();
  const lv_font_t *f14();
  const lv_font_t *f16();
  const lv_font_t *f20();
  const lv_font_t *f40();

  // ---- 1.6 Funções de apoio -------------------------------------------------
  enum class Tipo { NORMAL, DESTAQUE, PERIGO, CONTORNO_PERIGO, FANTASMA };
  //  NORMAL: PRETO_3 / texto PAPEL · DESTAQUE: LIMAO / texto PRETO · PERIGO: ERRO / texto PAPEL
  //  CONTORNO_PERIGO: PRETO_3 + borda 2 ERRO · FANTASMA: fundo transparente, texto PAPEL
  enum class Estado { PRONTA, IMPRIMINDO, PAUSADA, ERRO, NEUTRO };

  // Cria os estilos fixos (uma vez só; chamar de novo não faz nada).
  void iniciar();
  // Chamada pelo tema filho para cada objeto criado (GuppyScreen::new_theme_apply_cb).
  void aplicar_tema(lv_obj_t *obj);

  // Cartão: PRETO_2, raio 12, borda 0, pad 0, sem rolagem.
  void cartao(lv_obj_t *obj);
  lv_obj_t *criar_cartao(lv_obj_t *pai, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
  // Cartão que se toca: cartao() + fundo APERTADO enquanto apertado.
  void cartao_tocavel(lv_obj_t *obj);

  // Pinta um lv_btn (ou lv_obj clicável) no tipo pedido. Pode ser chamada de
  // novo para trocar o tipo (ex.: Pausar NORMAL <-> Retomar DESTAQUE).
  // Também pinta os ícones lv_img que são filhos diretos: PRETO no DESTAQUE,
  // PAPEL nos outros tipos.
  void estilo_botao(lv_obj_t *btn, Tipo t);
  // lv_btn + lv_label central (filho 0 do botão), pad 0. Fonte f14 se f for nulo.
  // Tamanho e posição ficam com quem chama.
  lv_obj_t *criar_botao(lv_obj_t *pai, const char *texto, Tipo t, const lv_font_t *f = nullptr);
  // Rótulo com fonte e cor. Largura/modo longo ficam com quem chama.
  lv_obj_t *criar_rotulo(lv_obj_t *pai, const char *texto, const lv_font_t *f, uint32_t cor = PAPEL);

  // Segmentado: trilho PRETO_2 raio 12; itens transparentes texto GRAFITE f14;
  // escolhido LIMAO com texto PRETO.
  void estilo_segmentado(lv_obj_t *btnm);

  // Barra de abas: btnmatrix em (x,0) com largura x 44; itens GRAFITE f14,
  // fundo transparente; escolhido: texto PAPEL + traço de 3 px LIMAO embaixo;
  // checkable + one_checked (começa no item 0). Cria também a linha PRETO_3
  // de 1 px em (0,44) com a largura toda do pai. O MAPA PRECISA SER ESTÁTICO.
  lv_obj_t *criar_barra_abas(lv_obj_t *pai, const char *mapa[], lv_coord_t x, lv_coord_t largura);

  // Cabeçalho de subtela (subtela com pad 0, como Navegacao::subtela deixa):
  //  voltar: fantasma 44x44 em (2,0) com LV_SYMBOL_LEFT;
  //  titulo: f20 em (50,10) largura 236, LONG_DOT;
  //  direita: f12 GRAFITE em (256,16) largura 160, alinhado à direita, vazio.
  // voltar_cb nulo = Navegacao::fechar(subtela). Com voltar_cb, ud vai como user_data.
  struct Cabecalho { lv_obj_t *voltar; lv_obj_t *titulo; lv_obj_t *direita; };
  Cabecalho criar_cabecalho(lv_obj_t *subtela, const char *titulo,
                            lv_event_cb_t voltar_cb = nullptr, void *ud = nullptr);

  // Chip de estado: 24 de altura, raio 12, pad 10, bolinha 8 (filho 0) +
  // texto f12 (filho 1). Largura: conteúdo (quem chama pode fixar, ex.: 112).
  lv_obj_t *criar_chip(lv_obj_t *pai);
  // PRONTA: PRETO_2 + ponto OK_PONTO; IMPRIMINDO: PRETO_2 + ponto LIMAO;
  // PAUSADA: ALERTA + ponto PAPEL; ERRO: ERRO + ponto PAPEL; NEUTRO: PRETO_2 + ponto GRAFITE.
  void chip_estado(lv_obj_t *chip, Estado e, const char *texto);

  // Círculo d x d com texto/símbolo central (filho 0, só se texto não for nulo).
  // Fonte: f12 até 19 px, f14 até 39 px, f20 acima.
  lv_obj_t *criar_selo(lv_obj_t *pai, lv_coord_t d, uint32_t fundo, uint32_t cor_texto, const char *texto);
  // Ícone de imagem de 46 px reduzido para px (zoom px*256/46) e recolorido.
  // O objeto fica com px x px (modo REAL); posicione com lv_obj_set_pos.
  lv_obj_t *criar_icone(lv_obj_t *pai, const void *img46, lv_coord_t px, uint32_t cor);
  // Troca a cor de um ícone criado por criar_icone.
  void cor_icone(lv_obj_t *img, uint32_t cor);

  // Só chama lv_label_set_text se o texto mudou (evita redesenho).
  void texto(lv_obj_t *label, const char *txt);
}

#endif // __MARCA_H__
