#ifndef AI_CHAT_BLOCK_H
#define AI_CHAT_BLOCK_H

#include "scene/gui/rich_text_label.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/style_box_flat.h"


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
    };
private:
    Ref<Theme> theme;
    AIChatBlock::ChatType chat_type = AIChatBlock::AI_CHAT_TYPE_USER;
    RichTextLabel *chat_content;
    String mark_text;

protected:
    static void _bind_methods();

public:
    void to_bbcode();
    void set_text(const String &p_text);
    void add_text(const String &p_text);
    String get_text();
    void set_fit_content(bool fit);
    void set_chat_type(AIChatBlock::ChatType type);
    AIChatBlock::ChatType get_chat_type();
    void change_panel_color(const Color &new_color);
    AIChatBlock(AIChatBlock::ChatType i_chat_type);
};



#endif