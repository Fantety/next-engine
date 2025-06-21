#ifndef AI_CHAT_BLOCK_H
#define AI_CHAT_BLOCK_H

#include "scene/gui/rich_text_label.h"
#include "scene/gui/panel_container.h"


class AIChatBlock : public PanelContainer {
    GDCLASS(AIChatBlock, PanelContainer);
    RichTextLabel *chat_content;

protected:
    static void _bind_methods();

public:
    void set_text(const String &p_text);
    void add_text(const String &p_text);
    void set_fit_content(bool fit);
    AIChatBlock();
};



#endif