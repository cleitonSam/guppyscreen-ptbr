#include "setting_panel.h"
#include "config.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "spdlog/spdlog.h"
#include "subprocess.hpp"

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;
namespace sp = subprocess;

// Sistema (coordenadas do conteúdo da aba, que rola na vertical):
//   y =   7 e  59  botões 200 x 44: reiniciar Klipper / firmware / tela, atualizar tela
//   y = 111        cartão: Impressoras, Spoolman
//   y = 207        cartão de ajustes do SysInfoPanel (5 linhas, até 427)
static constexpr lv_coord_t BOTOES_Y = 7;
static constexpr lv_coord_t CONEXOES_Y = 111;
static constexpr lv_coord_t AJUSTES_Y = 207;

static lv_obj_t *criar_botao_sistema(lv_obj_t *pai, const char *texto,
                                     lv_coord_t x, lv_coord_t y, void *ud) {
  lv_obj_t *btn = Marca::criar_botao(pai, texto, Marca::Tipo::NORMAL);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, 200, Marca::TOQUE);
  lv_obj_add_event_cb(btn, &SettingPanel::_handle_callback, LV_EVENT_CLICKED, ud);
  return btn;
}

SettingPanel::SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm)
  : ws(c)
  , cont(lv_obj_create(parent))
  , rede_cont(lv_obj_create(Navegacao::aba(Aba::REDE)))
#ifndef OS_ANDROID
  , wifi_panel(l)
