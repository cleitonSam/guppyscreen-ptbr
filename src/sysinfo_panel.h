#ifndef __SYSINFO_PANEL_H__
#define __SYSINFO_PANEL_H__

#include "navegacao.h"
#include "lvgl/lvgl.h"

#include <functional>
#include <vector>
#include <string>

// Peças da lista das abas de Configurações (quadro 12 da maquete B):
// cartão de 408 de largura, linhas de 44, rótulo f14 em (14,13) e o controle
// à direita na área (250,0) 144 x 44 (lista suspensa, chave 44 x 24 ou a seta
// LV_SYMBOL_RIGHT de quem abre outra tela). Separador de 1 px PRETO_3 de
// (14,44) a (394,44) entre as linhas. Só aparência: nada aqui manda comando.
namespace ListaCfg {
  constexpr lv_coord_t LINHA = 44;          // altura de cada linha
  constexpr lv_coord_t CARTAO_X = 8;        // cartão em (8, y) do conteúdo da aba
  constexpr lv_coord_t CARTAO_L = 408;
  constexpr lv_coord_t ROTULO_X = 14;
  constexpr lv_coord_t ROTULO_L = 250;
  constexpr lv_coord_t CTRL_X = 250;        // área do controle à direita
  constexpr lv_coord_t CTRL_L = 144;
  constexpr lv_coord_t RODAPE_Y = 197;      // rodapé das abas (12 G)

  // Conteúdo da aba (424 x 227, transparente, pad 0) colocado com
  // Navegacao::colocar. rolavel: rola na vertical, sem inércia e sem elástico.
  void preparar_conteudo(lv_obj_t *obj, Aba a, bool rolavel);
  // Cartão 408 x (linhas * 44) em (8, y), já com os separadores.
  lv_obj_t *criar_cartao(lv_obj_t *pai, lv_coord_t y, int linhas);
  // Rótulo f14 PAPEL da linha i, em (14, 44i + 13), largura 250, uma linha.
  lv_obj_t *rotulo(lv_obj_t *cartao, int i, const char *texto);
  // Linha inteira tocável (abre outra tela): rótulo + valor opcional
  // (f14 GRAFITE, à direita) + LV_SYMBOL_RIGHT. Filho 0 da linha = rótulo.
  lv_obj_t *linha_seta(lv_obj_t *cartao, int i, const char *texto, lv_obj_t **valor = nullptr);
  // Liga/desliga uma linha_seta (desligada: texto GRAFITE e não reage).
  void habilitar_linha(lv_obj_t *linha, bool sim);
  // Lista suspensa (filha do cartão) no lugar do controle da linha i: sem
  // fundo, texto f14 + LV_SYMBOL_DOWN GRAFITE encostados à direita.
  void lista(lv_obj_t *dd, int i);
  // Reposiciona o texto da lista ao lado da seta (chamar depois de mudar a escolha).
  void alinhar_lista(lv_obj_t *dd);
  // Chave (filha do cartão) 44 x 24 à direita da linha i.
  void chave(lv_obj_t *sw, int i);
  // Texto fixo f14 GRAFITE no lugar do controle da linha i.
  lv_obj_t *texto_direita(lv_obj_t *cartao, int i, const char *texto);
  // Rótulo do rodapé da aba (12 G) em (x, 197).
  lv_obj_t *rodape(lv_obj_t *pai, lv_coord_t x, lv_coord_t largura, const char *texto);

  // Endereço IP atual (Wi-Fi primeiro; so_wifi = só o Wi-Fi). "" sem endereço.
  // Só lê as interfaces (getifaddrs + ioctl): não fala com o wpa_supplicant.
  std::string ip_atual(bool so_wifi);
}

// Ajustes da tela e do sistema (os controles e as chaves de configuração de
// sempre, reorganizados nas abas da página Configurações):
//  - aba Tela (inteira): Brilho, Apagar tela, Som ao tocar, Confirmar parada
//    de emergência ("Sempre") e o rodapé com a versão e o estado do Wi-Fi;
//  - aba Sistema: cartão com Nível de registro, Temp. padrão, Inverter
//    direção do Z/Y e Opções de redefinição, no conteúdo que o SettingPanel
//    passa (sistema), na altura y.
class SysInfoPanel {
 public:
  SysInfoPanel(lv_obj_t *sistema, lv_coord_t y);
  ~SysInfoPanel();

  // Abre Configurações › Tela (com os textos atualizados).
  void foreground();
  // Atualiza o rodapé da aba Tela (estado do Wi-Fi). Com o lv_lock preso.
  void atualizar();
  void handle_callback(lv_event_t *event);
  void show_reset_options();
  void show_reset_confirm(const char *title, const char *detail,
                          const std::function<void()> &cb);

  static void _handle_callback(lv_event_t *event) {
    SysInfoPanel *panel = (SysInfoPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  // aba Tela
  lv_obj_t *tela_cont;
  lv_obj_t *tela_cartao;
  lv_obj_t *brightness_dd;     // nulo se /sys/class/backlight não tem aparelho
  lv_obj_t *display_sleep_dd;
  lv_obj_t *touch_beep_toggle;
  lv_obj_t *versao_label;
  lv_obj_t *wifi_label;

  // aba Sistema
  lv_obj_t *ajustes_cartao;
  lv_obj_t *loglevel_dd;
  uint32_t loglevel;
  lv_obj_t *def_temp_dd;
  lv_obj_t *z_icon_toggle;
  lv_obj_t *y_icon_toggle;
  lv_obj_t *reset_row;

  // janela "Opções de redefinição" (lv_layer_top, criada na 1ª vez)
  lv_obj_t *reset_janela;

  static std::vector<std::string> log_levels;
};

#endif //__SYSINFO_PANEL_H__
