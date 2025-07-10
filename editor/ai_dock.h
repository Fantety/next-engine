/*
 * @FilePath: \editor\ai_dock.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-04 18:32:07
 */
#ifndef AI_DOCK_H
#define AI_DOCK_H

#include "scene/gui/box_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/item_list.h"
#include "scene/gui/button.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/panel_container.h"
#include "ai_settings_dialog.h"
#include "chatapis/deepseek_api.h"
#include "chatapis/ai_chat_manager.h"
#include "ai_chat_block.h"
#include "core/io/json.h"
#include "ai_chat_panel.h"
class AIDock : public TabContainer {
    GDCLASS(AIDock, TabContainer);
    String generate_uuid() const;

private:
    // 历史视图组件
    VBoxContainer *history_view;
    ItemList *history_list;
    ScrollContainer *chat_scroll;
    VBoxContainer *chat_view;
    VBoxContainer *chat_list;
    AIChatPanel* history_input_panel;
    AIChatPanel* chat_input_panel;
    AISettingsDialog *dialog;
    DeepSeekAPI *deepseek_api;
    String current_ai_response;
    int current_chat_index = -1;
    int chat_sum = 0;

    Vector<AIChatBlock*> chat_blocks;

    String current_chat_uid;
    AIChatManager chat_manager;


    void _send_message();
    void _add_message(const String &message, int block_index);
    void _add_reason_message(const String &message, int block_index);
    void _create_chat_block(AIChatBlock::ChatType chat_type, const String& message);
    private:
    void delete_all_blocks();
    void on_stream_response(String text);
    void on_reason_response(String text);
    void on_data_start();
    void on_request_completed();

protected:
    void _notification(int p_notification);
    static void _bind_methods();

public:
    AIDock();
    void set_ai_settings_dialog(AISettingsDialog *i_dialog);
};

#endif // AI_DOCK_H