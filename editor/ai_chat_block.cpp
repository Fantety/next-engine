/*
 * @FilePath: \editor\ai_chat_block.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-30 14:47:07
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-04 18:23:51
 */
#include "ai_chat_block.h"

void AIChatBlock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_text", "text"), &AIChatBlock::set_text);
    ClassDB::bind_method(D_METHOD("get_text"), &AIChatBlock::get_text);
    ClassDB::bind_method(D_METHOD("to_bbcode"), &AIChatBlock::to_bbcode);
}

AIChatBlock::AIChatBlock(AIChatBlock::ChatType i_chat_type) {
    this->chat_type = i_chat_type;
    theme = get_theme();
    chat_content = memnew(RichTextLabel);
    chat_content->set_use_bbcode(true);
    add_child(chat_content);
    chat_content->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    chat_content->set_scroll_follow(true);
    chat_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    switch (this->chat_type)
    {
    case ChatType::AI_CHAT_TYPE_USER:
        change_panel_color(Color(0.0, 78/255, 100/255));//rgb(0,78,100)
        break;
    case ChatType::AI_CHAT_TYPE_ASSISTANT:
        change_panel_color(Color(30/255, 55/255, 153/255));//rgb(30, 55, 153)
        break;
    case ChatType::AI_CHAT_SYS_MESSAGE:
        change_panel_color(Color(48/255, 57/255, 82/255));//rgb(48, 57, 82)
        break;
    case ChatType::AI_CHAT_SYS_WARNING:
        change_panel_color(Color(229/255, 142/255, 38/255));//rgb(229, 142, 38)
        break;
    case ChatType::AI_CHAT_SYS_ERROR:
        change_panel_color(Color(183/255, 21/255, 64/255));//rgb(183, 21, 64)
        break;
    default:
        break;
    }
}

String AIChatBlock::get_text(){
    return chat_content->get_text();
}

void AIChatBlock::set_text(const String &p_text) {
    chat_content->set_text(p_text);
}
void AIChatBlock::add_text(const String &p_text) {
    chat_content->add_text(p_text);
    mark_text+=p_text;
}

void AIChatBlock::set_fit_content(bool fit){
    chat_content->set_fit_content(fit);
}

void AIChatBlock::to_bbcode(){
    String bbcode = mark_text;
    chat_content->set_text(bbcode);
}

void AIChatBlock::set_chat_type(AIChatBlock::ChatType type) {
    chat_type = type;
}
AIChatBlock::ChatType AIChatBlock::get_chat_type() {
    return chat_type;
}


void AIChatBlock::change_panel_color(const Color &new_color){
    Ref<StyleBox> style = this->get_theme_stylebox("panel");
    // 检查是否是 StyleBoxFlat（可修改颜色的类型）
    if (style.is_valid() && style->is_class("StyleBoxFlat")) {
        // 转换为 StyleBoxFlat 并修改颜色
        Ref<StyleBoxFlat> style_flat = style;
        style_flat->set_bg_color(new_color);
        // 重新应用修改后的 StyleBox
        this->add_theme_style_override("panel", style_flat);
    }
}