/*
 * @FilePath: \scene\gui\markdown_viewer.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-19 11:25:10
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-20 21:07:13
 */
#include "markdown_viewer.h"
#include "scene/gui/label.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/button.h"
#include "core/os/os.h"
#include "modules/gdscript/editor/gdscript_highlighter.h"

// 辅助函数：创建富文本标签并设置基本样式
RichTextLabel *_create_rich_text_label(const String &p_text = "", bool p_selectable = true) {
    RichTextLabel *label = memnew(RichTextLabel);
    label->set_text(p_text);
    label->set_use_bbcode(true);
    label->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
    label->set_scroll_follow(true);
    label->set_fit_content(true);
    label->set_selection_enabled(p_selectable);
    label->set_drag_and_drop_selection_enabled(true);
    label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    return label;
}

MarkdownViewer::MarkdownViewer() {
    // 初始化默认样式
    styles["heading_1_font_size"] = 28;
    styles["heading_2_font_size"] = 24;
    styles["heading_3_font_size"] = 20;
    styles["heading_4_font_size"] = 18;
    styles["heading_5_font_size"] = 16;
    styles["heading_6_font_size"] = 14;
    styles["text_font_size"] = 16;
    styles["code_font_size"] = 14;
    styles["block_quote_color"] = Color(0.7, 0.7, 0.7);
    styles["link_color"] = Color(0.2, 0.4, 0.8);
    styles["list_indent"] = 20;
    styles["margin"] = 10;
    // 设置默认间距
    add_theme_constant_override("separation", 5);
}


void MarkdownViewer::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            if (markdown_ast.is_valid()) {
                refresh();
            }
            break;
    }
}

void MarkdownViewer::set_markdown_ast(const Ref<MarkdownNode> &p_ast) {
    markdown_ast = p_ast;
    refresh();
}

Ref<MarkdownNode> MarkdownViewer::get_markdown_ast() const {
    return markdown_ast;
}

void MarkdownViewer::set_selectable(bool p_selectable) {
    selectable = p_selectable;
    refresh();
}

bool MarkdownViewer::is_selectable() const {
    return selectable;
}

void MarkdownViewer::set_style(const String &p_style_name, const Variant &p_style_value) {
    styles[p_style_name] = p_style_value;
    if (is_inside_tree()) {
        refresh();
    }
}

Variant MarkdownViewer::get_style(const String &p_style_name) const {
    if (styles.has(p_style_name)) {
        return styles[p_style_name];
    }
    return Variant();
}

void MarkdownViewer::clear() {
    // 移除并释放所有子控件
    Array children = get_children();
    for (int i = 0; i < children.size(); i++) {
        Control *child = Object::cast_to<Control>(children[i]);
        if (child) {
            remove_child(child);
            memdelete(child);
        }
    }
}

void MarkdownViewer::refresh() {
    clear();
    if (!markdown_ast.is_valid()) {
        return;
    }
    // 渲染文档节点
    Array children = markdown_ast->get_children();
    for (int i = 0; i < children.size(); i++) {
        Ref<MarkdownNode> child = children[i];
        if (!child.is_valid()) {
            continue;
        }
        Control *node_control = nullptr;
        switch (child->get_type()) {
            case MarkdownNode::NMARK_NODE_TEXT:
                node_control = _create_text_node(child);
                break;
                
            case MarkdownNode::NMARK_NODE_PARAGRAPH:
                node_control = _create_paragraph_node(child);
                break;
                
            case MarkdownNode::NMARK_NODE_HEADING:
                node_control = _create_heading_node(child);
                break;
                
            case MarkdownNode::NMARK_NODE_LIST:
                node_control = _create_list_node(child);
                break;
                
            case MarkdownNode::NMARK_NODE_CODE_BLOCK:
                node_control = _create_code_block_node(child);
                break;
                
            case MarkdownNode::NMARK_NODE_LINK:
                node_control = _create_link_node(child);
                break;
                
            case MarkdownNode::NMARK_NODE_IMAGE:
                node_control = _create_image_node(child);
                break;
                
            case MarkdownNode::NMARK_NODE_BLOCK_QUOTE:
                node_control = _create_block_quote_node(child);
                break;
                
            default:
                // 未知节点类型，创建一个简单的文本标签
                RichTextLabel *label = _create_rich_text_label("[" + itos((int)child->get_type()) + "] " + child->get_literal(), selectable);
                node_control = label;
                break;
        }
        if (node_control) {
            add_child(node_control);
        }
    }
}

Control *MarkdownViewer::_create_text_node(const Ref<MarkdownNode> &p_node) {
    RichTextLabel *label = _create_rich_text_label(p_node->get_literal(), selectable);
    _apply_text_formatting(label, p_node);
    return label;
}

