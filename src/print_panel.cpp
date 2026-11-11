#include "print_panel.h"
#include "file_panel.h"
#include "marca.h"
#include "navegacao.h"
#include "spoolman_panel.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>

#define SORTED_BY_NAME 1 << 0
#define SORTED_BY_MODIFIED  1 << 1

namespace {
  // Barra de abas da página Arquivos: só filtra a lista (não é Navegacao::Aba).
  // MAPA ESTÁTICO: o LVGL guarda o ponteiro.
  const char *mapa_abas[] = {"Na impressora", "USB", "Histórico", ""};

  constexpr int POR_PAGINA = 3;
  constexpr lv_coord_t CARTAO_Y[POR_PAGINA] = {50, 122, 194};
  constexpr int MINI_LISTA = 56;        // caixinha da miniatura no cartão
  constexpr int MINI_DESEJADA = 48;     // pede a menor miniatura com largura >= 48
  constexpr int MINI_MAXIMA = 160;      // acima disso não mostra na lista (pesa decodificar)
  constexpr int MINI_DETALHE = 120;

  using Miniatura = std::pair<std::string, std::pair<size_t, size_t>>;

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
      return "";
    }
    std::time_t t = (std::time_t)quando;
    std::tm lt = *std::localtime(&t);
    std::stringstream s;
    s << std::put_time(&lt, "%d/%m %H:%M");
    return s.str();
  }

  std::string juntar(const std::vector<std::string> &partes) {
    std::string r;
    for (const auto &p : partes) {
      if (p.empty()) {
        continue;
      }
      if (!r.empty()) {
        r += " · ";
      }
      r += p;
    }
    return r;
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

  int dimensao(const json &t, const char *chave) {
    auto v = t.find(chave);
    if (v == t.end()) {
      return 0;
    }
    try {
      if (v->is_number()) {
        return v->template get<int>();
      }
      if (v->is_string()) {
        return std::stoi(v->template get<std::string>());
      }
    } catch (...) {
    }
    return 0;
  }

  // Largura da miniatura que KUtils::get_thumbnail(arquivo, j, desejada) vai
  // escolher (mesma regra: a menor >= desejada; se nenhuma, a maior). 0 = nenhuma.
  // Serve para decidir ANTES de baixar/abrir a imagem.
  int largura_escolhida(const json &j, int desejada) {
    const json *th = campo(j, "thumbnails");
    if (th == nullptr || !th->is_array() || th->empty()) {
      return 0;
    }
    int menor_ok = -1;
    int maior = -1;
    for (const auto &t : *th) {
      if (!t.is_object()) {
        continue;
      }
      int w = dimensao(t, "width");
      if (w >= desejada && (menor_ok < 0 || w < menor_ok)) {
        menor_ok = w;
      }
      if (w > maior) {
        maior = w;
      }
    }
    return menor_ok >= 0 ? menor_ok : std::max(maior, 0);
  }

  // Miniatura pequena da lista (<= 160 px) ou nada. Roda FORA do lv_lock
  // (pode baixar o arquivo quando a tela não roda na impressora).
  Miniatura miniatura_lista(const std::string &arquivo, json &j) {
    int w = largura_escolhida(j, MINI_DESEJADA);
    if (w <= 0 || w > MINI_MAXIMA) {
      return Miniatura();
    }
    try {
      return KUtils::get_thumbnail(arquivo, j, MINI_DESEJADA);
    } catch (const std::exception &e) {
      spdlog::warn("miniatura de {} ilegível: {}", arquivo, e.what());
    }
    return Miniatura();
  }

  // Miniatura dos Detalhes (cabe em 120). Também FORA do lv_lock.
  Miniatura miniatura_detalhe(const std::string &arquivo, json &j) {
    try {
      return KUtils::get_thumbnail(arquivo, j, MINI_DETALHE);
    } catch (const std::exception &e) {
      spdlog::warn("miniatura de {} ilegível: {}", arquivo, e.what());
    }
    return Miniatura();
  }

  std::string minusculas(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
    return s;
  }

  bool comeca_com(const std::string &s, const std::string &prefixo) {
    return s.size() >= prefixo.size() && s.compare(0, prefixo.size(), prefixo) == 0;
  }

  void tamanho_rotulo(lv_obj_t *l, lv_coord_t w, const lv_font_t *f, int linhas) {
    lv_obj_set_size(l, w, lv_font_get_line_height(f) * linhas + (linhas > 1 ? 2 : 0));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  }
}

