/*
 * @FilePath: \editor\ai_chat_panel.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-29 15:06:20
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-04 22:29:35
 */
#ifndef AI_CHAT_PANEL_H
#define AI_CHAT_PANEL_H

#include "scene/gui/box_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/item_list.h"
#include "scene/gui/button.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/label.h"

class AIChatPanel:public PanelContainer{
    GDCLASS(AIChatPanel, PanelContainer);
    VBoxContainer *input_view;
    HBoxContainer* input_bottom_bar;
    TextEdit *input_box;
    OptionButton *model_selector;
    Button *send_button;
    HBoxContainer *selector_container;
    Label *model_label;
public:
    AIChatPanel();
protected:
    static void _bind_methods();

public:
    void _update_model_list();
    String get_input_text();
    String get_model();
    void clear_text();
    void set_button_enabled(bool enabled);
    void on_send_button_pressed();
};

#endif