#endif
  , sysinfo_panel(cont, AJUSTES_Y)
  , spoolman_panel(sm)
  , printer_select_panel()
  , restart_klipper_btn(NULL)
  , restart_firmware_btn(NULL)
  , guppy_restart_btn(NULL)
  , guppy_update_btn(NULL)
  , printer_select_row(NULL)
  , spoolman_row(NULL)
  , spoolman_ativo(false)
  , wifi_row(NULL)
  , wifi_valor(NULL)
  , ip_row(NULL)
  , ip_valor(NULL)
  , ip_fixo_row(NULL)
  , pm_toggle(NULL)
{
  // ------------------------------------------------------------ aba Sistema
  ListaCfg::preparar_conteudo(cont, Aba::SISTEMA, true);

  restart_klipper_btn = criar_botao_sistema(cont, LV_SYMBOL_REFRESH " Reiniciar Klipper", 8, BOTOES_Y, this);
  restart_firmware_btn = criar_botao_sistema(cont, LV_SYMBOL_REFRESH " Reiniciar firmware", 216, BOTOES_Y, this);
  guppy_restart_btn = criar_botao_sistema(cont, LV_SYMBOL_REFRESH " Reiniciar tela", 8, BOTOES_Y + 52, this);
  guppy_update_btn = criar_botao_sistema(cont, LV_SYMBOL_DOWNLOAD " Atualizar tela", 216, BOTOES_Y + 52, this);

  lv_obj_t *conexoes = ListaCfg::criar_cartao(cont, CONEXOES_Y, 2);
  printer_select_row = ListaCfg::linha_seta(conexoes, 0, "Impressoras");
  lv_obj_add_event_cb(printer_select_row, &SettingPanel::_handle_callback, LV_EVENT_CLICKED, this);
  spoolman_row = ListaCfg::linha_seta(conexoes, 1, "Spoolman");
  lv_obj_add_event_cb(spoolman_row, &SettingPanel::_handle_callback, LV_EVENT_CLICKED, this);
  ListaCfg::habilitar_linha(spoolman_row, false);   // liga em enable_spoolman()

  // --------------------------------------------------------------- aba Rede
  ListaCfg::preparar_conteudo(rede_cont, Aba::REDE, false);
  lv_obj_t *rede = ListaCfg::criar_cartao(rede_cont, 7, 4);

  wifi_row = ListaCfg::linha_seta(rede, 0, "Wi-Fi", &wifi_valor);
  lv_obj_add_event_cb(wifi_row, &SettingPanel::_handle_callback, LV_EVENT_CLICKED, this);

  ip_row = ListaCfg::linha_seta(rede, 1, "Endereço IP", &ip_valor);
  lv_obj_add_event_cb(ip_row, &SettingPanel::_handle_callback, LV_EVENT_CLICKED, this);

  ip_fixo_row = ListaCfg::linha_seta(rede, 2, "IP fixo");
  lv_obj_add_event_cb(ip_fixo_row, &SettingPanel::_handle_callback, LV_EVENT_CLICKED, this);

  // Baixa latência: a mesma chave /wifi_low_latency de antes (era o botão
  // da tela de Wi-Fi). O GuppyScreen::init e o WifiPanel reaplicam sozinhos
  // a cada ligação, como antes.
  ListaCfg::rotulo(rede, 3, "Baixa latência");
  pm_toggle = lv_switch_create(rede);
  {
    auto v = Config::get_instance()->get_json("/wifi_low_latency");
    if (!v.is_null() && v.template get<bool>()) {
      lv_obj_add_state(pm_toggle, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(pm_toggle, LV_STATE_CHECKED);
    }
  }
  ListaCfg::chave(pm_toggle, 3);
  lv_obj_add_event_cb(pm_toggle, &SettingPanel::_handle_callback, LV_EVENT_VALUE_CHANGED, this);

  ListaCfg::rodape(rede_cont, 8, 408, "Wi-Fi mais estável, sem economia de energia nem Bluetooth");

#ifdef OS_ANDROID
  ListaCfg::habilitar_linha(wifi_row, false);
  ListaCfg::habilitar_linha(ip_row, false);
  ListaCfg::habilitar_linha(ip_fixo_row, false);
#endif

  Navegacao::ao_mostrar(Aba::REDE, [this]() { atualizar_rede(); });
#ifndef OS_ANDROID
  wifi_panel.ao_mudar_rede = [this]() { atualizar_rede(); };
#endif
}

SettingPanel::~SettingPanel() {
  if (rede_cont != NULL) {
    lv_obj_del(rede_cont);
    rede_cont = NULL;
  }
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *SettingPanel::get_container() {
  return cont;
}

void SettingPanel::atualizar_rede() {
  std::string ip = ListaCfg::ip_atual(false);
  Marca::texto(ip_valor, ip.empty() ? "--" : ip.c_str());

  std::string rede;
#ifndef OS_ANDROID
  rede = wifi_panel.rede_atual();
#endif
  if (!rede.empty()) {
    Marca::texto(wifi_valor, rede.c_str());
  } else {
    // a tela ainda não leu o nome da rede (só lê ao abrir o Wi-Fi)
    Marca::texto(wifi_valor, ListaCfg::ip_atual(true).empty() ? "Desconectado" : "Conectado");
  }
}

void SettingPanel::reiniciar_tela() {
  Config *conf = Config::get_instance();
  auto init_script = conf->get<std::string>("/guppy_init_script");
  const fs::path script(init_script);
  if (fs::exists(script) || init_script.rfind("service guppyscreen", 0) == 0) {
    sp::call({init_script, "restart"});
  } else {
    spdlog::warn("Failed to restart Guppy Screen. Did not find restart script.");
  }
}

void SettingPanel::atualizar_tela() {
  // TODO: throw this inside the global threadpool to make it async
  auto update_script = fs::canonical("/proc/self/exe").parent_path() / "update.sh";
  const fs::path script(update_script);
  if (fs::exists(script)) {
    sp::call(script);
  } else {
    spdlog::warn("Failed to update Guppy Screen. Did not find update script.");
  }
}

void SettingPanel::handle_callback(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);

  if (code == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *obj = lv_event_get_target(event);
    if (obj == pm_toggle) {
      bool low_latency = lv_obj_has_state(pm_toggle, LV_STATE_CHECKED);
      KUtils::set_wifi_low_latency(low_latency);
      Config *conf = Config::get_instance();
      conf->set<bool>("/wifi_low_latency", low_latency);
      conf->save();
      spdlog::debug("wifi low-latency toggled: {}", low_latency);
    }
    return;
  }

  if (code != LV_EVENT_CLICKED) {
    return;
  }

  lv_obj_t *btn = lv_event_get_current_target(event);

  if (btn == wifi_row) {
    spdlog::trace("wifi pressed");
#ifndef OS_ANDROID
    wifi_panel.foreground();
#endif
  } else if (btn == ip_row) {
#ifndef OS_ANDROID
    wifi_panel.abrir_detalhes();
#endif
  } else if (btn == ip_fixo_row) {
#ifndef OS_ANDROID
    wifi_panel.abrir_ip_fixo();
#endif
  } else if (btn == restart_klipper_btn) {
    spdlog::trace("setting restart klipper pressed");
    KUtils::confirm_if_printing("A impressora está imprimindo.\nReiniciar o Klipper mesmo assim?",
      [this]() { ws.send_jsonrpc("printer.restart"); });
  } else if (btn == restart_firmware_btn) {
    spdlog::trace("setting restart firmware pressed");
    KUtils::confirm_if_printing("A impressora está imprimindo.\nReiniciar o firmware mesmo assim?",
      [this]() { ws.send_jsonrpc("printer.firmware_restart"); });
  } else if (btn == spoolman_row) {
    spdlog::trace("setting spoolman pressed");
    if (spoolman_ativo) {
      spoolman_panel.foreground();
    }
  } else if (btn == guppy_restart_btn) {
    spdlog::trace("restart guppy pressed");
    KUtils::confirm_if_printing("A impressora está imprimindo.\nReiniciar a tela mesmo assim?",
      [this]() { reiniciar_tela(); });
  } else if (btn == guppy_update_btn) {
    spdlog::trace("update guppy pressed");
    KUtils::confirm_if_printing("A impressora está imprimindo.\nAtualizar a tela mesmo assim?",
      [this]() { atualizar_tela(); });
  } else if (btn == printer_select_row) {
    spdlog::trace("setting printers pressed");
    printer_select_panel.foreground();
  }
}

void SettingPanel::enable_spoolman() {
  spoolman_ativo = true;
  ListaCfg::habilitar_linha(spoolman_row, true);
}
