#ifndef MARKDOWN_PARSER_H
#define MARKDOWN_PARSER_H

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include "markdown_node.h"

extern "C" {
    #include "thirdparty/cmark/src/cmark.h"
    #include "thirdparty/cmark/src/node.h"
}

class MarkdownParser : public Object {
    GDCLASS(MarkdownParser, Object);

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("parse_markdown", "markdown_text"), &MarkdownParser::parse_markdown);
    }

public:
    Ref<MarkdownNode> parse_markdown(const String &markdown_text);

private:
    Ref<MarkdownNode> _convert_node_to_markdown_node(cmark_node *node);
    MarkdownNode::NodeType _convert_cmark_type_to_node_type(cmark_node_type cmark_type);
}; 

#endif // MARKDOWN_PARSER_H    