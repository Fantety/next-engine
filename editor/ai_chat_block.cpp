
#include "ai_chat_block.h"

void AIChatBlock::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            switch (this->chat_type)
            {
            case ChatType::AI_CHAT_TYPE_USER:
                change_panel_color(Color(0.0, 0.3, 0.4));//rgb(0,78,100)
                break;
            case ChatType::AI_CHAT_TYPE_ASSISTANT:
                change_panel_color(Color(0.12, 0.22, 0.6));//hsl(228, 67.20%, 35.90%)
                break;
            case ChatType::AI_CHAT_SYS_MESSAGE:
                change_panel_color(Color(0.18, 0.22, 0.32));//rgb(48, 57, 82)
                break;
            case ChatType::AI_CHAT_SYS_WARNING:
                change_panel_color(Color(0.9, 0.56, 0.15));//rgb(229, 142, 38)
                break;
            case ChatType::AI_CHAT_SYS_ERROR:
                change_panel_color(Color(0.72, 0.08, 0.25));//rgb(183, 21, 64)
                break;
            default:
                break;
            }
        } break;
    }
}

void AIChatBlock::set_clipboard(){
    DisplayServer::get_singleton()->clipboard_set(chat_content->get_text());
}

void AIChatBlock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_text", "text"), &AIChatBlock::set_text);
    ClassDB::bind_method(D_METHOD("get_text"), &AIChatBlock::get_text);
    ClassDB::bind_method(D_METHOD("to_bbcode"), &AIChatBlock::to_bbcode);
}

AIChatBlock::AIChatBlock(AIChatBlock::ChatType i_chat_type) {
    this->chat_type = i_chat_type;
    theme = get_theme();
    v_container = memnew(VBoxContainer);
    h_container = memnew(HBoxContainer);
    chat_content = memnew(RichTextLabel);
    copy_button = memnew(Button);
    retry_button = memnew(Button);
    copy_button->connect("pressed", callable_mp(this, &AIChatBlock::set_clipboard));
    chat_content->set_use_bbcode(true);
    v_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    v_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    add_child(v_container);
    v_container->add_child(chat_content);
    v_container->add_child(h_container);
    h_container->add_child(copy_button);
    h_container->add_child(retry_button);
    h_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);

    copy_button->set_text("Copy");
    switch (chat_type)
    {
    case ChatType::AI_CHAT_TYPE_ASSISTANT:
        retry_button->set_text("Retry");
        break;
    case ChatType::AI_CHAT_TYPE_USER:
        retry_button->set_text("Edit");
        break;
    default:
        break;
    }
    

    chat_content->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    chat_content->set_scroll_follow(true);
    chat_content->set_fit_content(true);
    chat_content->set_selection_enabled(true);
    chat_content->set_drag_and_drop_selection_enabled(true);
    chat_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);


    
    
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
    if (style.is_valid()) {
        // 转换为 StyleBoxFlat 并修改颜色
        Ref<StyleBoxFlat> style_flat = style;
        style_flat->set_bg_color(new_color);
        style_flat->set_corner_radius_all(10);
        // 重新应用修改后的 StyleBox
        this->add_theme_style_override("panel", style_flat);
    }
}