#ifndef __AVISOS_PT_H__
#define __AVISOS_PT_H__

#include <string>

// Transforma as mensagens de erro do Klipper (inclusive o formato da Creality,
// {"code":"keyNN","msg":"..."}) em um texto curto em português e diz quanto
// o aviso é sério.
namespace AvisosPt {

enum Nivel {
  IGNORAR,   // não mostra nada (ex.: "impressora ainda não está pronta")
  LEVE,      // some sozinho depois de alguns segundos
  GRAVE      // fica na tela até tocar
};

struct Aviso {
  std::string texto;
  Nivel nivel;
  bool pede_home;  // "Must home axis first" -> abre a janela de voltar ao início
};

// linha = texto depois do "!! " (ou o state_message do Klipper parado)
Aviso traduzir(const std::string &linha);

// só tira o embrulho {"code","msg"} e as linhas de instrução em inglês
std::string limpar(const std::string &linha);

}  // namespace AvisosPt

#endif  // __AVISOS_PT_H__
