#include "ai_chat_block.h"

void AIChatBlock::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_text", "text"), &AIChatBlock::set_text);
}

AIChatBlock::AIChatBlock() {
    chat_content = memnew(RichTextLabel);
    add_child(chat_content);
    chat_content->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    chat_content->set_scroll_follow(true);
    chat_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    chat_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
}

void AIChatBlock::set_text(const String &p_text) {
    chat_content->set_text(p_text);
}
void AIChatBlock::add_text(const String &p_text) {
    chat_content->add_text(p_text);
}

void AIChatBlock::set_fit_content(bool fit){
    chat_content->set_fit_content(fit);
}