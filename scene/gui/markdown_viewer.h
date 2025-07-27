#ifndef MARKDOWN_RENDERER_H
#define MARKDOWN_RENDERER_H

#include "scene/gui/box_container.h"
#include "core/markdown/markdown_parser.h"
#include "scene/resources/label_settings.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/code_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/link_button.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/style_box_flat.h"

class MarkdownViewer : public VBoxContainer {
    GDCLASS(MarkdownViewer, VBoxContainer);

private:
    Ref<MarkdownNode> markdown_ast;
    bool selectable = true;
    Ref<Font> code_font;
    Dictionary styles;

    // 辅助方法
    Control *_create_text_node(const Ref<MarkdownNode> &p_node);
    Control *_create_heading_node(const Ref<MarkdownNode> &p_node);
    Control *_create_paragraph_node(const Ref<MarkdownNode> &p_node);
    Control *_create_list_node(const Ref<MarkdownNode> &p_node);
    Control *_create_code_block_node(const Ref<MarkdownNode> &p_node);
    Control *_create_link_node(const Ref<MarkdownNode> &p_node);
    Control *_create_image_node(const Ref<MarkdownNode> &p_node);
    Control *_create_block_quote_node(const Ref<MarkdownNode> &p_node);
    
    void _apply_text_formatting(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node);
    void _parse_inline_elements(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node);

    void _on_link_pressed(const String &p_url);

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    MarkdownViewer();

    // API方法
    void set_markdown_ast(const Ref<MarkdownNode> &p_ast);
    Ref<MarkdownNode> get_markdown_ast() const;
    
    void set_selectable(bool p_selectable);
    bool is_selectable() const;
    
    void set_style(const String &p_style_name, const Variant &p_style_value);
    Variant get_style(const String &p_style_name) const;
    
    void refresh();
    void clear();
};

#endif // MARKDOWN_RENDERER_H
