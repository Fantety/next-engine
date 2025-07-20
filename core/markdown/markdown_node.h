#ifndef MARKDOWN_NODE_H
#define MARKDOWN_NODE_H

#include "core/io/resource.h"
#include "core/object/class_db.h"
#include "core/variant/array.h"
#include "core/string/ustring.h"

class MarkdownNode : public Resource {
    GDCLASS(MarkdownNode, Resource);

protected:
    static void _bind_methods();

public:
    // 节点类型枚举
    enum NodeType {
        NMARK_NODE_DOCUMENT,
        NMARK_NODE_PARAGRAPH,
        NMARK_NODE_HEADING,
        NMARK_NODE_LIST,
        NMARK_NODE_LIST_ITEM,
        NMARK_NODE_TEXT,
        NMARK_NODE_CODE,
        NMARK_NODE_CODE_BLOCK,
        NMARK_NODE_LINK,
        NMARK_NODE_IMAGE,
        NMARK_NODE_EMPH,
        NMARK_NODE_STRONG,
        NMARK_NODE_BLOCK_QUOTE,
        // 其他节点类型...
    };
    enum ListStyle {
        NMARK_LIST_STYLE_NONE,
        NMARK_LIST_STYLE_BULLET,
        NMARK_LIST_STYLE_ORDERED
    };

private:
    NodeType type = NMARK_NODE_TEXT;
    String literal;
    int heading_level = 0;
    ListStyle list_style = NMARK_LIST_STYLE_NONE;
    String url;
    String title;
    String fence_info;
    Array children;

public:
    // Getters and setters
    void set_type(NodeType p_type) { type = p_type; }
    NodeType get_type() const { return type; }

    void set_literal(const String &p_literal) { literal = p_literal; }
    String get_literal() const { return literal; }

    void set_heading_level(int p_level) { heading_level = p_level; }
    int get_heading_level() const { return heading_level; }

    void set_list_style(ListStyle p_style) { list_style = p_style; }
    ListStyle get_list_style() const { return list_style; }

    void set_url(const String &p_url) { url = p_url; }
    String get_url() const { return url; }

    void set_title(const String &p_title) { title = p_title; }
    String get_title() const { return title; }

    void set_fence_info(const String &p_info) { fence_info = p_info; }
    String get_fence_info() const { return fence_info; }

    void add_child_node(MarkdownNode *p_child) {
        if (p_child) {
            children.append(Ref<MarkdownNode>(p_child));
        }
    }

    Array get_children() const { return children; }

    // 新增方法：转换为Dictionary
    Dictionary to_dictionary() const;
};

VARIANT_ENUM_CAST(MarkdownNode::NodeType);
VARIANT_ENUM_CAST(MarkdownNode::ListStyle);

#endif // MARKDOWN_NODE_H    