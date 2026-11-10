#ifndef __PRINTER_SELECT_PANEL_H__
#define __PRINTER_SELECT_PANEL_H__

#include "lvgl/lvgl.h"

#include <string>
#include <map>
#include <memory>

class PrinterSelectPanel;

// Uma impressora da lista: nome (f14), "IP: ... · Porta: ..." (f12 GRAFITE),
// "Trocar" e o X de excluir (cada um com a sua pergunta).
class PrinterSelectContainer {
 public:
  PrinterSelectContainer(PrinterSelectPanel &ps,
			 lv_obj_t *parent,
			 const std::string &name,
			 const std::string &ip,
			 uint32_t port);

  ~PrinterSelectContainer();

  lv_obj_t *prompt(const std::string &prompt_text);

 private:
  PrinterSelectPanel &printer_select_panel;
  lv_obj_t *cont;
  std::string name;
};


// Subtela "Impressoras" (Configurações › Sistema › Impressoras): campos para
// acrescentar uma impressora (nome, IP e porta do Moonraker) e a lista das
// impressoras salvas. O teclado fica dentro da subtela.
class PrinterSelectPanel {
 public:
  PrinterSelectPanel();
  ~PrinterSelectPanel();

  void handle_input(lv_event_t *);
  void remove_printer(const std::string &n);
  void add_printer(const std::string &n,
		   const std::string &ip,
		   uint32_t port);

  void foreground();

 private:
  void fechar_teclado();

  lv_obj_t *cont;
  lv_obj_t *printer_name;
  lv_obj_t *moonraker_ip;
  lv_obj_t *moonraker_port;
  /* lv_obj_t *moonraker_apikey; */
  lv_obj_t *add_btn;
  lv_obj_t *right;   // lista das impressoras salvas
  lv_obj_t *kb;

  std::map<std::string, std::shared_ptr<PrinterSelectContainer>> printers;
};

#endif // __PRINTER_SELECT_PANEL_H__
