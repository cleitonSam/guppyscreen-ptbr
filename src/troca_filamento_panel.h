#ifndef __TROCA_FILAMENTO_PANEL_H__
#define __TROCA_FILAMENTO_PANEL_H__

#include "lvgl/lvgl.h"

#include <functional>
#include <string>

// Subtela "Trocar filamento" (quadro 10 da Proposta B).
//
// Só desenho: quem decide as etapas e manda os comandos é o ExtruderPanel,
// que chama mostrar() quando algo muda e recebe os toques nos dois botões
// pelas funções passadas no construtor. Tudo roda com o lv_lock já preso
// (eventos do LVGL e consume()); nada aqui prende o lock.
class TrocaFilamentoPanel {
 public:
  struct Vista {
    int etapa = 0;                   // 0..3 = etapa de agora; 4 = as 4 feitas
    int temp = 0;                    // °C da etapa 1 ("Aquecer o bico até ...")
    std::string frase;               // frase f14 acima dos botões
    const char *esquerdo = "Cancelar";
    bool esquerdo_ok = true;
    const char *direito = "Continuar";
    bool direito_ok = true;          // desabilitado: vira NORMAL cinza (sem limão apagado)
  };

  TrocaFilamentoPanel(std::function<void()> ao_esquerdo, std::function<void()> ao_direito);
  ~TrocaFilamentoPanel();

  void abrir();
  void fechar();
  void mostrar(const Vista &v);

 private:
  static void _botao_cb(lv_event_t *e);

  std::function<void()> ao_esquerdo;
  std::function<void()> ao_direito;

  lv_obj_t *cont;
  lv_obj_t *etapa_label;
  lv_obj_t *cartao;
  lv_obj_t *linha_atual;
  lv_obj_t *selos[4];
  lv_obj_t *textos[4];
  lv_obj_t *frase_label;
  lv_obj_t *btn_esquerdo;
  lv_obj_t *btn_direito;

  // o que já está desenhado (só redesenha o que mudou)
  int etapa_mostrada = -1;
  int temp_mostrada = -1;
  int direito_tipo = -1;             // 1 = DESTAQUE, 0 = NORMAL
};

#endif // __TROCA_FILAMENTO_PANEL_H__
