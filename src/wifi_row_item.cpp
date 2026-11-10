#include "wifi_row_item.h"
#include "marca.h"

// Linha (largura LARGURA x 44):
//   símbolo do Wi-Fi f16 em (12,13): PAPEL com sinal bom/excelente, GRAFITE com razoável/fraco
//   nome f14 em (40,6), uma linha
//   "Conectado" / "Sinal: Bom" f12 GRAFITE em (40,24)
//   à direita: seta (conectada) ou o "esquecer" 44 x 44 (salva)
WifiRowItem::WifiRowItem(lv_obj_t *parent,
                         const std::string &ssid_,
                         int signal_dbm,
                         bool connected,
                         bool saved,
                         const std::string &nid_,
                         std::function<void(const std::string &)> on_activate_,
                         std::function<void(const std::string &, const std::string &)> on_forget_)
  : ssid(ssid_)
  , nid(nid_)
  , cont(lv_obj_create(parent))
  , on_activate(std::move(on_activate_))
  , on_forget(std::move(on_forget_))
{
  lv_obj_set_size(cont, LARGURA, Marca::TOQUE);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_radius(cont, 0, 0);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(cont, lv_color_hex(Marca::APERTADO), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(cont, 1, 0);
  lv_obj_set_style_border_side(cont, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(cont, lv_color_hex(Marca::PRETO_3), 0);

  // Bucket thresholds match common RSSI conventions (>=-50 excellent down to
  // <-70 weak) - reused as text in NetworkDetailPanel's Signal row too.
  int level = signal_dbm >= -50 ? 4 : signal_dbm >= -60 ? 3 : signal_dbm >= -70 ? 2 : 1;
  const char *signal_text = level == 4 ? "Sinal: Excelente" : level == 3 ? "Sinal: Bom"
                          : level == 2 ? "Sinal: Razoável" : "Sinal: Fraco";

  lv_obj_t *icone = Marca::criar_rotulo(cont, LV_SYMBOL_WIFI, Marca::f16(),
                                        level >= 3 ? Marca::PAPEL : Marca::GRAFITE);
  lv_obj_set_pos(icone, 12, 13);

  // espaço do texto: até o alvo da direita (44)
  const lv_coord_t texto_l = LARGURA - 40 - Marca::TOQUE;

  lv_obj_t *name_lbl = Marca::criar_rotulo(cont, ssid.c_str(), Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(name_lbl, 40, 6);
  lv_obj_set_width(name_lbl, texto_l);
  lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);

  lv_obj_t *sub = Marca::criar_rotulo(cont, connected ? "Conectado" : signal_text,
                                      Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(sub, 40, 24);
  lv_obj_set_width(sub, texto_l);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);

  if (connected) {
    // The only secondary affordance on the connected row - opens
    // NetworkDetailPanel. The row body does the same thing (see
    // handle_row_click), the chevron is purely a visual hint that this row
    // leads somewhere else.
    lv_obj_t *chev = Marca::criar_rotulo(cont, LV_SYMBOL_RIGHT, Marca::f14(), Marca::GRAFITE);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -14, 0);
  } else if (saved) {
    // Forget affordance for a known-but-not-connected network. Its own
    // clickable 44x44 box at the far right, so it can never be confused with
    // the row's own connect action - unlike the two flush 60px table columns
    // this replaces.
    lv_obj_t *forget_hit = lv_obj_create(cont);
    lv_obj_set_size(forget_hit, Marca::TOQUE, Marca::TOQUE - 1);
    lv_obj_align(forget_hit, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_clear_flag(forget_hit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(forget_hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(forget_hit, 0, 0);
    lv_obj_set_style_radius(forget_hit, 0, 0);
    lv_obj_set_style_bg_opa(forget_hit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(forget_hit, lv_color_hex(Marca::APERTADO), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(forget_hit, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(forget_hit, 0, 0);
    lv_obj_t *x = Marca::criar_rotulo(forget_hit, LV_SYMBOL_CLOSE, Marca::f14(), Marca::GRAFITE);
    lv_obj_center(x);
    lv_obj_add_event_cb(forget_hit, &WifiRowItem::_handle_forget_click, LV_EVENT_CLICKED, this);
  }

  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(cont, &WifiRowItem::_handle_row_click, LV_EVENT_CLICKED, this);
}

WifiRowItem::~WifiRowItem() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void WifiRowItem::handle_row_click(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (on_activate) on_activate(ssid);
}

void WifiRowItem::handle_forget_click(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (on_forget) on_forget(ssid, nid);
}