Control *MarkdownViewer::_create_heading_node(const Ref<MarkdownNode> &p_node) {
    int level = p_node->get_heading_level();
    if (level < 1 || level > 6) {
        level = 1;
    }
    String style_name = "heading_" + itos(level) + "_font_size";
    int font_size = styles.has(style_name) ? styles[style_name].operator int() : 24 - (level - 1) * 4;
    
    RichTextLabel *label = _create_rich_text_label(p_node->get_literal(), selectable);
    label->add_theme_font_size_override("normal_font_size", font_size);
    label->add_theme_font_size_override("bold_font_size", font_size);
    label->add_theme_font_size_override("italics_font_size", font_size);
    label->add_theme_font_size_override("bold_italics_font_size", font_size);
    label->add_theme_font_size_override("mono_font_size", font_size);
    // 解析内联元素（如粗体、斜体等）
    _parse_inline_elements(label, p_node);
    
    return label;
}

Control *MarkdownViewer::_create_paragraph_node(const Ref<MarkdownNode> &p_node) {
    RichTextLabel *label = _create_rich_text_label(p_node->get_literal(), selectable);
    label->add_theme_font_size_override("normal_font_size", styles["text_font_size"]);
    label->add_theme_font_size_override("bold_font_size", styles["text_font_size"]);
    label->add_theme_font_size_override("italics_font_size", styles["text_font_size"]);
    label->add_theme_font_size_override("bold_italics_font_size", styles["text_font_size"]);
    label->add_theme_font_size_override("mono_font_size", styles["text_font_size"]);

    // 解析内联元素
    _parse_inline_elements(label, p_node);
    return label;
}

Control *MarkdownViewer::_create_list_node(const Ref<MarkdownNode> &p_node) {
    VBoxContainer *list_container = memnew(VBoxContainer);
    list_container->add_theme_constant_override("separation", 5);
    
    // 设置缩进
    MarginContainer *margin = memnew(MarginContainer);
    margin->add_theme_constant_override("margin_left", styles["list_indent"]);
    margin->add_child(list_container);
    
    // 渲染列表项
    Array children = p_node->get_children();
    MarkdownNode::ListStyle list_style = p_node->get_list_style();
    
    for (int i = 0; i < children.size(); i++) {
        Ref<MarkdownNode> list_item = children[i];
        if (!list_item.is_valid() || list_item->get_type() != MarkdownNode::NMARK_NODE_LIST_ITEM) {
            continue;
        }
        
        String prefix = "";
        if (list_style == MarkdownNode::NMARK_LIST_STYLE_BULLET) {
            prefix = "• ";
        } else if (list_style == MarkdownNode::NMARK_LIST_STYLE_ORDERED) {
            prefix = itos(i + 1) + ". ";
        }
        
        RichTextLabel *item_label = _create_rich_text_label(prefix + list_item->get_literal(), selectable);
        item_label->add_theme_font_size_override("normal_font_size", styles["text_font_size"]);
        item_label->add_theme_font_size_override("bold_font_size", styles["text_font_size"]);
        item_label->add_theme_font_size_override("italics_font_size", styles["text_font_size"]);
        item_label->add_theme_font_size_override("bold_italics_font_size", styles["text_font_size"]);
        item_label->add_theme_font_size_override("mono_font_size", styles["text_font_size"]);
        
        // 解析列表项中的内联元素
        _parse_inline_elements(item_label, list_item);
        
        list_container->add_child(item_label);
    }
    
    return margin;
}

Control *MarkdownViewer::_create_code_block_node(const Ref<MarkdownNode> &p_node) {
    CodeEdit *code_editor = memnew(CodeEdit);
    code_editor->set_text(p_node->get_literal());
    code_editor->set_editable(false);
    code_editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    code_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    Ref<GDScriptSyntaxHighlighter> highlighter;
	highlighter.instantiate();
    code_editor->set_syntax_highlighter(highlighter);
    // 设置代码块样式
    Ref<StyleBoxFlat> style = memnew(StyleBoxFlat);
    style->set_bg_color(Color(0.1, 0.1, 0.1, 0.1));
    style->set_border_width_all(1);
    style->set_border_color(Color(0.3, 0.3, 0.3));
    style->set_corner_radius_all(4);
    code_editor->add_theme_style_override("normal", style);
    // 添加语言信息（如果有）
    if (!p_node->get_fence_info().is_empty()) {
        Label *lang_label = memnew(Label);
        lang_label->set_text(p_node->get_fence_info().to_upper() + " code block");
        lang_label->add_theme_font_size_override("font_size", 12);
        add_child(lang_label);
    }
    return code_editor;
}

Control *MarkdownViewer::_create_link_node(const Ref<MarkdownNode> &p_node) {
    LinkButton *link = memnew(LinkButton);
    link->set_text(p_node->get_literal());
    link->set_uri(p_node->get_url());
    link->set_underline_mode(LinkButton::UNDERLINE_MODE_ALWAYS);
    link->add_theme_color_override("font_color", styles["link_color"]);
    
    // 链接点击信号
    link->connect("pressed", Callable(this, "_on_link_pressed").bind(p_node->get_url()));
    return link;
}
void MarkdownViewer::_on_link_pressed(const String &p_url) {
    // 处理链接点击事件
    OS::get_singleton()->shell_open(p_url);
}


