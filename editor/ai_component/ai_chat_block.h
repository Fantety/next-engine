/*
 * @FilePath: \editor\ai_component\ai_chat_block.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-14 13:57:28
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-19 11:26:01
 */
/*
 * @FilePath: \editor\ai_component\ai_chat_block.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-14 13:57:28
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-18 17:51:52
 */
#ifndef AI_CHAT_BLOCK_H
#define AI_CHAT_BLOCK_H

#include "scene/gui/rich_text_label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/box_container.h"
#include "scene/resources/style_box_flat.h"
#include "scene/gui/button.h"
#include "servers/display_server.h"
#include "scene/gui/margin_container.h"


class AIChatBlock : public PanelContainer {
    GDCLASS(AIChatBlock, PanelContainer);
public:
    enum ChatType
    {
        AI_CHAT_TYPE_USER = 0,
        AI_CHAT_TYPE_ASSISTANT,
        AI_CHAT_SYS_MESSAGE,
        AI_CHAT_SYS_ERROR,
        AI_CHAT_SYS_WARNING,
        AI_CHAT_SYS_TOOL,
    };
private:
    Ref<Theme> theme;
    VBoxContainer* v_container = nullptr;
    HBoxContainer* h_container = nullptr;
    MarginContainer* margin_container = nullptr;
    Button* copy_button = nullptr;
    Button* retry_button = nullptr;
    Button* thinking_process_button = nullptr;

    AIChatBlock::ChatType chat_type = AIChatBlock::AI_CHAT_TYPE_USER;
    RichTextLabel *thought_content = nullptr;
    RichTextLabel *tool_content = nullptr;
    RichTextLabel *final_answer_content = nullptr;
    RichTextLabel *reason_content = nullptr;
    String mark_text;
    String bbcode_text;

    Ref<Texture2D> copy_icon;
    Ref<Texture2D> retry_icon;

    int index = -1;

    void set_clipboard();
    bool thinking_process_flag = false;
    void change_thinking_process_visible();
    void send_retry_pressed();


protected:
    static void _bind_methods();
    void _notification(int p_notification);


public:
    void to_bbcode();
    void set_reason_text(const String &p_text);
    void add_reason_text(const String &p_text);
    void set_text(const String &p_text);
    void add_text(const String &p_text);
    String get_text();
    void set_fit_content(bool fit);
    void set_chat_type(AIChatBlock::ChatType type);
    AIChatBlock::ChatType get_chat_type();
    void change_panel_color(const Color &new_color);
    int get_block_index();
    void set_block_index(int black_index);


    void set_tool_content(const String &p_text);
    void set_final_reason_content(const String &p_text);


    AIChatBlock(){};
    AIChatBlock(AIChatBlock::ChatType i_chat_type);
};



#endif