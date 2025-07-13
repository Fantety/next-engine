/*
 * @FilePath: \editor\ai_component\ai_dock.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-12 16:46:47
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
#include "core/io/json.h"
#include "apis/deepseek_api.h"
#include "ai_chat_manager.h"
#include "ai_chat_block.h"
#include "ai_chat_panel.h"
#include "ai_accept_dialog.h"
#include "ai_history_button.h"
#include "ai_ide_interface.h"


class AIDock : public TabContainer {
    GDCLASS(AIDock, TabContainer);
private:

    VBoxContainer *history_view = nullptr;
    VBoxContainer *history_list = nullptr;
    ScrollContainer *chat_scroll = nullptr;
    VBoxContainer *chat_view = nullptr;
    VBoxContainer *chat_list = nullptr;
    AIChatPanel* history_input_panel = nullptr;
    AIChatPanel* chat_input_panel = nullptr;
    DeepSeekAPI *deepseek_api = nullptr;
    String current_ai_response;
    int current_chat_index = -1;
    int chat_sum = 0;
    AIAcceptDialog* accept_dialog = nullptr;
    Vector<AIChatBlock*> chat_blocks;
    Vector<AIHistoryButton*> history_buttons;
    String current_chat_uid;
    AIChatManager chat_manager;
    AIIDEInterface *ide_interface = nullptr;
    static inline AIDock *singleton = nullptr;
    int retry_chat_type = 0;
    int retry_block_index = 0;

    bool block_create_flag = true;
    void _send_message();
    void _retry_message();
    void _add_message(const String &message, int block_index);
    void _add_reason_message(const String &message, int block_index);
    void _create_chat_block(AIChatBlock::ChatType chat_type, const String& message);
private:
    void delete_all_blocks();
    void on_stream_response(String text);
    void on_reason_response(String text);
    void on_tool_response(Array tools);
    void on_streaming_response(Dictionary dict, String finish_reason);
    void on_data_start();
    void on_request_completed(int chat_flag);
    void on_retry_pressed(int chat_type, int block_index);
    void on_history_button_pressed(String uuid);


    void confirm_retry();
    void load_historys();
    void delete_blocks_after_index(int index);
    String generate_uuid() const;

protected:
    void _notification(int p_notification);
    static void _bind_methods();

public:
    AIDock();
    static AIDock* get_singleton();
};

#endif // AI_DOCK_H