/*
 * @FilePath: \editor\ai_component\ai_dock.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-02 11:34:38
 */
#ifndef AI_DOCK_H
#define AI_DOCK_H

#include "editor/docks/editor_dock.h"

#include "scene/gui/box_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/item_list.h"
#include "scene/gui/button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/panel_container.h"
#include "core/io/json.h"
#include "apis/openai_request_handler.h"
#include "apis/ai_stream_processor.h"
#include "ai_chat_manager.h"
#include "ai_chat_block.h"
#include "ai_chat_panel.h"
#include "ai_accept_dialog.h"
#include "ai_history_button.h"
#include "engine_operator.h"
#include "mcp/mcp_http_server.h"


class AIDock : public EditorDock {
    GDCLASS(AIDock, EditorDock);
    enum MsgSendType{
        MSG_SEND_TYPE_NORMAL = 0,
        MSG_SEND_TYPE_TOOL,
        MSG_SEND_TYPE_RETRY,
    };
private:

    VBoxContainer *history_view = nullptr;
    VBoxContainer *history_list = nullptr;
    ScrollContainer *chat_scroll = nullptr;
    VBoxContainer *chat_view = nullptr;
    VBoxContainer *chat_list = nullptr;
    TabContainer *tab_container = nullptr;
    AIChatPanel* history_input_panel = nullptr;
    AIChatPanel* chat_input_panel = nullptr;

    OpenAIRequestHandler *openai_api = nullptr;
    MCPHttpServer *mcp_server = nullptr;
    CheckButton *mcp_toggle_button = nullptr;
    Ref<AIStreamProcessor> stream_processor;

    String current_ai_response;
    int current_chat_index = -1;
    int chat_sum = 0;
    AIAcceptDialog* accept_dialog = nullptr;
    Vector<AIChatBlock*> chat_blocks;
    AIChatBlock* current_chat_block = nullptr;
    Vector<AIHistoryButton*> history_buttons;
    String current_chat_uid;
    AIChatManager chat_manager;
    EngineOperator *engine_operator = nullptr;

    static inline AIDock *singleton = nullptr;
    
    int retry_chat_type = 0;
    int retry_block_index = 0;

    bool block_create_flag = true;
    void _send_message(MsgSendType send_type);
    void _send_normal_message();
    void _toggle_mcp_server(bool p_toggled);
    void _add_message(const String &message, int block_index);
    void _add_reason_message(const String &message, int block_index);
    void _create_chat_block(AIChatBlock::ChatType chat_type, const String& thought, const String& tool, const String& final_answer);
private:
    void delete_all_blocks();
    void on_streaming_response(String content, String finish_reason);
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
 
VARIANT_ENUM_CAST(AIDock::MsgSendType)

#endif // AI_DOCK_H
