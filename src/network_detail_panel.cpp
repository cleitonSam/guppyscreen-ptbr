#include "network_detail_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "spdlog/spdlog.h"

// Subtela 424 x 272:
//   cabeçalho com o nome da rede
//   cartão (8,52) 408 x 132: Sinal / Segurança / Endereço IP (linhas de 44)
//   "Configurar IP fixo" (8,212) 200 x 44 · "Esquecer esta rede" (216,212) 200 x 44
namespace {

constexpr lv_coord_t LINHA = 44;

lv_obj_t *make_info_row(lv_obj_t *card, int i, const char *label_text) {
  if (i > 0) {
    lv_obj_t *sep = lv_obj_create(card);
    lv_obj_set_pos(sep, 14, LINHA * i);
    lv_obj_set_size(sep, 380, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(Marca::PRETO_3), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
  }

  lv_obj_t *lbl = Marca::criar_rotulo(card, label_text, Marca::f14(), Marca::GRAFITE);
  lv_obj_set_pos(lbl, 14, LINHA * i + 13);
  lv_obj_set_width(lbl, 130);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);

  lv_obj_t *val = Marca::criar_rotulo(card, "", Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(val, 150, LINHA * i + 13);
  lv_obj_set_width(val, 244);
  lv_label_set_long_mode(val, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
  return val;
}

} // namespace

NetworkDetailPanel::NetworkDetailPanel(std::mutex &l,
                                       std::function<void()> on_configure_static_ip_,
                                       std::function<void()> on_forget_)
  : lv_lock(l)
  , cont(lv_obj_create(lv_scr_act()))
  , title_label(nullptr)
  , info_card(nullptr)
  , signal_val(nullptr)
  , security_val(nullptr)
  , ip_val(nullptr)
  , configure_btn(nullptr)
  , forget_btn(nullptr)
  , on_configure_static_ip(std::move(on_configure_static_ip_))
  , on_forget(std::move(on_forget_))
{
  Navegacao::subtela(cont);

  Marca::Cabecalho cab = Marca::criar_cabecalho(cont, "", &NetworkDetailPanel::_handle_back_btn, this);
  title_label = cab.titulo;

  info_card = Marca::criar_cartao(cont, 8, 52, 408, LINHA * 3);
  signal_val = make_info_row(info_card, 0, "Sinal");
  security_val = make_info_row(info_card, 1, "Segurança");
  ip_val = make_info_row(info_card, 2, "Endereço IP");

  configure_btn = Marca::criar_botao(cont, "Configurar IP fixo", Marca::Tipo::NORMAL);
  lv_obj_set_pos(configure_btn, 8, 212);
  lv_obj_set_size(configure_btn, 200, Marca::TOQUE);
  lv_obj_add_event_cb(configure_btn, &NetworkDetailPanel::_handle_configure_btn, LV_EVENT_CLICKED, this);

  forget_btn = Marca::criar_botao(cont, "Esquecer esta rede", Marca::Tipo::CONTORNO_PERIGO);
  lv_obj_set_pos(forget_btn, 216, 212);
  lv_obj_set_size(forget_btn, 200, Marca::TOQUE);
  lv_obj_add_event_cb(forget_btn, &NetworkDetailPanel::_handle_forget_btn, LV_EVENT_CLICKED, this);
}

NetworkDetailPanel::~NetworkDetailPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

void NetworkDetailPanel::foreground(const std::string &ssid,
                                    const std::string &signal_text,
                                    const std::string &security_text,
                                    const std::string &ip_text) {
  lv_label_set_text(title_label, ssid.c_str());
  lv_label_set_text(signal_val, signal_text.c_str());
  lv_label_set_text(security_val, security_text.c_str());
  lv_label_set_text(ip_val, ip_text.c_str());
  Navegacao::abrir(cont);
}

void NetworkDetailPanel::hide() {
  Navegacao::fechar(cont);
}

void NetworkDetailPanel::handle_back_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  hide();
}

void NetworkDetailPanel::handle_configure_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (on_configure_static_ip) on_configure_static_ip();
}

void NetworkDetailPanel::handle_forget_btn(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  if (on_forget) on_forget();
}
