#include "prompt_panel.h"
#include "state.h"
#include "utils.h"
#include "marca.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>

// Janela das perguntas do Klipper (action:prompt: M600, backup etc.).
// Proposta B (tela deitada 480x272), no jeito do quadro 10 da maquete:
//   - fundo PRETO liso e opaco na área de conteúdo (56,0 424x272), sem véu;
//     a coluna da esquerda continua à vista (o botão vermelho de parada
//     continua alcançável);
//   - título f16 no alto, em (8,8);
//   - caixa PRETO_2 raio 12 sem borda com o texto f14 e os botões da macro;
//   - fileira de botões de rodapé embaixo, 44 px de altura, em y = 220.
// Mora em lv_layer_top(): o acompanhamento automático e a navegação só mexem
// em lv_scr_act(), então nunca escondem uma pergunta da impressora.
// Mostrar/esconder continua só com LV_OBJ_FLAG_HIDDEN (is_visible igual).

PromptPanel::PromptPanel(KWebSocketClient &websocket_client,
  std::mutex &lock,
  lv_obj_t *parent)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , prompt_cont(lv_obj_create(lv_layer_top()))
  , header(lv_label_create(prompt_cont))
  , body_cont(lv_obj_create(prompt_cont))
  , body(lv_label_create(body_cont))
  , button_cont(lv_obj_create(body_cont))
  , footer_cont(lv_obj_create(prompt_cont))
{
  LV_UNUSED(parent);  // a janela não pertence a página nenhuma (fica no topo)

  // JANELA: fundo liso PRETO na área de conteúdo, bloqueia o toque no que
  // está atrás (lv_obj é clicável por padrão), sem rolagem.
  lv_obj_set_pos(prompt_cont, Marca::NAV_L, 0);
  lv_obj_set_size(prompt_cont, Marca::CONT_L, Marca::TELA_A);
  lv_obj_set_style_bg_color(prompt_cont, lv_color_hex(Marca::PRETO), 0);
  lv_obj_set_style_bg_opa(prompt_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(prompt_cont, 0, 0);
  lv_obj_set_style_radius(prompt_cont, 0, 0);
  lv_obj_set_style_shadow_width(prompt_cont, 0, 0);
  lv_obj_set_style_text_color(prompt_cont, lv_color_hex(Marca::PAPEL), 0);
  lv_obj_set_style_pad_all(prompt_cont, Marca::MARGEM, 0);
  lv_obj_set_style_pad_row(prompt_cont, Marca::VAO, 0);
  lv_obj_set_style_pad_column(prompt_cont, 0, 0);
  lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_SCROLLABLE);

  static lv_coord_t grid_cols[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_rows[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(prompt_cont, grid_cols, grid_rows);

  lv_obj_set_grid_cell(header, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
  lv_obj_set_grid_cell(body_cont, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_set_grid_cell(footer_cont, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_END, 2, 1);

  // TÍTULO: f16, uma linha só; o que não couber vira "..." (nada de texto
  // rolando sozinho). Altura fixa para o LONG_DOT funcionar.
  lv_obj_set_height(header, 22);
  lv_label_set_long_mode(header, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(header, Marca::f16(), 0);
  lv_obj_set_style_text_color(header, lv_color_hex(Marca::PAPEL), 0);
  lv_obj_set_style_text_align(header, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_pad_top(header, 2, 0);
  lv_label_set_text(header, "");

  // CAIXA: PRETO_2, raio 12, sem borda. Rola na vertical só se o texto da
  // macro for maior que a caixa (sem inércia nem elástico: mais leve).
  Marca::cartao(body_cont);
  lv_obj_set_style_pad_all(body_cont, 12, 0);
  lv_obj_set_style_pad_row(body_cont, 12, 0);
  lv_obj_add_flag(body_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(body_cont, LV_DIR_VER);
  lv_obj_clear_flag(body_cont, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_clear_flag(body_cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_flex_flow(body_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(body_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  // TEXTO: f14 PAPEL, quebra de linha, alinhado à esquerda (como o quadro 10).
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_style_pad_all(body, 0, 0);
  lv_obj_set_style_text_font(body, Marca::f14(), 0);
  lv_obj_set_style_text_color(body, lv_color_hex(Marca::PAPEL), 0);
  lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_label_set_text(body, "");

  // BOTÕES DA MACRO (dentro da caixa): uma fileira, todos da mesma largura
  // (flex_grow em consume()), o rótulo quebra em 2 linhas se precisar.
  // Mesmo comportamento de antes: não pula para outra fileira.
  lv_obj_set_style_pad_all(button_cont, 0, 0);
  lv_obj_set_style_pad_column(button_cont, Marca::VAO, 0);
  lv_obj_set_style_pad_row(button_cont, Marca::VAO, 0);
  lv_obj_set_flex_flow(button_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(button_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_size(button_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_clear_flag(button_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(button_cont, LV_OBJ_FLAG_CLICKABLE);

  // RODAPÉ: mesma fileira de largura igual, encostada embaixo (y = 220).
  lv_obj_set_size(footer_cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(footer_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(footer_cont,
    LV_FLEX_ALIGN_START,
    LV_FLEX_ALIGN_CENTER,
    LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(footer_cont, 0, 0);
  lv_obj_set_style_pad_column(footer_cont, Marca::VAO, 0);
  lv_obj_clear_flag(footer_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(footer_cont, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);

  ws.register_notify_update(this);
  ws.register_method_callback(
    "notify_gcode_response",
    "PromptPanel",
    [this](json &d) { consume(d); }
  );
}

PromptPanel::~PromptPanel() {
  if (prompt_cont) lv_obj_del(prompt_cont);
  ws.unregister_notify_update(this);
}

void PromptPanel::foreground() {
  lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(prompt_cont);
}

void PromptPanel::background() {
  lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_background(prompt_cont);
}

bool PromptPanel::is_visible() const {
  return !lv_obj_has_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
}

void PromptPanel::on_button_click(lv_event_t *e) {
  auto btn = lv_event_get_target(e);
  auto pcmd = static_cast<std::string *>(btn->user_data);
  auto panel = static_cast<PromptPanel *>(lv_event_get_user_data(e));
  if (pcmd && !pcmd->empty()) {
    // Previously fire-and-forget with no response handling at all: a Klipper-
    // side rejection of the button's gcode (e.g. a macro erroring out) was
    // completely invisible - the button just looked like it did nothing, no
    // different from a real failure. Surface it as a toast instead. This
    // callback runs on the websocket thread, not the LVGL thread, so it must
    // take lv_lock before touching any LVGL object (same pattern as
    // FilamentRunoutPanel::send_load_chunk).
    panel->ws.gcode_script(*pcmd, [panel](json &j) {
      if (j.contains("error")) {
        std::string msg = "Falha ao executar o comando";
        if (j["error"].contains("message") && j["error"]["message"].is_string()) {
          msg = j["error"]["message"].template get<std::string>();
        }
        std::lock_guard<std::mutex> lock(panel->lv_lock);
        KUtils::notify_toast(msg);
      }
    });
  }
}

void PromptPanel::on_button_delete(lv_event_t *e) {
  // Antes o texto do comando nunca era liberado: cada janela do Klipper
  // deixava alguns bytes perdidos na memória. Agora sai junto com o botão
  // (prompt_end apaga os botões com lv_obj_clean).
  auto btn = lv_event_get_target(e);
  auto pcmd = static_cast<std::string *>(btn->user_data);
  btn->user_data = nullptr;
  delete pcmd;
}

void PromptPanel::consume(json &j) {
  auto &v = j["/params/0"_json_pointer];
  if (!v.is_string()) return;
  auto resp = v.get<std::string>();

  // Klipper reports errors via notify_gcode_response in TWO different shapes,
  // confirmed empirically against a real device (2026-07-16), and NEITHER is
  // redundant with on_button_click's own RPC-response error check above:
  //   - "!! <json>"  - genuine command-level runtime errors (e.g. cold
  //     extrude) - these ALSO populate printer.gcode.script's own RPC
  //     response with an "error" field, so this is a harmless duplicate toast
  //     for those specifically.
  //   - "// {<json>}" - a totally unrecognized/mistyped gcode command (e.g. a
  //     typo'd macro name). This one is NOT a duplicate: Moonraker's RPC
  //     response for this case is plain {"result":"ok"} with no error field
  //     at all - this is the ONLY signal that ever surfaces it. Plain "// "
  //     informational text (e.g. "// Purging filament...") is NOT JSON and
  //     must not trigger a toast - checking for "// {" specifically (not just
  //     "// ") is what distinguishes a structured error notice from ordinary
  //     human-readable chatter.
  bool is_error_notice = resp.rfind("!! ", 0) == 0;
  if (!is_error_notice && resp.rfind("// {", 0) == 0) {
    is_error_notice = true;
  }
  if (is_error_notice) {
    std::string msg = resp.substr(3);
    try {
      auto ej = json::parse(msg);
      if (ej.contains("msg") && ej["msg"].is_string()) {
        msg = ej["msg"].template get<std::string>();
      }
    } catch (...) {
      // not JSON - show the raw text as-is
    }
    std::lock_guard<std::mutex> guard(lv_lock);
    KUtils::notify_toast(msg);
    return;
  }

  if (resp.rfind("// action:", 0) != 0) return;
  auto cmd = resp.substr(10);

  std::lock_guard<std::mutex> guard(lv_lock);

  if (cmd.rfind("prompt_begin", 0) == 0) {
    lv_label_set_text(header, (std::string(LV_SYMBOL_WARNING) + " " + cmd.substr(13)).c_str());

  } else if (cmd.rfind("prompt_text", 0) == 0) {
    lv_label_set_text(body, cmd.substr(12).c_str());

  } else if (cmd.rfind("prompt_button", 0) == 0 || cmd.rfind("prompt_footer_button", 0) == 0) {
    if (cmd.rfind("prompt_button_group", 0) == 0) return;
    auto parts = KUtils::split(cmd, '|');
    auto label = parts[0].substr(parts[0].find("button") + 6);
    auto gcode = parts.size() > 1 ? parts[1] : "";
    auto type = parts.size() > 2 ? parts[2] : "default";

    lv_obj_t *parent = (cmd.rfind("prompt_footer_button", 0) == 0) ? footer_cont : button_cont;

    // Só aparência: o rótulo sem o espaço do começo; o comando fica intacto.
    size_t ini = label.find_first_not_of(' ');
    std::string texto = ini == std::string::npos ? "" : label.substr(ini);

    // Tipo do botão na marca. Cancelar a impressão (o comando contém
    // CANCEL_PRINT, ex.: _M600_CANCEL_PRINT) e "error" = PERIGO (vermelho);
    // "primary" = DESTAQUE (limão, texto preto); "warning" = fundo ALERTA com
    // texto papel; os demais = NORMAL. Só lê o comando, não muda nada nele.
    std::string gcode_up = gcode;
    std::transform(gcode_up.begin(), gcode_up.end(), gcode_up.begin(),
      [](unsigned char ch) { return (char)std::toupper(ch); });
    Marca::Tipo tipo = Marca::Tipo::NORMAL;
    bool alerta = false;
    if (gcode_up.find("CANCEL_PRINT") != std::string::npos || type == "error") {
      tipo = Marca::Tipo::PERIGO;
    } else if (type == "primary") {
      tipo = Marca::Tipo::DESTAQUE;
    } else if (type == "warning") {
      alerta = true;
    }

    auto *b = Marca::criar_botao(parent, texto.c_str(), tipo);
    if (alerta) {
      lv_color_t cor_alerta = lv_color_hex(Marca::ALERTA);
      lv_obj_set_style_bg_color(b, cor_alerta, 0);
      lv_obj_set_style_bg_color(b, lv_color_darken(cor_alerta, LV_OPA_20), LV_STATE_PRESSED);
    }
    lv_obj_set_height(b, Marca::TOQUE);
    lv_obj_set_style_pad_hor(b, 4, 0);

    // Fileira de largura igual (ver os comentários do construtor): cresce
    // para dividir a fileira; o rótulo quebra em até 2 linhas no botão.
    lv_obj_set_flex_grow(b, 1);
    auto *lbl = lv_obj_get_child(b, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);

    b->user_data = new std::string(gcode);
    lv_obj_add_event_cb(b, PromptPanel::on_button_click, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(b, PromptPanel::on_button_delete, LV_EVENT_DELETE, NULL);

  } else if (cmd == "prompt_show") {
    lv_obj_update_layout(prompt_cont);
    foreground();

  } else if (cmd == "prompt_end") {
    lv_label_set_text(header, "");
    lv_label_set_text(body, "");
    lv_obj_clean(button_cont);
    lv_obj_clean(footer_cont);
    lv_obj_scroll_to_y(body_cont, 0, LV_ANIM_OFF);
    background();
  }
}
