/*
 * @FilePath: \editor\ai_dock.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-29 16:05:46
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
    String generate_uuid() const;
    GDCLASS(AIDock, TabContainer);

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

    // 新增数据结构
    struct ChatSession {
        String uuid;
        String model;
        String preview;
        uint64_t timestamp;
        Vector<String> messages;
    };
    Vector<ChatSession> sessions;
    Vector<AIChatBlock*> chat_blocks;
    String current_session;

    String current_chat_uid;
    AIChatManager chat_manager;



    void _send_message();
    void _add_message(const String &message, int block_index, bool is_user);  // 确保这个方法有声明
private:
    void _handle_ai_response(const String& content, bool isFinal);
    void _handle_request_completed();
    void _handle_error(const String& message);

    void on_stream_response(String text);
    void on_data_updated();
    void on_request_completed();

    void _start_new_chat();
    void _on_history_selected(int index);
    void _update_history_list();
protected:
    void _notification(int p_notification);
    static void _bind_methods();

public:
    AIDock();
    void set_ai_settings_dialog(AISettingsDialog *i_dialog);
};

#endif // AI_DOCK_H