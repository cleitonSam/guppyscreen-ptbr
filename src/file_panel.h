#ifndef __FILE_PANEL_H__
#define __FILE_PANEL_H__

#include "lvgl/lvgl.h"
#include "hv/json.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

using json = nlohmann::json;

// Detalhes do arquivo (Proposta B, quadro 03): subtela própria com cabeçalho
// "Detalhes do arquivo", miniatura de 128, nome, 5 linhas de dados e o botão
// limão "Imprimir". O botão só é desenhado aqui: quem manda imprimir é o
// PrintPanel (get_print_button()).
class FilePanel {
public:
  // parent: onde o contêiner nasce (lv_scr_act()); ele vira subtela.
  FilePanel(lv_obj_t *parent);
  ~FilePanel();

  void foreground();                  // abre a subtela
  void background();                  // fecha a subtela

  // Dados que já se sabe pela lista (nome, tamanho, data); o resto fica "--".
  void show_basic(const std::string &gcode_path, size_t size_bytes, uint32_t modified);
  // Metadados completos. A miniatura (120) é escolhida aqui se ainda não for
  // a deste arquivo.
  void refresh_view(json &j, const std::string &gcode_path);
  // Igual, com a miniatura já escolhida fora do lv_lock (path, (w, h)).
  void refresh_view(json &j, const std::string &gcode_path,
                    const std::pair<std::string, std::pair<size_t, size_t>> &thumb);
  void show_loading(const std::string &gcode_path);
  void show_no_metadata();

  lv_obj_t *get_container();
  lv_obj_t *get_print_button();
  const std::string &get_gcode_path() const;
  const char *get_thumbnail_path();

private:
  void set_thumbnail(const std::string &gcode_path, const std::string &file, size_t w, size_t h);
  void clear_values();

  lv_obj_t *file_cont;
  lv_obj_t *thumbnail_container;
  lv_obj_t *thumbnail;
  lv_obj_t *no_thumb_icon;
  lv_obj_t *fname_label;
  lv_obj_t *row_labels[5];
  lv_obj_t *row_values[5];
  lv_obj_t *print_btn;

  std::string gcode_path;     // arquivo mostrado agora
  std::string thumb_for;      // arquivo da miniatura que está na tela
};

#endif // __FILE_PANEL_H__
