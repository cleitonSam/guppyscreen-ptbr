#include "file_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {
  // Quadro 03: linhas em y = 95, 116, 137, 158, 179 (coordenadas da subtela)
  constexpr lv_coord_t LINHA_Y[5] = {95, 116, 137, 158, 179};
  const char *LINHA_ROTULO[5] = {
    "Tempo de impressão", "Filamento", "Bico / mesa", "Tamanho", "Modificado em"
  };
  enum { L_TEMPO = 0, L_FILAMENTO, L_TEMPERATURAS, L_TAMANHO, L_MODIFICADO };

  constexpr lv_coord_t MINI_LADO = 120;   // a miniatura cabe em 120 dentro do cartão de 128

  // nome do arquivo sem a pasta e sem ".gcode"
  std::string nome_curto(const std::string &caminho) {
    std::string n = caminho;
    size_t barra = n.find_last_of('/');
    if (barra != std::string::npos) {
      n = n.substr(barra + 1);
    }
    const std::string ext = ".gcode";
    if (n.size() > ext.size() && n.compare(n.size() - ext.size(), ext.size(), ext) == 0) {
      n = n.substr(0, n.size() - ext.size());
    }
    return n;
  }

  std::string data_curta(double quando) {
    if (quando <= 0) {
      return "--";
    }
    std::time_t t = (std::time_t)quando;
    std::tm lt = *std::localtime(&t);
    std::stringstream s;
    s << std::put_time(&lt, "%d/%m %H:%M");
    return s.str();
  }

  // campo de /result sem criar nada no json (operator[] criaria)
  const json *campo(const json &j, const char *nome) {
    auto r = j.find("result");
    if (r == j.end() || !r->is_object()) {
      return nullptr;
    }
    auto f = r->find(nome);
    if (f == r->end() || f->is_null()) {
      return nullptr;
    }
    return &(*f);
  }

  double numero(const json *v, double padrao = -1) {
    if (v != nullptr && v->is_number()) {
      return v->template get<double>();
    }
    return padrao;
  }

  lv_obj_t *rotulo_linha(lv_obj_t *pai, const char *txt, const lv_font_t *f, uint32_t cor,
                         lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_text_align_t alinhar) {
    lv_obj_t *l = Marca::criar_rotulo(pai, txt, f, cor);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_size(l, w, lv_font_get_line_height(f));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(l, alinhar, 0);
    return l;
  }
}

FilePanel::FilePanel(lv_obj_t *parent)
  : file_cont(lv_obj_create(parent))
  , thumbnail_container(NULL)
  , thumbnail(NULL)
  , no_thumb_icon(NULL)
  , fname_label(NULL)
  , print_btn(NULL)
{
  // subtela: (56,0) 424 x 272, fundo PRETO, pad 0, sem rolagem, começa atrás
  Navegacao::subtela(file_cont);
  Marca::criar_cabecalho(file_cont, "Detalhes do arquivo");

  // miniatura: cartão 128 x 128 em (8,52)
  thumbnail_container = Marca::criar_cartao(file_cont, 8, 52, 128, 128);
  lv_obj_clear_flag(thumbnail_container, LV_OBJ_FLAG_CLICKABLE);

  no_thumb_icon = Marca::criar_rotulo(thumbnail_container, LV_SYMBOL_FILE, Marca::f20(), Marca::GRAFITE);
  lv_obj_center(no_thumb_icon);

  thumbnail = lv_img_create(thumbnail_container);
  lv_obj_clear_flag(thumbnail, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);

  // nome: f14 em (148,52) 268 x 34, 2 linhas com "..."
  fname_label = Marca::criar_rotulo(file_cont, "", Marca::f14(), Marca::PAPEL);
  lv_obj_set_pos(fname_label, 148, 52);
  lv_obj_set_size(fname_label, 268, 34);
  lv_label_set_long_mode(fname_label, LV_LABEL_LONG_DOT);

  // 5 linhas: rótulo 12 grafite à esquerda, valor f14 papel à direita
  for (int i = 0; i < 5; i++) {
    // "Tempo de impressão" mede 128 px na f12: 136 de largura para não quebrar
    // (o valor é alinhado à direita, não encosta)
    row_labels[i] = rotulo_linha(file_cont, LINHA_ROTULO[i], Marca::f12(), Marca::GRAFITE,
                                 148, LINHA_Y[i], 136, LV_TEXT_ALIGN_LEFT);
    row_values[i] = rotulo_linha(file_cont, "--", Marca::f14(), Marca::PAPEL,
                                 276, LINHA_Y[i] - 1, 140, LV_TEXT_ALIGN_RIGHT);
  }

  // botão limão largo: o único da tela
  print_btn = Marca::criar_botao(file_cont, LV_SYMBOL_PLAY " Imprimir", Marca::Tipo::DESTAQUE, Marca::f16());
  lv_obj_set_pos(print_btn, 8, 212);
  lv_obj_set_size(print_btn, 408, 48);
}

FilePanel::~FilePanel() {
  // A subtela fica registrada na Navegacao: não é apagada (regra 6 da
  // implementação B; o painel vive o programa inteiro).
  file_cont = NULL;
}

void FilePanel::foreground() {
  Navegacao::abrir(file_cont);
}

void FilePanel::background() {
  Navegacao::fechar(file_cont);
}

void FilePanel::clear_values() {
  for (int i = 0; i < 5; i++) {
    Marca::texto(row_values[i], "--");
  }
}

