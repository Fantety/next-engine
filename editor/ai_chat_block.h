#ifndef AI_CHAT_BLOCK_H
#define AI_CHAT_BLOCK_H

#include "scene/gui/rich_text_label.h"
#include "scene/gui/panel_container.h"


class AIChatBlock : public PanelContainer {
    GDCLASS(AIChatBlock, PanelContainer);
    RichTextLabel *chat_content;
    String mark_text;
private:
    String _replace_markdown_headings(const String &text);
    String _replace_markdown_bold_italic(const String &text);
    String _replace_markdown_links(const String &text);
    String _replace_markdown_code_blocks(const String &text);
    String _replace_markdown_images(const String &text);
    String _replace_markdown_quotes(const String &text);
    String _replace_markdown_lists(const String &text);
    String _replace_markdown_inline_code(const String &text);

protected:
    static void _bind_methods();

public:
    void to_bbcode();
    void set_text(const String &p_text);
    void add_text(const String &p_text);
    String get_text();
    void set_fit_content(bool fit);
    AIChatBlock();
};



#endif