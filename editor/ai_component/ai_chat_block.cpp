
#include "ai_chat_block.h"
#include "editor/themes/editor_scale.h"

void AIChatBlock::_notification(int p_what) {
    // switch (p_what) {
    //     case NOTIFICATION_THEME_CHANGED: {
            
    //     } break;
    // }
}

void AIChatBlock::set_clipboard(){
    DisplayServer::get_singleton()->clipboard_set(thought_content->get_text());
}

void AIChatBlock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_text", "text"), &AIChatBlock::set_text);
    ClassDB::bind_method(D_METHOD("get_text"), &AIChatBlock::get_text);
    ClassDB::bind_method(D_METHOD("to_bbcode"), &AIChatBlock::to_bbcode);
    ADD_SIGNAL(MethodInfo("retry_pressed", PropertyInfo(Variant::INT, "chat_type"), PropertyInfo(Variant::INT, "block_index")));
}

void AIChatBlock::send_retry_pressed(){
    call_deferred("emit_signal", SNAME("retry_pressed"), (int)this->chat_type, this->index);
}

AIChatBlock::AIChatBlock(AIChatBlock::ChatType i_chat_type) {
    this->chat_type = i_chat_type;
    theme = get_theme();
    v_container = memnew(VBoxContainer);
    h_container = memnew(HBoxContainer);
    thought_content = memnew(RichTextLabel);
    reason_content = memnew(RichTextLabel);
    tool_content = memnew(RichTextLabel);
    final_answer_content = memnew(RichTextLabel);
    thinking_process_button = memnew(Button);
    thinking_process_button->set_text("Thinking Process");
    thinking_process_button->connect("pressed", callable_mp(this, &AIChatBlock::change_thinking_process_visible));
    reason_content->set_visible(false);
    reason_content->add_theme_color_override(SceneStringName(font_color), Color(0.35, 0.43, 0.47));
    thought_content->set_visible(false);
    tool_content->set_visible(false);
    final_answer_content->set_visible(false);
    copy_button = memnew(Button);
    retry_button = memnew(Button);
    margin_container = memnew(MarginContainer);
    retry_button->connect("pressed", callable_mp(this, &AIChatBlock::send_retry_pressed));
    copy_button->connect("pressed", callable_mp(this, &AIChatBlock::set_clipboard));
    thought_content->set_use_bbcode(true);
    tool_content->set_use_bbcode(true);
    final_answer_content->set_use_bbcode(true);
    v_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    v_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    margin_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    margin_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    Size2 borders = Size2(7, 7) * EDSCALE;
	margin_container->add_theme_constant_override("margin_right", borders.width);
	margin_container->add_theme_constant_override("margin_top", borders.height);
	margin_container->add_theme_constant_override("margin_left", borders.width);
	margin_container->add_theme_constant_override("margin_bottom", borders.height);
    add_child(margin_container);
    margin_container->add_child(v_container);
    v_container->add_child(thinking_process_button);
    thinking_process_button->set_visible(false);
    v_container->add_child(reason_content);
    v_container->add_child(thought_content);
    v_container->add_child(tool_content);
    v_container->add_child(final_answer_content);
    v_container->add_child(h_container);
    h_container->add_child(copy_button);
    h_container->add_child(retry_button);
    h_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);

    copy_button->set_text("Copy");
    thought_content->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    thought_content->set_scroll_follow(true);
    thought_content->set_fit_content(true);
    thought_content->set_selection_enabled(true);
    thought_content->set_drag_and_drop_selection_enabled(true);
    thought_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    thought_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    tool_content->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    tool_content->set_scroll_follow(true);
    tool_content->set_fit_content(true);
    tool_content->set_selection_enabled(true);
    tool_content->set_drag_and_drop_selection_enabled(true);
    tool_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    tool_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    final_answer_content->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    final_answer_content->set_scroll_follow(true);
    final_answer_content->set_fit_content(true);
    final_answer_content->set_selection_enabled(true);
    final_answer_content->set_drag_and_drop_selection_enabled(true);
    final_answer_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    final_answer_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    switch (this->chat_type)
    {
        case ChatType::AI_CHAT_TYPE_USER:
            change_panel_color(Color(0.0, 0.3, 0.4));//rgb(0,78,100)
            retry_button->set_text("Edit");
            break;
        case ChatType::AI_CHAT_TYPE_ASSISTANT:
            change_panel_color(Color(0.12, 0.22, 0.6));//hsl(228, 67.20%, 35.90%)
            retry_button->set_text("Retry");
            break;
        case ChatType::AI_CHAT_SYS_MESSAGE:
            change_panel_color(Color(0.18, 0.22, 0.32));//rgb(48, 57, 82)
            retry_button->set_visible(false);
            copy_button->set_visible(false);
            break;
        case ChatType::AI_CHAT_SYS_TOOL:
            change_panel_color(Color(0.02, 0.46, 0.29));//rgb(5, 119, 71)
            retry_button->set_visible(false);
            copy_button->set_visible(false);
            break;
        case ChatType::AI_CHAT_SYS_WARNING:
            change_panel_color(Color(0.9, 0.56, 0.15));//rgb(229, 142, 38)
            retry_button->set_visible(false);
            copy_button->set_visible(false);
            break;
        case ChatType::AI_CHAT_SYS_ERROR:
            change_panel_color(Color(0.72, 0.08, 0.25));//rgb(183, 21, 64)
            retry_button->set_visible(true);
            copy_button->set_visible(false);
            break;
        default:
            break;
    }
}

