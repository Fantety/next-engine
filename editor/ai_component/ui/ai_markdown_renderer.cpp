/**************************************************************************/
/*  ai_markdown_renderer.cpp                                              */
/**************************************************************************/

#include "ai_markdown_renderer.h"

#include "core/error/error_macros.h"
#include "core/string/string_name.h"
#include "scene/gui/rich_text_label.h"

namespace {

int _clamp_heading_level(int p_level) {
	if (p_level < 1) {
		return 1;
	}
	if (p_level > 6) {
		return 6;
	}
	return p_level;
}

int _max_int(int p_a, int p_b) {
	return p_a > p_b ? p_a : p_b;
}

} // namespace

void AIMarkdownRenderer::set_heading_base_size(int p_size) {
	heading_base_size = p_size;
}

void AIMarkdownRenderer::render(RichTextLabel *p_label, const Ref<MarkdownNode> &p_root) {
	ERR_FAIL_NULL(p_label);

	p_label->clear();
	list_depth = 0;
	in_list_item = false;

	if (p_root.is_null()) {
		return;
	}

	_render_children(p_label, p_root);
}

void AIMarkdownRenderer::_render_children(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node) {
	Array children = p_node->get_children();
	for (int i = 0; i < children.size(); i++) {
		Ref<MarkdownNode> child = children[i];
		if (child.is_valid()) {
			_render_node(p_label, child);
		}
	}
}

void AIMarkdownRenderer::_render_node(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node) {
	if (p_node.is_null()) {
		return;
	}

	if (_is_inline_node(p_node->get_type())) {
		_render_inline_node(p_label, p_node);
	} else {
		_render_block_node(p_label, p_node);
	}
}

void AIMarkdownRenderer::_render_block_node(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node) {
	switch (p_node->get_type()) {
		case MarkdownNode::NMARK_NODE_DOCUMENT: {
			_render_children(p_label, p_node);
		} break;
		case MarkdownNode::NMARK_NODE_PARAGRAPH: {
			_render_children(p_label, p_node);
			if (in_list_item) {
				_append_line_break_if_needed(p_label);
			} else {
				_append_block_spacing(p_label);
			}
		} break;
		case MarkdownNode::NMARK_NODE_HEADING: {
			const int level = _clamp_heading_level(p_node->get_heading_level());
			bool pushed_font_size = false;
			if (heading_base_size > 0) {
				p_label->push_font_size(_max_int(heading_base_size + 5 - level, heading_base_size));
				pushed_font_size = true;
			}
			const bool pushed_bold = _push_bold_if_available(p_label);
			_render_children(p_label, p_node);
			if (pushed_bold) {
				p_label->pop();
			}
			if (pushed_font_size) {
				p_label->pop();
			}
			_append_block_spacing(p_label);
		} break;
		case MarkdownNode::NMARK_NODE_LIST: {
			const RichTextLabel::ListType list_type = p_node->get_list_style() == MarkdownNode::NMARK_LIST_STYLE_ORDERED ? RichTextLabel::LIST_NUMBERS : RichTextLabel::LIST_DOTS;
			p_label->push_list(list_depth, list_type, false);
			list_depth++;
			_render_children(p_label, p_node);
			list_depth--;
			p_label->pop();
			_append_block_spacing(p_label);
		} break;
		case MarkdownNode::NMARK_NODE_LIST_ITEM: {
			const bool was_in_list_item = in_list_item;
			in_list_item = true;
			_render_children(p_label, p_node);
			in_list_item = was_in_list_item;
			_append_line_break_if_needed(p_label);
		} break;
		case MarkdownNode::NMARK_NODE_CODE_BLOCK: {
			_append_code_block(p_label, p_node);
			_append_block_spacing(p_label);
		} break;
		case MarkdownNode::NMARK_NODE_BLOCK_QUOTE: {
			p_label->push_indent(list_depth + 1);
			const bool pushed_italics = _push_italics_if_available(p_label);
			_render_children(p_label, p_node);
			if (pushed_italics) {
				p_label->pop();
			}
			p_label->pop();
			_append_block_spacing(p_label);
		} break;
		default: {
			_render_inline_node(p_label, p_node);
		} break;
	}
}

