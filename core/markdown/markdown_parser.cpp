#include "markdown_parser.h"
#include "core/error/error_macros.h"
#include "core/object/class_db.h"

Ref<MarkdownNode> MarkdownParser::parse_markdown(const String &markdown_text) {
    const char *text = markdown_text.utf8().get_data();
    size_t len = markdown_text.length();
    
    if (len == 0) {
        ERR_PRINT("Empty Markdown text provided");
        return nullptr;
    }
    
    cmark_node *doc = cmark_parse_document(text, len, CMARK_OPT_DEFAULT);
    if (!doc) {
        ERR_PRINT("Failed to parse Markdown document");
        return nullptr;
    }
    
    Ref<MarkdownNode> root_node = _convert_node_to_markdown_node(doc);
    cmark_node_free(doc);
    return root_node;
}

Ref<MarkdownNode> MarkdownParser::_convert_node_to_markdown_node(cmark_node *node) {
    if (!node) {
        return nullptr;
    }
    
    Ref<MarkdownNode> md_node;
    md_node.instantiate();
    
    // 设置节点类型
    md_node->set_type(_convert_cmark_type_to_node_type(cmark_node_get_type(node)));
    
    // 设置文本内容
    const char *literal = cmark_node_get_literal(node);
    if (literal) {
        md_node->set_literal(String(literal));
    }

    // 设置标题级别
    int heading_level = cmark_node_get_heading_level(node);
    if (heading_level > 0) {
        md_node->set_heading_level(heading_level);
    }

    // 设置列表类型
    cmark_list_type list_type = cmark_node_get_list_type(node);
    if (list_type == CMARK_BULLET_LIST) {
        md_node->set_list_style(MarkdownNode::NMARK_LIST_STYLE_BULLET);
    } else if (list_type == CMARK_ORDERED_LIST) {
        md_node->set_list_style(MarkdownNode::NMARK_LIST_STYLE_ORDERED);
    }

    // 设置链接和图片属性
    const char *url = cmark_node_get_url(node);
    if (url && *url) {
        md_node->set_url(String(url));
    }

    const char *title = cmark_node_get_title(node);
    if (title && *title) {
        md_node->set_title(String(title));
    }

    // 设置代码块信息
    const char *fence_info = cmark_node_get_fence_info(node);
    if (fence_info && *fence_info) {
        md_node->set_fence_info(String(fence_info));
    }

    // 处理子节点
    cmark_node *child = cmark_node_first_child(node);
    while (child) {
        Ref<MarkdownNode> child_node = _convert_node_to_markdown_node(child);
        if (child_node.is_valid()) {
            md_node->add_child_node(child_node.ptr());
        }
        child = cmark_node_next(child);
    }

    return md_node;
}

MarkdownNode::NodeType MarkdownParser::_convert_cmark_type_to_node_type(cmark_node_type cmark_type) {
    switch (cmark_type) {
        case cmark_node_type::CMARK_NODE_DOCUMENT:
            return MarkdownNode::NMARK_NODE_DOCUMENT;
        case cmark_node_type::CMARK_NODE_PARAGRAPH:
            return MarkdownNode::NMARK_NODE_PARAGRAPH;
        case cmark_node_type::CMARK_NODE_HEADING:
            return MarkdownNode::NMARK_NODE_HEADING;
        case cmark_node_type::CMARK_NODE_LIST:
            return MarkdownNode::NMARK_NODE_LIST;
        case cmark_node_type::CMARK_NODE_ITEM:
            return MarkdownNode::NMARK_NODE_LIST_ITEM;
        case cmark_node_type::CMARK_NODE_TEXT:
            return MarkdownNode::NMARK_NODE_TEXT;
        case cmark_node_type::CMARK_NODE_CODE:
            return MarkdownNode::NMARK_NODE_CODE;
        case cmark_node_type::CMARK_NODE_CODE_BLOCK:
            return MarkdownNode::NMARK_NODE_CODE_BLOCK;
        case cmark_node_type::CMARK_NODE_LINK:
            return MarkdownNode::NMARK_NODE_LINK;
        case cmark_node_type::CMARK_NODE_IMAGE:
            return MarkdownNode::NMARK_NODE_IMAGE;
        case cmark_node_type::CMARK_NODE_EMPH:
            return MarkdownNode::NMARK_NODE_EMPH;
        case cmark_node_type::CMARK_NODE_STRONG:
            return MarkdownNode::NMARK_NODE_STRONG;
        case cmark_node_type::CMARK_NODE_BLOCK_QUOTE:
            return MarkdownNode::NMARK_NODE_BLOCK_QUOTE;
        default:
            return MarkdownNode::NMARK_NODE_TEXT;
    }
}    