void FilePanel::show_basic(const std::string &path, size_t size_bytes, uint32_t modified) {
  if (path != gcode_path) {
    gcode_path = path;
    clear_values();
  }
  Marca::texto(fname_label, nome_curto(path).c_str());
  Marca::texto(row_values[L_TAMANHO], size_bytes > 0 ? KUtils::tamanho_arquivo(size_bytes).c_str() : "--");
  Marca::texto(row_values[L_MODIFICADO], data_curta(modified).c_str());

  if (thumb_for != path) {
    // outro arquivo: a miniatura antiga sai até a nova chegar
    lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(no_thumb_icon, LV_OBJ_FLAG_HIDDEN);
  }
}

void FilePanel::refresh_view(json &j, const std::string &path) {
  if (thumb_for == path) {
    // miniatura já está na tela: só os textos
    std::pair<std::string, std::pair<size_t, size_t>> nada;
    refresh_view(j, path, nada);
    return;
  }

  std::pair<std::string, std::pair<size_t, size_t>> thumb;
  try {
    thumb = KUtils::get_thumbnail(path, j, MINI_LADO);
  } catch (const std::exception &e) {
    spdlog::warn("miniatura de {} ilegível: {}", path, e.what());
  }
  refresh_view(j, path, thumb);
}

void FilePanel::refresh_view(json &j, const std::string &path,
                             const std::pair<std::string, std::pair<size_t, size_t>> &thumb) {
  if (path != gcode_path) {
    gcode_path = path;
    clear_values();
  }
  Marca::texto(fname_label, nome_curto(path).c_str());

  // Tempo de impressão
  double eta = numero(campo(j, "estimated_time"));
  Marca::texto(row_values[L_TEMPO], eta > 0 ? KUtils::tempo_curto((int64_t)eta).c_str() : "--");

  // Filamento: tipo; sem tipo, o peso em g
  std::string filamento = "--";
  const json *tipo = campo(j, "filament_type");
  if (tipo != nullptr && tipo->is_string() && !tipo->template get<std::string>().empty()) {
    filamento = tipo->template get<std::string>();
  } else {
    double peso = numero(campo(j, "filament_weight_total"));
    if (peso > 0) {
      filamento = fmt::format("{:.0f} g", peso);
    }
  }
  Marca::texto(row_values[L_FILAMENTO], filamento.c_str());

  // Bico / mesa (primeira camada)
  double bico = numero(campo(j, "first_layer_extr_temp"));
  double mesa = numero(campo(j, "first_layer_bed_temp"));
  std::string temps = "--";
  if (bico >= 0 && mesa >= 0) {
    temps = fmt::format("{:.0f}°C / {:.0f}°C", bico, mesa);
  } else if (bico >= 0) {
    temps = fmt::format("{:.0f}°C / --", bico);
  }
  Marca::texto(row_values[L_TEMPERATURAS], temps.c_str());

  // Tamanho e data (se o metadado trouxer; senão fica o da lista)
  double tamanho = numero(campo(j, "size"));
  if (tamanho > 0) {
    Marca::texto(row_values[L_TAMANHO], KUtils::tamanho_arquivo((size_t)tamanho).c_str());
  }
  double modificado = numero(campo(j, "modified"));
  if (modificado > 0) {
    Marca::texto(row_values[L_MODIFICADO], data_curta(modificado).c_str());
  }

  if (!thumb.first.empty()) {
    set_thumbnail(path, thumb.first, thumb.second.first, thumb.second.second);
  } else if (thumb_for != path) {
    lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(no_thumb_icon, LV_OBJ_FLAG_HIDDEN);
  }
}

void FilePanel::set_thumbnail(const std::string &path, const std::string &file, size_t w, size_t h) {
  if (file.empty() || w == 0 || h == 0) {
    lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(no_thumb_icon, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  const std::string src = "A:" + file;
  const void *atual = lv_img_get_src(thumbnail);
  if (atual == NULL || lv_img_src_get_type(atual) != LV_IMG_SRC_FILE
      || src != (const char *)atual) {
    lv_img_set_src(thumbnail, src.c_str());
  }
  size_t maior = std::max(w, h);
  uint32_t zoom = (uint32_t)(MINI_LADO * 256 / maior);
  if (zoom == 0) zoom = 1;
  if (zoom > 1024) zoom = 1024;
  lv_img_set_zoom(thumbnail, (uint16_t)zoom);
  lv_obj_center(thumbnail);
  lv_obj_clear_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(no_thumb_icon, LV_OBJ_FLAG_HIDDEN);
  thumb_for = path;
}

void FilePanel::show_loading(const std::string &path) {
  show_basic(path, 0, 0);
}

void FilePanel::show_no_metadata() {
  // Sem metadados (ex.: arquivo do pendrive que o Moonraker não leu): fica o
  // ícone no lugar da miniatura e "--" nos dados que faltam.
  lv_obj_add_flag(thumbnail, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(no_thumb_icon, LV_OBJ_FLAG_HIDDEN);
  thumb_for.clear();
}

lv_obj_t *FilePanel::get_container() {
  return file_cont;
}

lv_obj_t *FilePanel::get_print_button() {
  return print_btn;
}

const std::string &FilePanel::get_gcode_path() const {
  return gcode_path;
}

const char *FilePanel::get_thumbnail_path() {
  if (lv_obj_has_flag(thumbnail, LV_OBJ_FLAG_HIDDEN)) {
    return NULL;
  }
  const void *src = lv_img_get_src(thumbnail);
  if (src == NULL || lv_img_src_get_type(src) != LV_IMG_SRC_FILE) {
    return NULL;
  }
  return (const char *)src;
}
