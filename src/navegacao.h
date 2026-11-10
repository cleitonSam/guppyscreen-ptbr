#ifndef __NAVEGACAO_H__
#define __NAVEGACAO_H__

// Navegação da tela deitada (Proposta B): coluna da esquerda com 5 páginas,
// barras de abas de Controle e Configurações, subtelas que abrem por cima da
// página, ações com nome (para um painel abrir outro sem depender dele) e a
// confirmação da parada de emergência.
//
// TUDO aqui é chamado com o lv_lock já preso (eventos do LVGL, consume() e
// ganchos). Nada aqui prende o lv_lock de novo, e os ganchos/ações também
// não podem prender (o std::mutex não é recursivo).
//
// Camadas:
//   lv_layer_sys()  protetor de tela
//   lv_layer_top()  janelas (caixas de mensagem, teclado numérico, parada...)
//   lv_scr_act()    coluna (0,0 56x272) + 5 páginas (56,0 424x272) + subtelas

#include "lvgl/lvgl.h"

#include <functional>
#include <string>

class KWebSocketClient;

enum class Pagina { INICIO = 0, CONTROLE, ARQUIVOS, CALIBRACAO, CONFIGURACOES };
enum class Aba { MOVER = 0, TEMPERATURA, FILAMENTO, VENTOINHAS,   // Controle
                 TELA, REDE, SISTEMA, AVANCADO };                 // Configurações

class Navegacao {
 public:
  // GuppyScreen::init chama ANTES de GuppyScreen::get().
  static void definir_ws(KWebSocketClient *ws);

  // Página 424x272 (pad 0, sem rolagem). Cria coluna, páginas e barras de
  // abas na 1ª chamada de qualquer função daqui.
  static lv_obj_t *pagina(Pagina p);
  // Conteúdo da aba: 424x227 em (0,45) dentro da página dona (pad 0, transparente).
  static lv_obj_t *aba(Aba a);

  // lv_obj_set_parent + alinhado em (0,0) + tamanho 100% x 100%.
  static void colocar(lv_obj_t *obj, Pagina p);
  static void colocar(lv_obj_t *obj, Aba a);

  // Transforma um contêiner em subtela: pos (56,0), 424x272, fundo PRETO
  // opaco, pad 0, sem borda/raio, sem rolagem; registra, manda para trás e
  // esconde (LV_OBJ_FLAG_HIDDEN).
  // Chamar de novo com o mesmo contêiner não registra duas vezes.
  static void subtela(lv_obj_t *cont);
  static void abrir(lv_obj_t *subtela);           // tira o HIDDEN + lv_obj_move_foreground
  static void fechar(lv_obj_t *subtela);          // lv_obj_move_background + HIDDEN
  static void fechar_subtelas();                  // fechar() nas registradas que estão abertas

  // Fecha as subtelas, mostra a página, marca a coluna e chama os ganchos
  // ao_mostrar da página (e da aba lembrada, se a página tiver abas).
  // Início + impressão ativa (imprimindo ou pausada) = abre o acompanhamento.
  static void ir_para(Pagina p);
  // Idem, escolhendo a aba (e lembrando dela para a próxima vez).
  static void ir_para(Aba a);
  static Pagina pagina_atual();

  // Ganchos chamados por ir_para (vários por destino, na ordem de registro).
  static void ao_mostrar(Pagina p, std::function<void()> cb);
  static void ao_mostrar(Aba a, std::function<void()> cb);

  // Acompanhamento da impressão (lote Acompanhamento chama no construtor).
  // Já faz subtela(cont) se ainda não foi feito.
  static void registrar_acompanhamento(lv_obj_t *subtela);
  // fecha o teclado numérico + ganchos ao_abrir_acompanhamento +
  // fechar_subtelas + mostra Início (sem ganchos) + acompanhamento na frente
  // + acende a tela (lv_disp_trig_activity).
  static void abrir_acompanhamento();
  // Gancho chamado por abrir_acompanhamento logo depois de fechar o teclado:
  // cada painel fecha as próprias janelas em lv_layer_top() (perguntas do
  // filamento, lista suspensa aberta...) para nada ficar por cima do
  // acompanhamento. Só mexe na tela: não manda comando para a impressora.
  static void ao_abrir_acompanhamento(std::function<void()> cb);

  // Ações com nome: abrir_arquivo, ajuste_fino, altura_do_bico, malha_da_mesa,
  // luz, macros, console, spoolman.
  static void registrar_acao(const std::string &nome, std::function<void(const std::string &)> cb);
  static bool executar(const std::string &nome, const std::string &arg = ""); // false se ninguém registrou

  // Janela de confirmação da parada de emergência (sempre pergunta).
  static void confirmar_parada();
};

#endif // __NAVEGACAO_H__
