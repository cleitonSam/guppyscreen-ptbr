#ifndef __TEMPO_RESTANTE_H__
#define __TEMPO_RESTANTE_H__

#include <cstdint>

// Tempo que falta da impressão (14/11/2026).
//
// De onde vem, nesta ordem:
//  1. Macro M73 do melhorias.cfg (objeto "gcode_macro _TEMPO"): guarda o R
//     das linhas "M73 P R" do Orca (o Klipper joga o R fora) e mede, na própria
//     impressão, quanto o tempo real difere do tempo do Orca. É o mesmo número
//     do "Estimado" do Mainsail. Vale também na continuação (RETOMAR_...) e
//     para na pausa.
//  2. Sem a macro: o tempo do fatiador (metadado estimated_time) menos o tempo
//     de peça. Na KE o Orca errou menos de 1% (medido em 14/11/2026).
//  3. Sem nada disso: extrapola pela posição no arquivo, só depois de 5%.
// A média de arquivo + filamento + fatiador que havia antes errava horas no
// começo (16h09 em vez de 8h34 na 02_carcaca) e saiu.
//
// Sem LVGL e sem json: dá para testar sozinho (tempo/tela/teste).

struct TempoEntrada {
  // print_stats
  double impresso = 0.0;           // print_duration (s): peça, sem preparo nem pausa ("Decorrido")
  // gcode_macro _TEMPO (só vale se for desta impressão: mesmo tamanho de arquivo)
  bool macro_valida = false;
  double macro_falta = -1.0;       // s, calculado quando o tempo de peça era macro_pd_falta
  double macro_pd_falta = 0.0;
  int macro_p_orca = -1;           // % do Orca (da peça inteira, também na continuação)
  // reservas
  double estimado_fatiador = 0.0;  // metadado estimated_time (s); 0 = não tem
  double progresso_arquivo = -1.0; // posição no G-code, 0..1; -1 = não sabe
  double progresso_m73 = -1.0;     // display_status.progress, 0..1; -1 = não sabe
};

enum class FonteTempo { NADA = 0, MACRO, FATIADOR, ARQUIVO };

struct TempoSaida {
  int64_t falta = -1;   // s; -1 = não sabe ("--")
  int64_t total = -1;   // s: tempo de peça impresso + falta; -1 = não sabe
  int pct = -1;         // 0..100; -1 = não sabe
  FonteTempo fonte = FonteTempo::NADA;
};

TempoSaida calcular_tempo(const TempoEntrada &e);

#endif // __TEMPO_RESTANTE_H__
