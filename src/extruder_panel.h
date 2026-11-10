#ifndef __EXTRUDER_PANEL_H__
#define __EXTRUDER_PANEL_H__

#include "websocket_client.h"
#include "notify_consumer.h"
#include "spoolman_panel.h"
#include "selector.h"
#include "button_container.h"
#include "numpad.h"
#include "troca_filamento_panel.h"
#include "lvgl/lvgl.h"

#include <string>

// Aba Filamento (quadro 09 da Proposta B) e a subtela "Trocar filamento"
// (quadro 10). A lógica de carga/descarga é a de antes: aquece antes de mexer
// (run_when_hot), carga em pedaços que pode parar, tempos-limite, pergunta do
// Spoolman e macros do config. Só mudou o desenho e quem mostra o "Parar".
class ExtruderPanel : public NotifyConsumer {
 public:
  ExtruderPanel(KWebSocketClient &ws, std::mutex &l, Numpad &np, SpoolmanPanel &sm);
  ~ExtruderPanel();

  // Vai para Controle › Filamento (o painel agora é uma aba).
  void foreground();
  void enable_spoolman();
  void consume(json &j);
  void handle_callback(lv_event_t *e);

  // Highest of the selected temp and the current target, so a manually-set
  // hotter temperature is never lowered by an extrude/load/unload action.
  int effective_temp();

  // PA_AQUECER: só aquecer e esperar (etapa 1 da troca), sem comando depois.
  enum PendingKind { PA_NONE = 0, PA_EXTRUDE, PA_RETRACT, PA_LOAD, PA_UNLOAD, PA_AQUECER };

  static void _handle_callback(lv_event_t *event) {
    ExtruderPanel *panel = (ExtruderPanel*)event->user_data;
    panel->handle_callback(event);
  };

  void handle_filament_btns(lv_event_t *event);
  static void _handle_filament_btns(lv_event_t *event) {
    ExtruderPanel *panel = (ExtruderPanel*)event->user_data;
    panel->handle_filament_btns(event);
  };

 private:
  // Botão de ação da aba. Quando a ação dele está aquecendo ou carregando,
  // ele vira PERIGO "Parar" (o Esfriar, que fazia esse papel, foi para a aba
  // Temperatura). Carregar/Descarregar/Trocar são ButtonContainer (ícone 46);
  // Extrudar/Retrair são lv_btn simples com símbolo no texto.
  struct Acao {
    lv_obj_t *obj = NULL;          // o que recebe o toque
    lv_obj_t *rotulo = NULL;
    const char *texto = "";        // texto normal (fora do "Parar")
    ButtonContainer *bc = NULL;    // nulo nos botões simples
  };

  KWebSocketClient &ws;
  lv_obj_t *panel_cont;
  SpoolmanPanel &spoolman_panel;

  // cartão "Temp. padrão" (408 x 76)
  lv_obj_t *temp_cartao = NULL;
  lv_obj_t *estado_ponto = NULL;
  lv_obj_t *estado_label = NULL;
  lv_obj_t *menos_btn = NULL;
  lv_obj_t *mais_btn = NULL;
  lv_obj_t *temp_valor = NULL;
  // índice em TEMPS {180..300} (o antigo temp_selector virou índice)
  int temp_idx = 3;
  int padrao_lido = -1;            // último /default_extruder_temp aplicado

  ButtonContainer load_btn;
  ButtonContainer unload_btn;
  ButtonContainer troca_btn;
  lv_obj_t *extrude_obj;
  lv_obj_t *retract_obj;
  Selector length_selector;

  Acao acao_carregar;
  Acao acao_descarregar;
  Acao acao_trocar;
  Acao acao_extrudar;
  Acao acao_retrair;
  Acao *botao_ativo = NULL;        // botão da ação em andamento
  Acao *botao_parar = NULL;        // botão que está mostrando "Parar"
  // imprimindo e não pausada (recalculado em refresh_button_state, que roda
  // quando print_stats.state muda e ao abrir a aba)
  bool travado = false;
  uint32_t ponto_mostrado = 1;     // cor da bolinha já desenhada (1 = nenhuma ainda)

  std::string load_filament_macro;
  std::string unload_filament_macro;
  std::string cooldown_macro;

  // Spoolman "Use this filament?" confirm before a Load (mirrors the print
  // dialog). Only shown when Spoolman is configured; if a spool is selected it
  // shows the swatch/name, otherwise it warns there's none. Yes -> load,
  // No -> open the Spoolman panel to pick/fix the spool.
  bool spoolman_enabled = false;
  bool dialogo_da_troca = false;   // a pergunta veio da etapa 3 da troca
  lv_obj_t *fil_cont = NULL;
  lv_obj_t *fil_box = NULL;
  lv_obj_t *fil_content_cont = NULL;
  lv_obj_t *fil_row_cont = NULL;
  lv_obj_t *fil_title_label = NULL;
  lv_obj_t *fil_swatch = NULL;
  lv_obj_t *fil_name_label = NULL;
  lv_obj_t *fil_detail_label = NULL;
  lv_obj_t *fil_yes_btn = NULL;
  lv_obj_t *fil_no_btn = NULL;
  void build_filament_dialog();
  void show_load_filament_dialog();

