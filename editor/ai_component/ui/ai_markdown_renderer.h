/**************************************************************************/
/*  ai_markdown_renderer.h                                                */
/**************************************************************************/

#pragma once

#include "core/markdown/markdown_node.h"
#include "core/string/ustring.h"

class RichTextLabel;

class AIMarkdownRenderer {
	int heading_base_size = 0;
	int list_depth = 0;
	bool in_list_item = false;

	void _render_children(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node);
	void _render_node(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node);
	void _render_block_node(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node);
	void _render_inline_node(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node);
	void _append_block_spacing(RichTextLabel *p_label);
	void _append_line_break_if_needed(RichTextLabel *p_label);
	void _append_code_block(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node);
	bool _push_bold_if_available(RichTextLabel *p_label);
	bool _push_italics_if_available(RichTextLabel *p_label);
	bool _push_mono_if_available(RichTextLabel *p_label);
	bool _is_inline_node(MarkdownNode::NodeType p_type) const;
	bool _node_has_renderable_text(const Ref<MarkdownNode> &p_node) const;

public:
	void set_heading_base_size(int p_size);
	void render(RichTextLabel *p_label, const Ref<MarkdownNode> &p_root);
	void render_append(RichTextLabel *p_label, const Ref<MarkdownNode> &p_root);
	void render_inline_markdown(RichTextLabel *p_label, const String &p_markdown);
};