String AIChatBlock::get_text(){
    if(mark_text.is_empty()){
        ERR_PRINT("chat_content is empty");
        return "";
    }
    return mark_text;
}

void AIChatBlock::set_text(const String &p_text) {
    thought_content->set_visible(true);
    reason_content->set_visible(false);
    mark_text = p_text;
    thought_content->set_text(mark_text);
}
void AIChatBlock::add_text(const String &p_text) {
    thought_content->set_visible(true);
    reason_content->set_visible(false);
    thought_content->add_text(p_text);
    mark_text+=p_text;
}
void AIChatBlock::set_reason_text(const String &p_text){
    thinking_process_button->set_visible(true);
    reason_content->set_visible(true);
    reason_content->set_text(p_text);
}
void AIChatBlock::add_reason_text(const String &p_text){
    thinking_process_button->set_visible(true);
    reason_content->set_visible(true);
    reason_content->add_text(p_text);
}

void AIChatBlock::set_fit_content(bool fit){
    thought_content->set_fit_content(fit);
    reason_content->set_fit_content(fit);
}

void AIChatBlock::to_bbcode(){
    String bbcode = mark_text;
    thought_content->set_text(bbcode);
}

void AIChatBlock::set_chat_type(AIChatBlock::ChatType type) {
    chat_type = type;
}
AIChatBlock::ChatType AIChatBlock::get_chat_type() {
    return chat_type;
}
void AIChatBlock::change_thinking_process_visible(){
    thinking_process_flag = !thinking_process_flag;
    reason_content->set_visible(thinking_process_flag);
}

int AIChatBlock::get_block_index(){
    return index;
}
void AIChatBlock::set_block_index(int black_index){
    this->index = black_index;
}

void AIChatBlock::change_panel_color(const Color &new_color){
    Ref<StyleBoxFlat> style_flat;
    style_flat.instantiate();
    style_flat->set_bg_color(new_color);
    style_flat->set_corner_radius_all(10);
    add_theme_style_override(SceneStringName(panel), style_flat);
}


void AIChatBlock::set_tool_content(const String &p_text){
    tool_content->set_visible(true);
    tool_content->set_text(p_text);
}
void AIChatBlock::set_final_answer_content(const String &p_text){
    final_answer_content->set_visible(true);
    final_answer_content->set_text(p_text);
}