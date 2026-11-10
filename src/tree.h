#ifndef __TREE_H__
#define __TREE_H__

#include <cstddef>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>

#include "hv/json.hpp"
#include "spdlog/spdlog.h"

using json = nlohmann::json;

struct Tree {
Tree(const std::string &filename, const std::string &path, uint32_t modified)
    : name(filename)
    , full_path(path)
    , date_modified(modified)
    , has_metadata(false)
    , parent(this) {
    // spdlog::debug("creating new node {}, {}", name, path);
  }

  ~Tree() {
    children.clear();
    // delete file_panel;
  }

  bool is_leaf() const {
    // this is just wrong, a folder without files is a still a
    // folder. lucky the files.list endpoint doesn't return empty
    // folders
    return children.empty();
  }

  Tree& find_or_create(const std::string &value, const std::string &path, uint32_t modified) {
    if (modified > date_modified) {
      date_modified = modified;
    }

    const auto &entry = children.find(value);
    if (entry != children.cend()) {
      return entry->second;
    }

    children.insert({value, Tree(value, path, modified)});
    Tree &child = children.find(value)->second;
    child.parent = this;
    return child;
  }

  void add_path(const std::vector<std::string>& paths, const std::string &path, uint32_t modified) {
    add_path(paths, path, modified, 0);
  }

  // Igual ao de cima, e guarda o tamanho (bytes) no arquivo (a última parte).
  void add_path(const std::vector<std::string>& paths, const std::string &path, uint32_t modified,
                size_t file_size) {
    if (paths.empty()) {
      return;
    }

    Tree *cur_node = this;
    std::string cur_path;
    for (const auto &p : paths) {
      if (cur_path.length() > 0) {
	cur_path = fmt::format("{}/{}", cur_path, p);
      } else {
	cur_path = p;
      }

      cur_node = &(cur_node->find_or_create(p, cur_path, modified));
    }
    cur_node->size = file_size;
  }

  Tree *find_path(const std::vector<std::string>& paths) {
    if (paths.empty()) {
      return this;
    }

    Tree *cur_node = this;
    for (const auto &p : paths) {
      const auto &entry = cur_node->children.find(p);
      if (entry != cur_node->children.cend()) {
	cur_node = &entry->second;
      } else {
	return this;
      }
    }
    return cur_node->is_leaf() ? this : cur_node;
  }

  // Acha o nó exato (arquivo ou pasta) pelo caminho; nullptr se não existe.
  Tree *find_node(const std::vector<std::string>& paths) {
    if (paths.empty()) {
      return nullptr;
    }

    Tree *cur_node = this;
    for (const auto &p : paths) {
      const auto &entry = cur_node->children.find(p);
      if (entry == cur_node->children.cend()) {
        return nullptr;
      }
      cur_node = &entry->second;
    }
    return cur_node;
  }

  // Chama f(nó) para todos os nós abaixo deste (não inclui este).
  template <typename F>
  void for_each_node(F f) {
    for (auto &e : children) {
      f(e.second);
      e.second.for_each_node(f);
    }
  }

  Tree *get_child(const std::string child) {
    const auto &e = children.find(child);
    if (e != children.cend()) {
      // spdlog::debug("get_child {}, result = {}", child, e->second.name);
      return &e->second;
    }

    return NULL;
  }

  void set_name(const std::string &n) {
    name = n;
  }

  void traverse() const {
    // spdlog::debug("%s%s", name, is_leaf() ? " *": "");
    if (!is_leaf()) {
      for (auto e = children.cbegin(); e != children.cend(); ++e) {
	e->second.traverse();
      }
    }
  }

  bool contains_metadata() {
    return has_metadata;
  }

  void set_metadata(json &j) {
    has_metadata = true;
    metadata = j;
  }

  void clear() {
    children.clear();
  }

  const char* get_thumbpath() {
    if (metadata.contains("result") && metadata["result"].contains("thumbnails")) {
      // XXX: fix my index
      return metadata["result"]["thumbnails"][1]["relative_path"].template get<std::string>().c_str();
    }
    return NULL;
  }

  std::string name;
  std::string full_path;
  uint32_t date_modified;
  json metadata;
  bool has_metadata;
  Tree *parent;
  std::map<std::string, Tree> children;

  // Lista de arquivos (Proposta B): só os arquivos que já apareceram na tela
  // pedem metadados; a miniatura pequena (<= 160 px) fica guardada aqui.
  size_t size = 0;                 // bytes (do server.files.list)
  bool metadata_requested = false; // já pediu server.files.metadata (não pede de novo)
  bool thumb_checked = false;      // já escolheu a miniatura da lista
  std::string thumb_path;          // vazio = sem miniatura pequena (fica o ícone)
  size_t thumb_w = 0;
  size_t thumb_h = 0;
};

#endif // __TREE_H__
