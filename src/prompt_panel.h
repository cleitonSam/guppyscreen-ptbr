#ifndef __PROMPT_PANEL_H__
#define __PROMPT_PANEL_H__

#include "lvgl/lvgl.h"
#include "websocket_client.h"
#include "notify_consumer.h"
#include "button_container.h"

#include <map>
#include <memory>
#include <mutex>

class PromptPanel : public NotifyConsumer {
    public:
        PromptPanel(KWebSocketClient &ws, std::mutex &lock, lv_obj_t *parent);
        ~PromptPanel();
        void consume(json &j);
        void foreground();
        void background();
        // True while a Klipper action:prompt is on screen (between prompt_show
        // and prompt_end) - lets other panels avoid popping their own dialog on
        // top of one Klipper's own macros are already showing.
        bool is_visible() const;

    private:
        static void on_button_click(lv_event_t *e);
        // Libera o texto do comando guardado no botão quando o botão é apagado
        // (prompt_end limpa as fileiras com lv_obj_clean).
        static void on_button_delete(lv_event_t *e);

        KWebSocketClient &ws;
        // Janela em lv_layer_top(): o acompanhamento automático e a navegação
        // nunca a escondem. Cobre a área de conteúdo (56,0 424x272) e deixa a
        // coluna da esquerda (com o botão de parada) à vista.
        lv_obj_t *prompt_cont;
        lv_obj_t *header;
        lv_obj_t *body_cont;
        lv_obj_t *body;
        lv_obj_t *button_cont;
        lv_obj_t *footer_cont;
};

#endif // __PROMPT_PANEL_H__
