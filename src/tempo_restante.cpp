#include "tempo_restante.h"

#include <algorithm>
#include <cmath>

namespace {
  // 0,13 vira 13 (e não 12 pelo erro de arredondamento do 0,13 x 100)
  int pct_de(double fracao) {
    if (fracao < 0.0) {
      return -1;
    }
    int p = static_cast<int>(std::floor(std::min(1.0, fracao) * 100.0 + 1e-6));
    return std::max(0, std::min(100, p));
  }
}

TempoSaida calcular_tempo(const TempoEntrada &e) {
  TempoSaida s;
  double falta = -1.0;

  if (e.macro_valida && e.macro_falta >= 0.0) {
    // contagem regressiva desde a última conta da macro (ela refaz a cada 1 a
    // 4 s); o tempo de peça para na pausa, então a contagem também para
    double andou = std::max(0.0, e.impresso - e.macro_pd_falta);
    falta = std::max(0.0, e.macro_falta - andou);
    s.fonte = FonteTempo::MACRO;
  } else if (e.estimado_fatiador > 0.0) {
    falta = std::max(0.0, e.estimado_fatiador - std::max(0.0, e.impresso));
    s.fonte = FonteTempo::FATIADOR;
  } else if (e.impresso > 0.0 && e.progresso_arquivo >= 0.05 && e.progresso_arquivo < 1.0) {
    falta = e.impresso / e.progresso_arquivo - e.impresso;
    s.fonte = FonteTempo::ARQUIVO;
  }

  if (falta >= 0.0) {
    s.falta = static_cast<int64_t>(std::llround(falta));
    s.total = static_cast<int64_t>(std::llround(std::max(0.0, e.impresso) + falta));
  }

  // porcentagem: a do Orca (tempo, peça inteira) quando a macro tem; senão a do
  // Klipper (M73 do arquivo, ou a posição no arquivo quando não há M73)
  if (e.macro_valida && e.macro_p_orca >= 0) {
    s.pct = std::min(100, e.macro_p_orca);
  } else if (e.progresso_m73 >= 0.0) {
    s.pct = pct_de(e.progresso_m73);
  } else if (e.progresso_arquivo >= 0.0) {
    s.pct = pct_de(e.progresso_arquivo);
  }
  return s;
}