PrintPanel::PrintPanel(KWebSocketClient &websocket, std::mutex &lock, PrintStatusPanel &ps, SpoolmanPanel &spool)
  : NotifyConsumer(lock)
  , ws(websocket)
  , sm(spool)
  , files_cont(lv_obj_create(lv_scr_act()))
  , filament_cont(lv_obj_create(lv_layer_top()))
  , filament_box(lv_obj_create(filament_cont))
  , filament_content_cont(lv_obj_create(filament_box))
  , filament_row_cont(lv_obj_create(filament_content_cont))
  , filament_swatch(lv_obj_create(filament_row_cont))
  , filament_name_label(lv_label_create(filament_row_cont))
  , filament_detail_label(lv_label_create(filament_content_cont))
  , filament_enough_label(lv_label_create(filament_content_cont))
  , filament_yes_btn(lv_btn_create(filament_box))
  , filament_no_btn(lv_btn_create(filament_box))
  , root("", "", 0)
  , cur_dir(&root)
  , file_panel(lv_scr_act())
  , print_status(ps)
  , sort_mode(SORTED_BY_MODIFIED)
  , sort_reversed(true)
{
  spdlog::trace("building print panel");

  // ---- Página Arquivos (quadro 02) -----------------------------------------
  Navegacao::colocar(files_cont, Pagina::ARQUIVOS);
  lv_obj_clear_flag(files_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(files_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_pad_all(files_cont, 0, 0);
  lv_obj_set_style_border_width(files_cont, 0, 0);
  lv_obj_set_style_radius(files_cont, 0, 0);
  // a página já é PRETO opaco: o conteúdo não desenha fundo
  lv_obj_set_style_bg_opa(files_cont, LV_OPA_TRANSP, 0);

  // abas Na impressora / USB / Histórico: (8,0) 336 x 44 + linha de 1 px em y = 44
  tabs = Marca::criar_barra_abas(files_cont, mapa_abas, 8, 336);
  lv_obj_add_event_cb(tabs, &PrintPanel::_handle_tabs, LV_EVENT_VALUE_CHANGED, this);

  // ordenar: fantasma 72 x 44 em (344,0)
  sort_btn = Marca::criar_botao(files_cont, "Data " LV_SYMBOL_DOWN, Marca::Tipo::FANTASMA, Marca::f14());
  lv_obj_set_pos(sort_btn, 344, 0);
  lv_obj_set_size(sort_btn, 72, Marca::TOQUE);
  lv_obj_add_event_cb(sort_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);

  // 3 cartões 352 x 68, criados uma vez e só reescritos
  for (int i = 0; i < POR_PAGINA; i++) {
    Card &c = cards[i];
    c.card = lv_obj_create(files_cont);
    Marca::cartao_tocavel(c.card);
    lv_obj_set_pos(c.card, 8, CARTAO_Y[i]);
    lv_obj_set_size(c.card, 352, 68);
    lv_obj_add_event_cb(c.card, &PrintPanel::_handle_callback, LV_EVENT_CLICKED, this);

    // caixinha 56 x 56 em (6,6), PRETO_3 raio 8
    c.mini = lv_obj_create(c.card);
    lv_obj_set_pos(c.mini, 6, 6);
    lv_obj_set_size(c.mini, MINI_LISTA, MINI_LISTA);
    lv_obj_set_style_bg_color(c.mini, lv_color_hex(Marca::PRETO_3), 0);
    lv_obj_set_style_bg_opa(c.mini, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c.mini, Marca::RAIO_MINI, 0);
    lv_obj_set_style_border_width(c.mini, 0, 0);
    lv_obj_set_style_pad_all(c.mini, 0, 0);
    lv_obj_clear_flag(c.mini, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c.mini, LV_OBJ_FLAG_CLICKABLE);

    c.icon = Marca::criar_rotulo(c.mini, LV_SYMBOL_FILE, Marca::f20(), Marca::GRAFITE);
    lv_obj_center(c.icon);

    c.img = lv_img_create(c.mini);
    lv_obj_clear_flag(c.img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(c.img, LV_OBJ_FLAG_HIDDEN);

    c.badge = Marca::criar_selo(c.mini, 28, Marca::OK, Marca::PAPEL, LV_SYMBOL_OK);
    lv_obj_center(c.badge);
    lv_obj_add_flag(c.badge, LV_OBJ_FLAG_HIDDEN);

    // nome f14 em (72,8) 268 x 34, 2 linhas com "..."
    c.name = Marca::criar_rotulo(c.card, "", Marca::f14(), Marca::PAPEL);
    lv_obj_set_pos(c.name, 72, 8);
    lv_obj_set_size(c.name, 268, 34);
    lv_label_set_long_mode(c.name, LV_LABEL_LONG_DOT);

    // tempo · tamanho · data: 12 grafite em (72,46) largura 268
    c.meta = Marca::criar_rotulo(c.card, "", Marca::f12(), Marca::GRAFITE);
    lv_obj_set_pos(c.meta, 72, 46);
    tamanho_rotulo(c.meta, 268, Marca::f12(), 1);

    lv_obj_add_flag(c.card, LV_OBJ_FLAG_HIDDEN);
  }

  // setas redondas 48 x 48 e "1/2"
  page_up_btn = Marca::criar_botao(files_cont, LV_SYMBOL_UP, Marca::Tipo::NORMAL, Marca::f20());
  lv_obj_set_pos(page_up_btn, 368, 50);
  lv_obj_set_size(page_up_btn, 48, 48);
  lv_obj_set_style_radius(page_up_btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_event_cb(page_up_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);

  page_down_btn = Marca::criar_botao(files_cont, LV_SYMBOL_DOWN, Marca::Tipo::NORMAL, Marca::f20());
  lv_obj_set_pos(page_down_btn, 368, 214);
  lv_obj_set_size(page_down_btn, 48, 48);
  lv_obj_set_style_radius(page_down_btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_add_event_cb(page_down_btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);

  page_label = Marca::criar_rotulo(files_cont, "1/1", Marca::f12(), Marca::GRAFITE);
  lv_obj_set_pos(page_label, 368, 148);
  tamanho_rotulo(page_label, 48, Marca::f12(), 1);
  lv_obj_set_style_text_align(page_label, LV_TEXT_ALIGN_CENTER, 0);

  // ---- Detalhes do arquivo (quadro 03): botão Imprimir ---------------------
  lv_obj_add_event_cb(file_panel.get_print_button(), &PrintPanel::_handle_print_callback,
                      LV_EVENT_CLICKED, this);

  // ---- Pergunta do Spoolman ("usar o mesmo filamento?") --------------------
  // Mora em lv_layer_top() (janela): o acompanhamento automático não a esconde.
  KUtils::style_dialog_overlay(filament_cont);
  lv_obj_add_flag(filament_cont, LV_OBJ_FLAG_HIDDEN);

  KUtils::style_dialog_box(filament_box);
  lv_obj_set_size(filament_box, 384, 200);
  lv_obj_align(filament_box, LV_ALIGN_CENTER, 0, 0);

  // header (text swapped per spool state in show_filament_dialog)
  filament_title_label = lv_label_create(filament_box);
  lv_label_set_text(filament_title_label, "Usar o mesmo filamento?");
  lv_obj_set_width(filament_title_label, LV_PCT(100));
  lv_label_set_long_mode(filament_title_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(filament_title_label, LV_TEXT_ALIGN_CENTER, 0);
  KUtils::style_dialog_title(filament_title_label);

  // content block: a centered group of rows, centered in the box
  lv_obj_remove_style_all(filament_content_cont);
  lv_obj_set_width(filament_content_cont, LV_PCT(100));
  lv_obj_set_height(filament_content_cont, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(filament_content_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(filament_content_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(filament_content_cont, 8, 0);
  lv_obj_clear_flag(filament_content_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(filament_content_cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(filament_content_cont, LV_ALIGN_CENTER, 0, -4);

  // swatch + name, centered together as a group
  lv_obj_remove_style_all(filament_row_cont);
  lv_obj_set_width(filament_row_cont, LV_SIZE_CONTENT);
  lv_obj_set_height(filament_row_cont, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(filament_row_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(filament_row_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(filament_row_cont, 8, 0);
  lv_obj_clear_flag(filament_row_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(filament_row_cont, LV_OBJ_FLAG_CLICKABLE);

  // amostra da cor do carretel (a cor é a do filamento; borda grafite para
  // aparecer mesmo com filamento preto)
  lv_obj_set_size(filament_swatch, 20, 20);
  lv_obj_set_style_bg_opa(filament_swatch, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(filament_swatch, 1, 0);
  lv_obj_set_style_border_color(filament_swatch, lv_color_hex(Marca::GRAFITE), 0);
  lv_obj_set_style_radius(filament_swatch, 4, 0);
  lv_obj_clear_flag(filament_swatch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(filament_swatch, LV_OBJ_FLAG_CLICKABLE);

  lv_label_set_long_mode(filament_name_label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_max_width(filament_name_label, 300, 0);
  lv_obj_set_style_text_font(filament_name_label, Marca::f14(), 0);
  lv_obj_set_style_text_color(filament_name_label, lv_color_hex(Marca::PAPEL), 0);
  lv_label_set_text(filament_name_label, "");

  lv_obj_set_style_text_align(filament_detail_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(filament_detail_label, Marca::f14(), 0);
  lv_obj_set_style_text_color(filament_detail_label, lv_color_hex(Marca::GRAFITE), 0);
  lv_label_set_text(filament_detail_label, "");

  // "suficiente / insuficiente": selo com fundo OK / ERRO e texto papel
  lv_obj_set_style_text_align(filament_enough_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(filament_enough_label, Marca::f12(), 0);
  lv_obj_set_style_radius(filament_enough_label, Marca::RAIO, 0);
  lv_obj_set_style_pad_hor(filament_enough_label, 10, 0);
  lv_obj_set_style_pad_ver(filament_enough_label, 4, 0);
  lv_obj_set_style_bg_opa(filament_enough_label, LV_OPA_TRANSP, 0);
  lv_label_set_text(filament_enough_label, "");

  lv_obj_t *label = NULL;

  lv_obj_set_size(filament_yes_btn, 96, Marca::TOQUE);
  lv_obj_set_style_pad_all(filament_yes_btn, 0, 0);
  lv_obj_add_event_cb(filament_yes_btn, &PrintPanel::_handle_filament_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(filament_yes_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  label = lv_label_create(filament_yes_btn);
  lv_label_set_text(label, "Sim");
  lv_obj_set_style_text_font(label, Marca::f14(), 0);
  lv_obj_center(label);
  Marca::estilo_botao(filament_yes_btn, Marca::Tipo::DESTAQUE);

  lv_obj_set_size(filament_no_btn, 96, Marca::TOQUE);
  lv_obj_set_style_pad_all(filament_no_btn, 0, 0);
  lv_obj_add_event_cb(filament_no_btn, &PrintPanel::_handle_filament_btns, LV_EVENT_CLICKED, this);
  lv_obj_align(filament_no_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  label = lv_label_create(filament_no_btn);
  lv_label_set_text(label, "Não");
  lv_obj_set_style_text_font(label, Marca::f14(), 0);
  lv_obj_center(label);
  Marca::estilo_botao(filament_no_btn, Marca::Tipo::NORMAL);

  // ---- Navegação -----------------------------------------------------------
  // Ao mostrar a página Arquivos: o que o foreground() antigo fazia (lista na
  // hora com o que já se tem + pedir a lista nova ao Moonraker). Roda com o
  // lv_lock já preso: não prende de novo.
  Navegacao::ao_mostrar(Pagina::ARQUIVOS, [this]() {
    show_list();
    subscribe();
    if (tab == TAB_HISTORY) {
      request_history();
    }
  });

  // "Imprimir de novo" (Início e tela de concluída): abre os Detalhes do
  // arquivo; quem imprime é o botão Imprimir de lá (trava + Spoolman).
  Navegacao::registrar_acao("abrir_arquivo", [this](const std::string &arquivo) {
    open_file(arquivo);
  });

  // O acompanhamento abriu (uma impressão começou, de qualquer origem): a
  // pergunta "Usar o mesmo filamento?" fecha e o pedido pendente é
  // esquecido. Assim um "Sim" atrasado não tenta começar outra impressão.
  Navegacao::ao_abrir_acompanhamento([this]() {
    lv_obj_add_flag(filament_cont, LV_OBJ_FLAG_HIDDEN);
    pending_print_path.clear();
  });

  update_sort_label();
  show_list();

  ws.register_notify_update(this);
}

PrintPanel::~PrintPanel() {
  if (files_cont != NULL) {
    lv_obj_del(files_cont);
    files_cont = NULL;
  }

}

void PrintPanel::populate_files(json &j) {
  // A ordenação escolhida pelo dono fica (antes voltava para "Data" a cada
  // atualização da lista).
  show_dir(cur_dir);
}

void PrintPanel::consume(json &j) {
  auto m = j.find("method");
  if (m != j.end() && m->is_string() && *m == "notify_filelist_changed") {
    if (refreshing_files) {
      refresh_pending = true;
    } else {
      subscribe();
    }
  }
}

void PrintPanel::subscribe() {
  refreshing_files = true;
  refresh_pending = false;
  ws.send_jsonrpc("server.files.list", R"({"root":"gcodes"})"_json, [this](json &d) {
    std::lock_guard<std::mutex> lock(lv_lock);
    std::string cur_path = cur_dir != NULL ? cur_dir->full_path : "";

    // Guarda o que já foi lido dos arquivos (metadados e miniatura) para não
    // pedir de novo ao Moonraker o que não mudou.
    struct Lido {
      uint32_t modified;
      json metadata;
      bool thumb_checked;
      std::string thumb_path;
      size_t thumb_w;
      size_t thumb_h;
    };
    std::map<std::string, Lido> lidos;
    root.for_each_node([&lidos](Tree &t) {
      if (t.is_leaf() && t.has_metadata) {
        Lido l;
        l.modified = t.date_modified;
        l.metadata = std::move(t.metadata);
        l.thumb_checked = t.thumb_checked;
        l.thumb_path = t.thumb_path;
        l.thumb_w = t.thumb_w;
        l.thumb_h = t.thumb_h;
        lidos.emplace(t.full_path, std::move(l));
      }
    });

    root.clear();
    cur_dir = NULL;

    auto r = d.find("result");
    if (r != d.end() && r->is_array()) {
      for (auto &f : *r) {
        auto p = f.find("path");
        if (p == f.end() || !p->is_string()) {
          continue;
        }
        std::string path = p->template get<std::string>();
        auto mod = f.find("modified");
        uint32_t modified = (mod != f.end() && mod->is_number()) ? (uint32_t)mod->template get<double>() : 0;
        auto sz = f.find("size");
        size_t size = (sz != f.end() && sz->is_number()) ? (size_t)sz->template get<double>() : 0;
        root.add_path(KUtils::split(path, '/'), path, modified, size);
      }
    }

    for (auto &l : lidos) {
      Tree *t = root.find_node(KUtils::split(l.first, '/'));
      if (t != nullptr && t->is_leaf() && t->date_modified == l.second.modified) {
        t->set_metadata(l.second.metadata);
        t->metadata_requested = true;
        t->thumb_checked = l.second.thumb_checked;
        t->thumb_path = l.second.thumb_path;
        t->thumb_w = l.second.thumb_w;
        t->thumb_h = l.second.thumb_h;
      }
    }

    Tree *dir = root.find_path(KUtils::split(cur_path, '/'));
    // need to simply this using the directory endpoint
    cur_dir = dir;
    files_loaded = true;
    this->populate_files(d);
    refreshing_files = false;

    // "abrir_arquivo" que chegou antes da lista
    if (!requested_file.empty()) {
      std::string arquivo = requested_file;
      requested_file.clear();
      Tree *f = find_file(arquivo);
      if (f != nullptr) {
        show_file_detail(f);
      } else {
        KUtils::notify_toast("Este arquivo não existe mais.", 2500);
      }
    }

    if (refresh_pending) {
      refresh_pending = false;
      subscribe();
    }
    });
}

void PrintPanel::foreground() {
  // A página Arquivos é destino da coluna: o gancho ao_mostrar atualiza a lista.
  Navegacao::ir_para(Pagina::ARQUIVOS);
}

// ---- Lista ---------------------------------------------------------------------

Tree *PrintPanel::find_file(const std::string &path) {
  if (path.empty()) {
    return nullptr;
  }
  Tree *t = root.find_node(KUtils::split(path, '/'));
  return (t != nullptr && t->is_leaf()) ? t : nullptr;
}

Tree *PrintPanel::usb_dir() {
  for (auto &e : root.children) {
    if (e.second.is_leaf()) {
      continue;
    }
    std::string n = minusculas(e.first);
    if (n.find("usb") != std::string::npos
        || n.find("udisk") != std::string::npos
        || n.find("sda") != std::string::npos) {
      return &e.second;
    }
  }
  return nullptr;
}

void PrintPanel::update_sort_label() {
  const char *txt = "Data " LV_SYMBOL_DOWN;
  if (sort_mode == SORTED_BY_MODIFIED) {
    txt = sort_reversed ? "Data " LV_SYMBOL_DOWN : "Data " LV_SYMBOL_UP;
  } else {
    txt = sort_reversed ? "Nome " LV_SYMBOL_UP : "Nome " LV_SYMBOL_DOWN;
  }
  Marca::texto(lv_obj_get_child(sort_btn, 0), txt);
}

void PrintPanel::show_dir(Tree *dir) {
  cur_dir = dir;
  show_list();
}

void PrintPanel::show_list() {
  entries.clear();
  empty_text.clear();

  if (tab == TAB_HISTORY) {
    if (history_jobs.empty()) {
      empty_text = history_loading ? "Carregando..." : "Nenhuma impressão ainda";
    } else {
      for (int i = 0; i < (int)history_jobs.size(); i++) {
        entries.push_back(Entry{EntryType::HISTORY, nullptr, i});
      }
    }
  } else {
    Tree *base = &root;
    if (tab == TAB_USB) {
      base = usb_dir();
      if (base == nullptr) {
        empty_text = files_loaded ? "Nenhum pendrive encontrado" : "Carregando...";
      } else if (cur_dir == nullptr
                 || (cur_dir != base && !comeca_com(cur_dir->full_path, base->full_path + "/"))) {
        cur_dir = base;
      }
    } else if (cur_dir == nullptr) {
      cur_dir = &root;
    }

    if (base != nullptr) {
      if (cur_dir != base && cur_dir->parent != nullptr && cur_dir->parent != cur_dir) {
        entries.push_back(Entry{EntryType::UP, cur_dir->parent, -1});
      }

      // ordena ponteiros (antes copiava a árvore inteira a cada vez):
      // pastas primeiro, depois pela data ou pelo nome, como hoje
      std::vector<Tree *> ordem;
      ordem.reserve(cur_dir->children.size());
      for (auto &e : cur_dir->children) {
        ordem.push_back(&e.second);
      }
      bool por_data = sort_mode == SORTED_BY_MODIFIED;
      bool invertido = sort_reversed;
      std::sort(ordem.begin(), ordem.end(), [por_data, invertido](Tree *x, Tree *y) {
        if (x->is_leaf() && !y->is_leaf()) {
          return false;
        } else if (!x->is_leaf() && y->is_leaf()) {
          return true;
        }
        if (por_data) {
          return invertido ? x->date_modified > y->date_modified : y->date_modified > x->date_modified;
        }
        return invertido ? x->name > y->name : y->name > x->name;
      });

      for (Tree *t : ordem) {
        entries.push_back(Entry{t->is_leaf() ? EntryType::FILE : EntryType::FOLDER, t, -1});
      }

      if (entries.empty()) {
        empty_text = files_loaded ? "Nenhum arquivo ainda" : "Carregando...";
      }
    }
  }

  if (!empty_text.empty()) {
    entries.push_back(Entry{EntryType::EMPTY, nullptr, -1});
  }

  // ordenar não vale para o histórico (vem do mais novo)
  if (tab == TAB_HISTORY) {
    lv_obj_add_flag(sort_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(sort_btn, LV_OBJ_FLAG_HIDDEN);
  }

  show_page();
}

void PrintPanel::show_page() {
  int total = (int)entries.size();
  int paginas = std::max(1, (total + POR_PAGINA - 1) / POR_PAGINA);
  if (page >= paginas) {
    page = paginas - 1;
  }
  if (page < 0) {
    page = 0;
  }

  for (int i = 0; i < POR_PAGINA; i++) {
    int idx = page * POR_PAGINA + i;
    if (idx < total) {
      draw_card(i, entries[idx]);
      lv_obj_clear_flag(cards[i].card, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(cards[i].card, LV_OBJ_FLAG_HIDDEN);
    }
  }

  Marca::texto(page_label, fmt::format("{}/{}", page + 1, paginas).c_str());

  if (page > 0) {
    lv_obj_clear_state(page_up_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(page_up_btn, LV_STATE_DISABLED);
  }
  if (page + 1 < paginas) {
    lv_obj_clear_state(page_down_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(page_down_btn, LV_STATE_DISABLED);
  }
}

void PrintPanel::draw_card(int i, const Entry &e) {
  Card &c = cards[i];

  if (e.type == EntryType::EMPTY) {
    lv_obj_clear_flag(c.card, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_add_flag(c.card, LV_OBJ_FLAG_CLICKABLE);
  }

  bool mostrar_img = false;
  bool mostrar_selo = false;
  const char *icone = LV_SYMBOL_FILE;
  std::string nome;
  std::string meta;

  switch (e.type) {
  case EntryType::UP:
    icone = LV_SYMBOL_DIRECTORY;
    nome = "..";
    break;

  case EntryType::FOLDER:
    icone = LV_SYMBOL_DIRECTORY;
    nome = e.node->name;
    meta = data_curta(e.node->date_modified);
    break;

  case EntryType::FILE: {
    Tree *f = e.node;
    nome = nome_curto(f->name);
    std::vector<std::string> partes;
    if (f->has_metadata) {
      double eta = numero(campo(f->metadata, "estimated_time"));
      if (eta > 0) {
        partes.push_back(KUtils::tempo_curto((int64_t)eta));
      }
    }
    if (f->size > 0) {
      partes.push_back(KUtils::tamanho_arquivo(f->size));
    }
    partes.push_back(data_curta(f->date_modified));
    meta = juntar(partes);

    if (!f->thumb_path.empty() && f->thumb_w > 0 && f->thumb_h > 0) {
      std::string src = "A:" + f->thumb_path;
      if (c.img_src != src) {
        lv_img_set_src(c.img, src.c_str());
        c.img_src = src;
        size_t maior = std::max(f->thumb_w, f->thumb_h);
        uint32_t zoom = (uint32_t)(MINI_LISTA * 256 / maior);
        if (zoom == 0) zoom = 1;
        if (zoom > 1024) zoom = 1024;
        lv_img_set_zoom(c.img, (uint16_t)zoom);
        lv_obj_center(c.img);
      }
      mostrar_img = true;
    }

    // metadados só dos arquivos à vista, uma vez por arquivo
    if (!f->has_metadata && !f->metadata_requested) {
      request_list_metadata(f);
    }
    break;
  }

  case EntryType::HISTORY: {
    const HistoryJob &h = history_jobs[e.history];
    nome = nome_curto(h.filename);
    mostrar_selo = true;

    lv_obj_t *selo_txt = lv_obj_get_child(c.badge, 0);
    std::string palavra;
    if (h.status == "completed") {
      lv_obj_set_style_bg_color(c.badge, lv_color_hex(Marca::OK), 0);
      lv_obj_set_style_text_color(c.badge, lv_color_hex(Marca::PAPEL), 0);
      Marca::texto(selo_txt, LV_SYMBOL_OK);
      palavra = "Concluída";
    } else if (h.status == "in_progress") {
      lv_obj_set_style_bg_color(c.badge, lv_color_hex(Marca::LIMAO), 0);
      lv_obj_set_style_text_color(c.badge, lv_color_hex(Marca::PRETO), 0);
      Marca::texto(selo_txt, LV_SYMBOL_PLAY);
      palavra = "Imprimindo";
    } else if (h.status == "cancelled") {
      lv_obj_set_style_bg_color(c.badge, lv_color_hex(Marca::ERRO), 0);
      lv_obj_set_style_text_color(c.badge, lv_color_hex(Marca::PAPEL), 0);
      Marca::texto(selo_txt, LV_SYMBOL_CLOSE);
      palavra = "Cancelada";
    } else {
      // error, klippy_shutdown, klippy_disconnect, server_exit...
      lv_obj_set_style_bg_color(c.badge, lv_color_hex(Marca::ERRO), 0);
      lv_obj_set_style_text_color(c.badge, lv_color_hex(Marca::PAPEL), 0);
      Marca::texto(selo_txt, LV_SYMBOL_CLOSE);
      palavra = "Erro";
    }
    meta = juntar({palavra,
                   h.status == "in_progress" ? std::string() : KUtils::tempo_curto(h.duration),
                   data_curta(h.when)});
    break;
  }

  case EntryType::EMPTY:
    if (tab == TAB_USB) {
      icone = LV_SYMBOL_USB;
    } else if (tab == TAB_HISTORY) {
      icone = LV_SYMBOL_LIST;
    } else {
      icone = LV_SYMBOL_DIRECTORY;
    }
    nome = empty_text;
    break;
  }

  if (mostrar_img) {
    lv_obj_clear_flag(c.img, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(c.img, LV_OBJ_FLAG_HIDDEN);
  }
  if (mostrar_selo) {
    lv_obj_clear_flag(c.badge, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(c.badge, LV_OBJ_FLAG_HIDDEN);
  }
  if (mostrar_img || mostrar_selo) {
    lv_obj_add_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
  } else {
    Marca::texto(c.icon, icone);
    lv_obj_clear_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
  }

  Marca::texto(c.name, nome.c_str());
  Marca::texto(c.meta, meta.c_str());
}

void PrintPanel::request_list_metadata(Tree *f) {
  f->metadata_requested = true;
  std::string path = f->full_path;
  ws.send_jsonrpc("server.files.metadata", json{{"filename", path}}, [this, path](json &d) {
    // thread do websocket: escolher/baixar a miniatura fica fora do lv_lock
    bool tem = d.contains("result");
    Miniatura mini;
    if (tem) {
      mini = miniatura_lista(path, d);
    }

    std::lock_guard<std::mutex> lock(lv_lock);
    Tree *t = find_file(path);
    if (t == nullptr) {
      return;  // a lista mudou nesse meio tempo
    }
    if (tem) {
      if (!t->has_metadata) {
        t->set_metadata(d);
      }
      t->thumb_checked = true;
      t->thumb_path = mini.first;
      t->thumb_w = mini.second.first;
      t->thumb_h = mini.second.second;
    }
    // sem metadados (ex.: arquivo do pendrive ainda não lido): fica o ícone e
    // não pede de novo; os Detalhes pedem a leitura (metascan) se for aberto

    if (tab == TAB_HISTORY) {
      return;
    }
    int ini = page * POR_PAGINA;
    for (int k = ini; k < ini + POR_PAGINA && k < (int)entries.size(); k++) {
      if (entries[k].node == t) {
        draw_card(k - ini, entries[k]);
        break;
      }
    }
  });
}

void PrintPanel::request_history() {
  history_loading = true;
  json params = {{"limit", 30}, {"order", "desc"}};
  ws.send_jsonrpc("server.history.list", params, [this](json &d) {
    // thread do websocket: lê a resposta fora do lv_lock
    std::vector<HistoryJob> jobs;
    auto r = d.find("result");
    if (r != d.end() && r->is_object()) {
      auto lista = r->find("jobs");
      if (lista != r->end() && lista->is_array()) {
        for (const auto &job : *lista) {
          if (!job.is_object()) {
            continue;
          }
          auto fn = job.find("filename");
          if (fn == job.end() || !fn->is_string()) {
            continue;
          }
          HistoryJob h;
          h.filename = fn->template get<std::string>();
          auto st = job.find("status");
          h.status = (st != job.end() && st->is_string()) ? st->template get<std::string>() : "";
          double dur = 0;
          auto pd = job.find("print_duration");
          if (pd != job.end() && pd->is_number()) {
            dur = pd->template get<double>();
          }
          if (dur <= 0) {
            auto td = job.find("total_duration");
            if (td != job.end() && td->is_number()) {
              dur = td->template get<double>();
            }
          }
          h.duration = (int64_t)dur;
          h.when = 0;
          auto fim = job.find("end_time");
          if (fim != job.end() && fim->is_number()) {
            h.when = fim->template get<double>();
          }
          if (h.when <= 0) {
            auto ini = job.find("start_time");
            if (ini != job.end() && ini->is_number()) {
              h.when = ini->template get<double>();
            }
          }
          jobs.push_back(std::move(h));
        }
      }
    }

    std::lock_guard<std::mutex> lock(lv_lock);
    history_jobs = std::move(jobs);
    history_loading = false;
    if (tab == TAB_HISTORY) {
      show_list();
    }
  });
}

void PrintPanel::handle_tabs(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
    return;
  }
  uint16_t sel = lv_btnmatrix_get_selected_btn(tabs);
  if (sel == LV_BTNMATRIX_BTN_NONE || sel > TAB_HISTORY) {
    return;
  }
  tab = sel;
  page = 0;
  if (tab == TAB_LOCAL) {
    cur_dir = &root;
  } else if (tab == TAB_USB) {
    Tree *usb = usb_dir();
    if (usb != nullptr) {
      cur_dir = usb;
    }
  } else {
    request_history();
  }
  show_list();
}

void PrintPanel::handle_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *alvo = lv_event_get_current_target(e);
  int idx = -1;
  for (int i = 0; i < POR_PAGINA; i++) {
    if (cards[i].card == alvo) {
      idx = page * POR_PAGINA + i;
      break;
    }
  }
  if (idx < 0 || idx >= (int)entries.size()) {
    return;
  }

  Entry en = entries[idx];
  switch (en.type) {
  case EntryType::UP:
  case EntryType::FOLDER:
    if (en.node != nullptr) {
      page = 0;
      show_dir(en.node);
    }
    break;

  case EntryType::FILE:
    show_file_detail(en.node);
    break;

  case EntryType::HISTORY: {
    if (en.history < 0 || en.history >= (int)history_jobs.size()) {
      return;
    }
    Tree *f = find_file(history_jobs[en.history].filename);
    if (f != nullptr) {
      show_file_detail(f);
    } else {
      KUtils::notify_toast("Este arquivo não existe mais.", 2500);
    }
    break;
  }

  case EntryType::EMPTY:
    break;
  }
}

void PrintPanel::handle_btns(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == sort_btn) {
    // mesma ordenação de hoje, num botão só:
    // Data (mais novo primeiro) -> Data (mais antigo) -> Nome (A-Z) -> Nome (Z-A)
    if (sort_mode == SORTED_BY_MODIFIED) {
      if (sort_reversed) {
        sort_reversed = false;
      } else {
        sort_mode = SORTED_BY_NAME;
        sort_reversed = false;
      }
    } else {
      if (!sort_reversed) {
        sort_reversed = true;
      } else {
        sort_mode = SORTED_BY_MODIFIED;
        sort_reversed = true;
      }
    }
    update_sort_label();
    page = 0;
    show_list();

  } else if (btn == page_up_btn) {
    if (page > 0) {
      page--;
      show_page();
    }

  } else if (btn == page_down_btn) {
    if ((page + 1) * POR_PAGINA < (int)entries.size()) {
      page++;
      show_page();
    }
  }
}

// ---- Detalhes do arquivo ---------------------------------------------------

void PrintPanel::open_file(const std::string &path) {
  // coluna em Arquivos (o gancho já pede a lista nova ao Moonraker)
  Navegacao::ir_para(Pagina::ARQUIVOS);
  Tree *f = find_file(path);
  if (f != nullptr) {
    requested_file.clear();
    show_file_detail(f);
    return;
  }
  if (refreshing_files || !files_loaded) {
    requested_file = path;  // abre quando a lista chegar (em subscribe)
    return;
  }
  KUtils::notify_toast("Este arquivo não existe mais.", 2500);
}

void PrintPanel::show_file_detail(Tree *f) {
  if (f == nullptr || !f->is_leaf()) return;

  file_panel.show_basic(f->full_path, f->size, f->date_modified);
  if (f->contains_metadata()) {
    file_panel.refresh_view(f->metadata, f->full_path);
  } else {
    spdlog::trace("getting metadata for {}", f->name);
    f->metadata_requested = true;
    std::string path = f->full_path;

    // a resposta chega na thread do websocket: miniaturas fora do lv_lock,
    // tela e árvore dentro dele
    auto aplicar = [this, path](json &dados) {
      Miniatura det = miniatura_detalhe(path, dados);
      Miniatura lista = miniatura_lista(path, dados);

      std::lock_guard<std::mutex> lock(lv_lock);
      Tree *t = find_file(path);
      if (t != nullptr) {
        if (!t->has_metadata) {
          t->set_metadata(dados);
        }
        if (!t->thumb_checked) {
          t->thumb_checked = true;
          t->thumb_path = lista.first;
          t->thumb_w = lista.second.first;
          t->thumb_h = lista.second.second;
        }
      }
      if (file_panel.get_gcode_path() != path) return;
      file_panel.refresh_view(dados, path, det);
    };

    ws.send_jsonrpc("server.files.metadata",
      json{{"filename", path}},
      [path, aplicar, this](json &d) {
        if (d.contains("result")) {
          aplicar(d);
        } else {
          // File not in Moonraker's metadata DB (normal for USB files).
          // metascan forces a parse + thumbnail extraction and returns
          // the same data shape as metadata — pipe it straight through.
          spdlog::info("metadata not cached for {}, requesting metascan", path);
          ws.send_jsonrpc("server.files.metascan",
            json{{"filename", path}},
            [path, aplicar, this](json &d2) {
              if (d2.contains("result")) {
                aplicar(d2);
              } else {
                std::lock_guard<std::mutex> lock(lv_lock);
                if (file_panel.get_gcode_path() != path) return;
                spdlog::warn("metascan failed for {}", path);
                file_panel.show_no_metadata();
              }
            });
        }
      });
  }

  file_panel.foreground();
}

void PrintPanel::handle_print_callback(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code != LV_EVENT_CLICKED) {
    return;
  }
  const std::string arquivo = file_panel.get_gcode_path();
  if (arquivo.empty()) {
    return;
  }
  Tree *cur_file = find_file(arquivo);
  if (cur_file == nullptr) {
    KUtils::notify_toast("Este arquivo não existe mais.", 2500);
    return;
  }

  json pstat_state = State::get_instance()
    ->get_data("/printer_state/print_stats/state"_json_pointer);
  spdlog::debug("print panel print stats {}",
    pstat_state.is_null() ? "nil" : pstat_state.template get<std::string>());

  if (!pstat_state.is_null()
    && pstat_state.template get<std::string>() != "printing"
    && pstat_state.template get<std::string>() != "paused") {

    // ws.send_jsonrpc("printer.gcode.script",
    // 		    json::parse(R"({"script":"PRINT_PREPARE_CLEAR"})"));

    pending_print_path = cur_file->full_path;

    // filament required by this print, from the cached gcode metadata
    pending_needed_g = -1;
    pending_needed_mm = -1;
    if (cur_file->contains_metadata()) {
      const json *w = campo(cur_file->metadata, "filament_weight_total");
      if (w != nullptr && w->is_number()) {
        pending_needed_g = w->template get<double>();
      }
      const json *l = campo(cur_file->metadata, "filament_total");
      if (l != nullptr && l->is_number()) {
        pending_needed_mm = l->template get<double>();
      }
    }

    if (spoolman_enabled) {
      // ask whether to keep the currently selected spool before printing
      show_filament_dialog();
    } else {
      start_pending_print();
    }

  } else {
    KUtils::notify_toast("Já há uma impressão em andamento.", 2500);
  }
}

void PrintPanel::start_pending_print() {
  if (pending_print_path.empty()) {
    return;
  }
  // Outra impressão começou (Orca, Mainsail...) enquanto a pergunta do
  // filamento estava aberta: não pede nada e não zera o acompanhamento dela.
  if (KUtils::is_printing()) {
    pending_print_path.clear();
    KUtils::notify_toast("Já há uma impressão em andamento.", 2500);
    return;
  }
  spdlog::debug("printer ready to print. print file {}", pending_print_path);
  json fname_input = {{"filename", pending_print_path}};
  ws.send_jsonrpc("printer.print.start", fname_input, [this](json &r) {
    // thread do websocket. Se a impressora recusou (Klipper em erro, arquivo
    // apagado...), fecha o acompanhamento que abriu na hora e avisa.
    if (!r.contains("error")) {
      return;
    }
    std::lock_guard<std::mutex> lock(lv_lock);
    if (!KUtils::is_printing()) {
      print_status.background();
    } else {
      // recusou porque já havia outra impressão: o reset() abaixo apagou os
      // dados dela da tela; lê de novo (só leitura, nenhum comando)
      print_status.reset();
      print_status.populate();
    }
    KUtils::notify_toast("Não foi possível iniciar a impressão.", 3000);
  });
  pending_print_path.clear();
  // Acompanhamento na frente já, mas limpo: sem os números da impressão
  // anterior até chegarem os da nova (quando o estado "printing" chegar, a
  // segunda abertura não muda nada).
  print_status.reset();
  Navegacao::abrir_acompanhamento();
}

void PrintPanel::show_filament_dialog() {
  if (pending_print_path.empty()) {
    return;  // o pedido foi esquecido (outra impressão começou)
  }
  json spool = sm.get_active_spool();

  if (spool.is_null()) {
    lv_label_set_text(filament_title_label, "Sem carretel ativo. Imprimir mesmo assim?");
    lv_obj_add_flag(filament_swatch, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(filament_name_label, "");
    lv_label_set_text(filament_detail_label, "");
    lv_obj_set_style_bg_opa(filament_enough_label, LV_OPA_TRANSP, 0);
    lv_label_set_text(filament_enough_label, "");
  } else {
    lv_label_set_text(filament_title_label, "Usar o mesmo filamento?");
    // name: "Vendor - Name"
    auto vendor_json = spool["/filament/vendor/name"_json_pointer];
    auto vendor = !vendor_json.is_null() ? vendor_json.template get<std::string>() : "";
    auto name_json = spool["/filament/name"_json_pointer];
    auto name = !name_json.is_null() ? name_json.template get<std::string>() : "";
    lv_label_set_text(filament_name_label, fmt::format("{} - {}", vendor, name).c_str());

    // color swatch from color_hex ("RRGGBB")
    auto color_json = spool["/filament/color_hex"_json_pointer];
    if (!color_json.is_null()) {
      uint32_t rgb = static_cast<uint32_t>(strtol(color_json.template get<std::string>().c_str(), NULL, 16));
      lv_obj_set_style_bg_color(filament_swatch, lv_color_hex(rgb), 0);
      lv_obj_clear_flag(filament_swatch, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(filament_swatch, LV_OBJ_FLAG_HIDDEN);
    }

    // material + remaining weight
    auto material_json = spool["/filament/material"_json_pointer];
    auto material = !material_json.is_null() ? material_json.template get<std::string>() : "?";
    auto remaining_json = spool["/remaining_weight"_json_pointer];
    if (!remaining_json.is_null()) {
      lv_label_set_text(filament_detail_label,
        fmt::format("{}, restam {:.0f} g", material, remaining_json.template get<double>()).c_str());
    } else {
      lv_label_set_text(filament_detail_label, material.c_str());
    }

    // enough for this print? compare grams first, fall back to length
    std::string verdict;
    bool known = false, enough = false;
    if (pending_needed_g > 0 && !remaining_json.is_null()) {
      known = true;
      enough = remaining_json.template get<double>() >= pending_needed_g;
      verdict = fmt::format("A impressão usa {:.0f} g  {}", pending_needed_g,
        enough ? LV_SYMBOL_OK " suficiente" : LV_SYMBOL_CLOSE " insuficiente");
    } else {
      auto rem_len_json = spool["/remaining_length"_json_pointer];
      if (pending_needed_mm > 0 && !rem_len_json.is_null()) {
        known = true;
        enough = rem_len_json.template get<double>() >= pending_needed_mm;
        verdict = fmt::format("A impressão usa {:.1f} m  {}", pending_needed_mm / 1000.0,
          enough ? LV_SYMBOL_OK " suficiente" : LV_SYMBOL_CLOSE " insuficiente");
      }
    }
    if (known) {
      // estado com fundo (OK / ERRO) e texto papel, sempre com palavra
      lv_obj_set_style_bg_color(filament_enough_label, lv_color_hex(enough ? Marca::OK : Marca::ERRO), 0);
      lv_obj_set_style_bg_opa(filament_enough_label, LV_OPA_COVER, 0);
      lv_obj_set_style_text_color(filament_enough_label, lv_color_hex(Marca::PAPEL), 0);
      lv_label_set_text(filament_enough_label, verdict.c_str());
    } else {
      lv_obj_set_style_bg_opa(filament_enough_label, LV_OPA_TRANSP, 0);
      lv_obj_set_style_text_color(filament_enough_label, lv_color_hex(Marca::GRAFITE), 0);
      lv_label_set_text(filament_enough_label, "Consumo de filamento desconhecido");
    }
  }

  lv_obj_clear_flag(filament_cont, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(filament_cont);
}

void PrintPanel::handle_filament_btns(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  lv_obj_t *btn = lv_event_get_current_target(event);

  // dismiss the dialog regardless of choice
  lv_obj_add_flag(filament_cont, LV_OBJ_FLAG_HIDDEN);

  if (btn == filament_yes_btn) {
    start_pending_print();
  } else if (btn == filament_no_btn) {
    // open Spoolman; once a spool is chosen there, re-show this dialog with the
    // newly selected filament so the user confirms before the print starts.
    sm.request_select_for_print([this]() { this->show_filament_dialog(); });
    sm.foreground();
  }
}

void PrintPanel::enable_spoolman() {
  spoolman_enabled = true;
}