void AIMarkdownRenderer::_render_inline_node(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node) {
	switch (p_node->get_type()) {
		case MarkdownNode::NMARK_NODE_TEXT: {
			p_label->add_text(p_node->get_literal());
		} break;
		case MarkdownNode::NMARK_NODE_CODE: {
			const bool pushed_mono = _push_mono_if_available(p_label);
			p_label->add_text(p_node->get_literal());
			if (pushed_mono) {
				p_label->pop();
			}
		} break;
		case MarkdownNode::NMARK_NODE_EMPH: {
			const bool pushed_italics = _push_italics_if_available(p_label);
			_render_children(p_label, p_node);
			if (pushed_italics) {
				p_label->pop();
			}
		} break;
		case MarkdownNode::NMARK_NODE_STRONG: {
			const bool pushed_bold = _push_bold_if_available(p_label);
			_render_children(p_label, p_node);
			if (pushed_bold) {
				p_label->pop();
			}
		} break;
		case MarkdownNode::NMARK_NODE_LINK: {
			if (!p_node->get_url().is_empty()) {
				p_label->push_meta(p_node->get_url(), RichTextLabel::META_UNDERLINE_ON_HOVER, p_node->get_url());
				_render_children(p_label, p_node);
				p_label->pop();
			} else {
				_render_children(p_label, p_node);
			}
		} break;
		case MarkdownNode::NMARK_NODE_IMAGE: {
			if (_node_has_renderable_text(p_node)) {
				_render_children(p_label, p_node);
			} else if (!p_node->get_url().is_empty()) {
				p_label->add_text(p_node->get_url());
			}
		} break;
		default: {
			_render_children(p_label, p_node);
			if (!p_node->get_literal().is_empty()) {
				p_label->add_text(p_node->get_literal());
			}
		} break;
	}
}

void AIMarkdownRenderer::_append_block_spacing(RichTextLabel *p_label) {
	const String parsed_text = p_label->get_parsed_text();
	if (parsed_text.is_empty() || parsed_text.ends_with("\n\n")) {
		return;
	}
	if (!parsed_text.ends_with("\n")) {
		p_label->add_newline();
	}
	p_label->add_newline();
}

void AIMarkdownRenderer::_append_line_break_if_needed(RichTextLabel *p_label) {
	const String parsed_text = p_label->get_parsed_text();
	if (parsed_text.is_empty() || parsed_text.ends_with("\n")) {
		return;
	}
	p_label->add_newline();
}

void AIMarkdownRenderer::_append_code_block(RichTextLabel *p_label, const Ref<MarkdownNode> &p_node) {
	if (!p_node->get_fence_info().is_empty()) {
		const bool pushed_italics = _push_italics_if_available(p_label);
		p_label->add_text(p_node->get_fence_info());
		if (pushed_italics) {
			p_label->pop();
		}
		p_label->add_newline();
	}

	const bool pushed_mono = _push_mono_if_available(p_label);
	p_label->add_text(p_node->get_literal().rstrip("\n"));
	if (pushed_mono) {
		p_label->pop();
	}
}

bool AIMarkdownRenderer::_push_bold_if_available(RichTextLabel *p_label) {
	if (p_label->get_theme_font(SNAME("bold_font")).is_null()) {
		return false;
	}

	p_label->push_bold();
	return true;
}

bool AIMarkdownRenderer::_push_italics_if_available(RichTextLabel *p_label) {
	if (p_label->get_theme_font(SNAME("italics_font")).is_null()) {
		return false;
	}

	p_label->push_italics();
	return true;
}

bool AIMarkdownRenderer::_push_mono_if_available(RichTextLabel *p_label) {
	if (p_label->get_theme_font(SNAME("mono_font")).is_null()) {
		return false;
	}

	p_label->push_mono();
	return true;
}

bool AIMarkdownRenderer::_is_inline_node(MarkdownNode::NodeType p_type) const {
	switch (p_type) {
		case MarkdownNode::NMARK_NODE_TEXT:
		case MarkdownNode::NMARK_NODE_CODE:
		case MarkdownNode::NMARK_NODE_LINK:
		case MarkdownNode::NMARK_NODE_IMAGE:
		case MarkdownNode::NMARK_NODE_EMPH:
		case MarkdownNode::NMARK_NODE_STRONG:
			return true;
		default:
			return false;
	}
}

bool AIMarkdownRenderer::_node_has_renderable_text(const Ref<MarkdownNode> &p_node) const {
	if (p_node.is_null()) {
		return false;
	}
	if (!p_node->get_literal().is_empty()) {
		return true;
	}

	Array children = p_node->get_children();
	for (int i = 0; i < children.size(); i++) {
		Ref<MarkdownNode> child = children[i];
		if (_node_has_renderable_text(child)) {
			return true;
		}
	}

	return false;
}