Control *MarkdownViewer::_create_image_node(const Ref<MarkdownNode> &p_node) {
    HBoxContainer *image_container = memnew(HBoxContainer);
    image_container->set_alignment(BoxContainer::ALIGNMENT_CENTER);
    
    TextureRect *texture_rect = memnew(TextureRect);
    texture_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    texture_rect->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    texture_rect->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    
    // 尝试加载图像
    String image_path = p_node->get_url();
    if (!image_path.is_empty()) {
        Ref<Texture2D> texture = ResourceLoader::load(image_path);
        if (texture.is_valid()) {
            texture_rect->set_texture(texture);
        } else {
            // 图像加载失败，显示占位符
            RichTextLabel *placeholder = _create_rich_text_label("[Image: " + image_path + "]", selectable);
            image_container->add_child(placeholder);
        }
    }
    
    // 添加标题（如果有）
    if (!p_node->get_title().is_empty()) {
        Label *title_label = memnew(Label);
        title_label->set_text(p_node->get_title());
        title_label->add_theme_font_size_override("font_size", 14);
        add_child(title_label);
    }
    
    if (texture_rect->get_texture().is_valid()) {
        image_container->add_child(texture_rect);
    }
    
    return image_container;
}

Control *MarkdownViewer::_create_block_quote_node(const Ref<MarkdownNode> &p_node) {
    MarginContainer *quote_container = memnew(MarginContainer);
    quote_container->add_theme_constant_override("margin_left", 20);
    // 设置引用样式
    Ref<StyleBoxFlat> style = memnew(StyleBoxFlat);
    style->set_bg_color(Color(0.95, 0.95, 0.95));
    style->set_border_width(Side::SIDE_LEFT, 4);
    style->set_border_color(styles["block_quote_color"]);
    quote_container->add_theme_style_override("normal", style);
    
    // 创建引用内容
    RichTextLabel *quote_label = _create_rich_text_label(p_node->get_literal(), selectable);
    quote_label->add_theme_font_size_override("normal_font_size", styles["text_font_size"]);
    quote_label->add_theme_font_size_override("bold_font_size", styles["text_font_size"]);
    quote_label->add_theme_font_size_override("italics_font_size", styles["text_font_size"]);
    quote_label->add_theme_font_size_override("bold_italics_font_size", styles["text_font_size"]);
    quote_label->add_theme_font_size_override("mono_font_size", styles["text_font_size"]);
    
    // 解析内联元素
    _parse_inline_elements(quote_label, p_node);
    quote_container->add_child(quote_label);
    return quote_container;
}

void MarkdownViewer::_apply_text_formatting(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node) {
    // 应用文本格式（如粗体、斜体等）
    if (p_node->get_type() == MarkdownNode::NMARK_NODE_STRONG) {
        p_label->add_text("[b]");
        p_label->add_text(p_node->get_literal());
        p_label->add_text("[/b]");
    } else if (p_node->get_type() == MarkdownNode::NMARK_NODE_EMPH) {
        p_label->add_text("[i]");
        p_label->add_text(p_node->get_literal());
        p_label->add_text("[/i]");
    } else if (p_node->get_type() == MarkdownNode::NMARK_NODE_CODE) {
        p_label->add_text("[code]");
        p_label->add_text(p_node->get_literal());
        p_label->add_text("[/code]");
    } else {
        p_label->add_text(p_node->get_literal());
    }
}

void MarkdownViewer::_parse_inline_elements(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node) {
    // 清空现有文本，重新解析所有子元素
    p_label->set_text("");
    Array children = p_node->get_children();
    for (int i = 0; i < children.size(); i++) {
        Ref<MarkdownNode> child = children[i];
        if (!child.is_valid()) {
            continue;
        }
        _apply_text_formatting(p_label, child);
    }
}

void MarkdownViewer::_bind_methods() {
    // 绑定属性
    ClassDB::bind_method(D_METHOD("set_markdown_ast", "ast"), &MarkdownViewer::set_markdown_ast);
    ClassDB::bind_method(D_METHOD("get_markdown_ast"), &MarkdownViewer::get_markdown_ast);
    
    ClassDB::bind_method(D_METHOD("set_selectable", "selectable"), &MarkdownViewer::set_selectable);
    ClassDB::bind_method(D_METHOD("is_selectable"), &MarkdownViewer::is_selectable);
    
    ClassDB::bind_method(D_METHOD("set_style", "style_name", "style_value"), &MarkdownViewer::set_style);
    ClassDB::bind_method(D_METHOD("get_style", "style_name"), &MarkdownViewer::get_style);
    
    ClassDB::bind_method(D_METHOD("refresh"), &MarkdownViewer::refresh);
    ClassDB::bind_method(D_METHOD("clear"), &MarkdownViewer::clear);
    
    // 注册属性
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "markdown_ast", PROPERTY_HINT_RESOURCE_TYPE, "MarkdownNode"), "set_markdown_ast", "get_markdown_ast");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selectable"), "set_selectable", "is_selectable");
}