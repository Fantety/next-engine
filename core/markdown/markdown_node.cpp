#include "markdown_node.h"

void MarkdownNode::_bind_methods() {
	BIND_ENUM_CONSTANT(NMARK_NODE_DOCUMENT);
	BIND_ENUM_CONSTANT(NMARK_NODE_PARAGRAPH);
	BIND_ENUM_CONSTANT(NMARK_NODE_HEADING);
	BIND_ENUM_CONSTANT(NMARK_NODE_LIST);
	BIND_ENUM_CONSTANT(NMARK_NODE_LIST_ITEM);
	BIND_ENUM_CONSTANT(NMARK_NODE_TEXT);
	BIND_ENUM_CONSTANT(NMARK_NODE_CODE);
	BIND_ENUM_CONSTANT(NMARK_NODE_CODE_BLOCK);
	BIND_ENUM_CONSTANT(NMARK_NODE_LINK);
	BIND_ENUM_CONSTANT(NMARK_NODE_IMAGE);
	BIND_ENUM_CONSTANT(NMARK_NODE_EMPH);
	BIND_ENUM_CONSTANT(NMARK_NODE_STRONG);
	BIND_ENUM_CONSTANT(NMARK_NODE_BLOCK_QUOTE);
	BIND_ENUM_CONSTANT(NMARK_NODE_THEMATIC_BREAK);

	ClassDB::bind_method(D_METHOD("set_type", "type"), &MarkdownNode::set_type);
	ClassDB::bind_method(D_METHOD("get_type"), &MarkdownNode::get_type);

	ClassDB::bind_method(D_METHOD("set_literal", "literal"), &MarkdownNode::set_literal);
	ClassDB::bind_method(D_METHOD("get_literal"), &MarkdownNode::get_literal);

	ClassDB::bind_method(D_METHOD("set_heading_level", "level"), &MarkdownNode::set_heading_level);
	ClassDB::bind_method(D_METHOD("get_heading_level"), &MarkdownNode::get_heading_level);

	ClassDB::bind_method(D_METHOD("set_list_style", "style"), &MarkdownNode::set_list_style);
	ClassDB::bind_method(D_METHOD("get_list_style"), &MarkdownNode::get_list_style);

	ClassDB::bind_method(D_METHOD("set_url", "url"), &MarkdownNode::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &MarkdownNode::get_url);

	ClassDB::bind_method(D_METHOD("set_title", "title"), &MarkdownNode::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &MarkdownNode::get_title);

	ClassDB::bind_method(D_METHOD("set_fence_info", "info"), &MarkdownNode::set_fence_info);
	ClassDB::bind_method(D_METHOD("get_fence_info"), &MarkdownNode::get_fence_info);

	ClassDB::bind_method(D_METHOD("add_child_node", "child"), &MarkdownNode::add_child_node);
	ClassDB::bind_method(D_METHOD("get_children"), &MarkdownNode::get_children);

	ClassDB::bind_method(D_METHOD("to_dictionary"), &MarkdownNode::to_dictionary);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM, "Document,Paragraph,Heading,List,ListItem,Text,Code,CodeBlock,Link,Image,Emph,Strong,BlockQuote,ThematicBreak"), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "literal"), "set_literal", "get_literal");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "heading_level"), "set_heading_level", "get_heading_level");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "list_style", PROPERTY_HINT_ENUM, "None,Bullet,Ordered"), "set_list_style", "get_list_style");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "fence_info"), "set_fence_info", "get_fence_info");
}

Dictionary MarkdownNode::to_dictionary() const {
	Dictionary node_dict;

	node_dict["type"] = (int)type;
	if (!literal.is_empty()) {
		node_dict["literal"] = literal;
	}

	if (heading_level > 0) {
		node_dict["heading_level"] = heading_level;
	}

	if (list_style != NMARK_LIST_STYLE_NONE) {
		node_dict["list_style"] = (int)list_style;
	}

	if (!url.is_empty()) {
		node_dict["url"] = url;
	}

	if (!title.is_empty()) {
		node_dict["title"] = title;
	}

	if (!fence_info.is_empty()) {
		node_dict["fence_info"] = fence_info;
	}

	if (!children.is_empty()) {
		Array children_dict_array;
		for (int i = 0; i < children.size(); i++) {
			Ref<MarkdownNode> child = children[i];
			if (child.is_valid()) {
				children_dict_array.append(child->to_dictionary());
			}
		}
		node_dict["children"] = children_dict_array;
	}

	return node_dict;
}