  // Live extruder telemetry, updated from consume().
  int current_temp = 0;
  int current_target = 0;

  // Pending action waiting for the hotend to heat. Any of extrude/retract/
  // load/unload can be queued: pending_gcode is the exact command to run once
  // we're hot enough. Cleared on fire, Stop, dtor, or heat timeout.
  PendingKind pending_kind = PA_NONE;
  std::string pending_gcode;
  int pending_want = 0;

  // Texto de andamento da ação ("Extrudando...", "Descarregando..."), mostrado
  // na linha do cartão e na frase da troca.
  std::string action_name;

  // True between sending an action gcode and receiving its JSON-RPC response
  // (Moonraker only replies when the script has finished executing).
  bool action_in_flight = false;

  // Auto-cooldown: an extrude/retract/load/unload heats the hotend just for that
  // action, so once it finishes we drop the heater back to 0 instead of leaving
  // it hot (and oozing). Set by run_when_hot() (except during the filament
  // change, which keeps the nozzle hot between steps), cleared when the
  // cooldown is fired or Stop is used (so cooldown never chains).
  bool auto_cool_after = false;

  // Chunked, stoppable filament load. Instead of delegating to the stock
  // LOAD_MATERIAL macro (one uninterruptible G1 E150 move that can't be
  // cancelled once queued), guppyscreen feeds the filament in small bounded
  // chunks it issues itself, so Stop can halt it between chunks (<~2s).
  // load_active is true while chunks are being fed; load_stop is set by
  // Stop to end the load after the in-flight chunk completes.
  bool load_active = false;
  bool load_stop = false;
  int load_remaining_mm = 0;

  // One timer at a time, used for whichever wait we're currently in:
  //   pending_kind != PA_NONE  -> heat timeout (clears pending if temp stalls)
  //   action_in_flight          -> response timeout (fallback unlock if ws dies)
  lv_timer_t *safety_timer = NULL;

  // ---- Troca de filamento (subtela) ----
  // troca_etapa: -1 sem troca; 0 aquecer; 1 descarregar; 2 carregar;
  // 3 purgar; 4 as quatro feitas (Purgar mais / Concluir).
  // troca_passo: a ação em andamento é de uma etapa da troca (ao terminar,
  // troca_resultado() avança a etapa).
  int troca_etapa = -1;
  bool troca_passo = false;
  TrocaFilamentoPanel troca;
  // Troca parada (sem ação andando) por 10 min: encerra e desliga o bico.
  // Um timer só, criado no construtor e pausado enquanto não conta.
  lv_timer_t *troca_espera = NULL;
  bool troca_espera_ativa = false;
  void vigiar_troca();             // liga/pausa a contagem (refresh_button_state)

  bool ocupado() const { return action_in_flight || pending_kind != PA_NONE || load_active; }
  bool travado_imprimindo();
  int temp_escolhida();
  std::string gcode_descarregar();

  void send_action(const std::string &gcode);
  void on_action_response();
  // If auto_cool_after is set, drop the heater to 0 now (used after an
  // extrude/retract/load/unload completes). Clears the flag first so the
  // cooldown action it sends can't re-trigger itself.
  void maybe_auto_cooldown();
  // Heat to effective_temp() if needed, then run gcode; if the hotend is cold,
  // queue it and fire from consume() once we reach temperature.
  void run_when_hot(PendingKind kind, const std::string &name, const std::string &gcode,
                    bool esfriar_depois = true);
  void executar_agora(PendingKind kind, const std::string &gcode);
  void fire_pending();
  void clear_pending();
  // Chunked filament load: feed LOAD_TOTAL_MM in LOAD_CHUNK_MM steps, each its
  // own gcode_script, stopping between chunks if load_stop is set.
  void begin_load();
  void send_load_chunk();
  // natural=true means the feed ran to completion (vs stopped via Stop or a
  // timeout); only then do we auto-cooldown.
  void finish_load(bool natural = false);
  void refresh_button_state();
  void atualizar_estado();
  void arm_safety_timer(uint32_t ms);
  void cancel_safety_timer();

  // "Parar": o mesmo caminho do antigo Esfriar durante uma ação (para a carga
  // entre pedaços ou cancela a espera do aquecimento e esfria o bico).
  void parar();

  // − / + da temperatura padrão
  void mudar_temp(int passo);
  void mostrar_temp();
  void ler_padrao();

  void pintar(Acao &a, bool modo_parar);
  void habilitar(Acao &a, bool ok);

  // troca de filamento
  void troca_iniciar();
  void troca_executar();
  void troca_resultado(bool ok);
  void troca_esquerdo();
  void troca_direito();
  void troca_concluir();
  void atualizar_troca();

#ifdef SIMULATOR
 public:
  // Mostra o estado "aquecendo" sem websocket, para conferir o visual.
  void sim_show_busy();
#endif
};

#endif // __EXTRUDER_PANEL_H__
