/**************************************************************************/
/*  markdown_viewer_document.cpp                                          */
/**************************************************************************/

#include "markdown_viewer_document.h"

#include "core/markdown/markdown_node.h"
#include "core/markdown/markdown_parser.h"

namespace {

void _append_inlines_from_node(const Ref<MarkdownNode> &p_node, Vector<MarkdownViewerInline> &r_inlines);
MarkdownViewerBlock _make_block_from_node(const Ref<MarkdownNode> &p_node);

void _append_text_inline(const String &p_text, Vector<MarkdownViewerInline> &r_inlines) {
	if (p_text.is_empty()) {
		return;
	}

	MarkdownViewerInline run;
	run.type = MarkdownViewerInline::TYPE_TEXT;
	run.text = p_text;
	r_inlines.push_back(run);
}

String _flatten_viewer_inlines(const Vector<MarkdownViewerInline> &p_inlines) {
	String text;
	for (const MarkdownViewerInline &run : p_inlines) {
		if (run.children.is_empty()) {
			text += run.text;
		} else {
			text += _flatten_viewer_inlines(run.children);
		}
	}
	return text;
}

void _append_text_with_strikethrough(const String &p_text, Vector<MarkdownViewerInline> &r_inlines) {
	int cursor = 0;
	while (cursor < p_text.length()) {
		const int start = p_text.find("~~", cursor);
		if (start < 0) {
			_append_text_inline(p_text.substr(cursor), r_inlines);
			return;
		}

		const int end = p_text.find("~~", start + 2);
		if (end < 0) {
			_append_text_inline(p_text.substr(cursor), r_inlines);
			return;
		}

		_append_text_inline(p_text.substr(cursor, start - cursor), r_inlines);

		const String strikethrough_text = p_text.substr(start + 2, end - start - 2);
		if (strikethrough_text.is_empty()) {
			_append_text_inline("~~~~", r_inlines);
		} else {
			MarkdownViewerInline run;
			run.type = MarkdownViewerInline::TYPE_STRIKETHROUGH;
			run.text = strikethrough_text;
			MarkdownViewerInline child;
			child.type = MarkdownViewerInline::TYPE_TEXT;
			child.text = strikethrough_text;
			run.children.push_back(child);
			r_inlines.push_back(run);
		}

		cursor = end + 2;
	}
}

String _strip_table_edge_pipes(const String &p_line) {
	String line = p_line.strip_edges();
	if (line.begins_with("|")) {
		line = line.substr(1);
	}
	if (line.ends_with("|")) {
		line = line.substr(0, line.length() - 1);
	}
	return line;
}

bool _is_table_separator_cell(const String &p_cell) {
	String cell = p_cell.strip_edges();
	if (cell.is_empty()) {
		return false;
	}
	cell = cell.trim_prefix(":").trim_suffix(":");
	for (int i = 0; i < cell.length(); i++) {
		const char32_t c = cell[i];
		if (c != '-' && c != ' ') {
			return false;
		}
	}
	return cell.contains("-");
}

bool _is_table_separator_line(const String &p_line) {
	Vector<String> cells = _strip_table_edge_pipes(p_line).split("|", false);
	if (cells.size() == 0) {
		return false;
	}
	for (int i = 0; i < cells.size(); i++) {
		if (!_is_table_separator_cell(cells[i])) {
			return false;
		}
	}
	return true;
}

String _flatten_child_text(const Ref<MarkdownNode> &p_node) {
	if (p_node.is_null()) {
		return String();
	}
	String text = p_node->get_literal();
	Array children = p_node->get_children();
	for (int i = 0; i < children.size(); i++) {
		Ref<MarkdownNode> child = children[i];
		text += _flatten_child_text(child);
	}
	return text;
}

MarkdownViewerInline _make_inline(MarkdownViewerInline::Type p_type, const Ref<MarkdownNode> &p_node) {
	MarkdownViewerInline run;
	run.type = p_type;
	run.text = p_node->get_literal();
	run.source = p_node->get_url();
	run.title = p_node->get_title();

	Array children = p_node->get_children();
	for (int i = 0; i < children.size(); i++) {
		Ref<MarkdownNode> child = children[i];
		_append_inlines_from_node(child, run.children);
	}
	if (run.text.is_empty() && !run.children.is_empty()) {
		for (const MarkdownViewerInline &child : run.children) {
			run.text += child.text;
		}
	}
	return run;
}

void _append_inlines_from_node(const Ref<MarkdownNode> &p_node, Vector<MarkdownViewerInline> &r_inlines) {
	if (p_node.is_null()) {
		return;
	}

	switch (p_node->get_type()) {
		case MarkdownNode::NMARK_NODE_TEXT: {
			_append_text_with_strikethrough(p_node->get_literal(), r_inlines);
		} break;
		case MarkdownNode::NMARK_NODE_CODE: {
			MarkdownViewerInline run;
			run.type = MarkdownViewerInline::TYPE_CODE;
			run.text = p_node->get_literal();
			r_inlines.push_back(run);
		} break;
		case MarkdownNode::NMARK_NODE_LINK: {
			r_inlines.push_back(_make_inline(MarkdownViewerInline::TYPE_LINK, p_node));
		} break;
		case MarkdownNode::NMARK_NODE_IMAGE: {
			MarkdownViewerInline run = _make_inline(MarkdownViewerInline::TYPE_IMAGE, p_node);
			run.text = _flatten_child_text(p_node);
			r_inlines.push_back(run);
		} break;
		case MarkdownNode::NMARK_NODE_STRONG: {
			r_inlines.push_back(_make_inline(MarkdownViewerInline::TYPE_STRONG, p_node));
		} break;
		case MarkdownNode::NMARK_NODE_EMPH: {
			r_inlines.push_back(_make_inline(MarkdownViewerInline::TYPE_EMPHASIS, p_node));
		} break;
		default: {
			Array children = p_node->get_children();
			for (int i = 0; i < children.size(); i++) {
				Ref<MarkdownNode> child = children[i];
				_append_inlines_from_node(child, r_inlines);
			}
		} break;
	}
}

MarkdownViewerBlock _make_block_from_node(const Ref<MarkdownNode> &p_node) {
	MarkdownViewerBlock block;
	if (p_node.is_null()) {
		return block;
	}

	switch (p_node->get_type()) {
		case MarkdownNode::NMARK_NODE_HEADING: {
			block.type = MarkdownViewerBlock::TYPE_HEADING;
			block.heading_level = p_node->get_heading_level();
			_append_inlines_from_node(p_node, block.inlines);
			block.plain_text = _flatten_viewer_inlines(block.inlines);
		} break;
		case MarkdownNode::NMARK_NODE_PARAGRAPH: {
			Vector<MarkdownViewerInline> inlines;
			_append_inlines_from_node(p_node, inlines);
			if (inlines.size() == 1 && inlines[0].type == MarkdownViewerInline::TYPE_IMAGE) {
				block.type = MarkdownViewerBlock::TYPE_IMAGE;
				block.inlines = inlines;
				block.source = inlines[0].source;
				block.title = inlines[0].title;
				block.plain_text = inlines[0].text;
			} else {
				block.type = MarkdownViewerBlock::TYPE_PARAGRAPH;
				block.inlines = inlines;
				block.plain_text = _flatten_viewer_inlines(block.inlines);
			}
		} break;
		case MarkdownNode::NMARK_NODE_LIST: {
			block.type = MarkdownViewerBlock::TYPE_LIST;
			block.ordered_list = p_node->get_list_style() == MarkdownNode::NMARK_LIST_STYLE_ORDERED;
			Array children = p_node->get_children();
			for (int i = 0; i < children.size(); i++) {
				Ref<MarkdownNode> child = children[i];
				block.children.push_back(_make_block_from_node(child));
			}
		} break;
		case MarkdownNode::NMARK_NODE_LIST_ITEM: {
			block.type = MarkdownViewerBlock::TYPE_LIST_ITEM;
			Array children = p_node->get_children();
			for (int i = 0; i < children.size(); i++) {
				Ref<MarkdownNode> child = children[i];
				if (child.is_valid() && child->get_type() == MarkdownNode::NMARK_NODE_PARAGRAPH) {
					Vector<MarkdownViewerInline> paragraph_inlines;
					_append_inlines_from_node(child, paragraph_inlines);
					block.inlines.append_array(paragraph_inlines);
					block.plain_text += _flatten_viewer_inlines(paragraph_inlines);
				} else {
					block.children.push_back(_make_block_from_node(child));
				}
			}
		} break;
		case MarkdownNode::NMARK_NODE_CODE_BLOCK: {
			block.type = MarkdownViewerBlock::TYPE_CODE_BLOCK;
			block.language = p_node->get_fence_info().strip_edges();
			block.plain_text = p_node->get_literal();
		} break;
		case MarkdownNode::NMARK_NODE_BLOCK_QUOTE: {
			block.type = MarkdownViewerBlock::TYPE_BLOCK_QUOTE;
			Array children = p_node->get_children();
			for (int i = 0; i < children.size(); i++) {
				Ref<MarkdownNode> child = children[i];
				block.children.push_back(_make_block_from_node(child));
			}
			block.plain_text = _flatten_child_text(p_node);
		} break;
		case MarkdownNode::NMARK_NODE_THEMATIC_BREAK: {
			block.type = MarkdownViewerBlock::TYPE_THEMATIC_BREAK;
		} break;
		default: {
			block.type = MarkdownViewerBlock::TYPE_PARAGRAPH;
			_append_inlines_from_node(p_node, block.inlines);
			block.plain_text = _flatten_viewer_inlines(block.inlines);
		} break;
	}

	return block;
}

} // namespace

String MarkdownViewerDocumentBuilder::_flatten_inlines(const Vector<MarkdownViewerInline> &p_inlines) const {
	return _flatten_viewer_inlines(p_inlines);
}

Vector<MarkdownViewerInline> MarkdownViewerDocumentBuilder::_parse_inline_markdown(const String &p_markdown) const {
	Vector<MarkdownViewerInline> inlines;
	MarkdownParser parser;
	Ref<MarkdownNode> root = parser.parse_markdown(p_markdown);
	if (root.is_null()) {
		MarkdownViewerInline run;
		run.type = MarkdownViewerInline::TYPE_TEXT;
		run.text = p_markdown;
		inlines.push_back(run);
		return inlines;
	}

	Array children = root->get_children();
	for (int i = 0; i < children.size(); i++) {
		Ref<MarkdownNode> child = children[i];
		if (child.is_valid() && child->get_type() == MarkdownNode::NMARK_NODE_PARAGRAPH) {
			_append_inlines_from_node(child, inlines);
		}
	}
	if (inlines.is_empty() && !p_markdown.is_empty()) {
		MarkdownViewerInline run;
		run.type = MarkdownViewerInline::TYPE_TEXT;
		run.text = p_markdown;
		inlines.push_back(run);
	}
	return inlines;
}

void MarkdownViewerDocumentBuilder::_append_markdown_range(const String &p_markdown, MarkdownViewerDocument &r_document) const {
	if (p_markdown.strip_edges().is_empty()) {
		return;
	}

	MarkdownParser parser;
	Ref<MarkdownNode> root = parser.parse_markdown(p_markdown);
	if (root.is_null()) {
		MarkdownViewerBlock block;
		block.type = MarkdownViewerBlock::TYPE_PARAGRAPH;
		block.plain_text = p_markdown.strip_edges();
		MarkdownViewerInline run;
		run.text = block.plain_text;
		block.inlines.push_back(run);
		r_document.blocks.push_back(block);
		return;
	}

	Array children = root->get_children();
	for (int i = 0; i < children.size(); i++) {
		Ref<MarkdownNode> child = children[i];
		if (child.is_valid()) {
			r_document.blocks.push_back(_make_block_from_node(child));
		}
	}
}

bool MarkdownViewerDocumentBuilder::_try_parse_pipe_table(const Vector<String> &p_lines, int p_start, MarkdownViewerBlock &r_block, int &r_end) const {
	if (p_start + 1 >= p_lines.size()) {
		return false;
	}
	if (!p_lines[p_start].contains("|") || !_is_table_separator_line(p_lines[p_start + 1])) {
		return false;
	}

	Vector<String> header_cells = _strip_table_edge_pipes(p_lines[p_start]).split("|", false);
	Vector<String> separator_cells = _strip_table_edge_pipes(p_lines[p_start + 1]).split("|", false);
	if (header_cells.size() == 0 || header_cells.size() != separator_cells.size()) {
		return false;
	}

	r_block = MarkdownViewerBlock();
	r_block.type = MarkdownViewerBlock::TYPE_TABLE;

	MarkdownViewerTableRow header;
	header.header = true;
	for (int i = 0; i < header_cells.size(); i++) {
		MarkdownViewerTableCell cell;
		cell.inlines = _parse_inline_markdown(header_cells[i].strip_edges());
		cell.plain_text = _flatten_inlines(cell.inlines);
		header.cells.push_back(cell);
	}
	r_block.table_rows.push_back(header);

	int row_index = p_start + 2;
	while (row_index < p_lines.size()) {
		const String line = p_lines[row_index];
		if (line.strip_edges().is_empty() || !line.contains("|")) {
			break;
		}

		Vector<String> row_cells = _strip_table_edge_pipes(line).split("|", false);
		MarkdownViewerTableRow row;
		for (int i = 0; i < header_cells.size(); i++) {
			const String cell_text = i < row_cells.size() ? row_cells[i].strip_edges() : String();
			MarkdownViewerTableCell cell;
			cell.inlines = _parse_inline_markdown(cell_text);
			cell.plain_text = _flatten_inlines(cell.inlines);
			row.cells.push_back(cell);
		}
		r_block.table_rows.push_back(row);
		row_index++;
	}

	if (r_block.table_rows.size() < 2) {
		return false;
	}

	r_end = row_index;
	return true;
}

MarkdownViewerDocument MarkdownViewerDocumentBuilder::build(const String &p_markdown) const {
	MarkdownViewerDocument document;
	Vector<String> lines = p_markdown.split("\n", true);
	String pending;

	for (int i = 0; i < lines.size();) {
		MarkdownViewerBlock table;
		int table_end = i;
		if (_try_parse_pipe_table(lines, i, table, table_end)) {
			_append_markdown_range(pending, document);
			pending.clear();
			document.blocks.push_back(table);
			i = table_end;
			continue;
		}

		if (!pending.is_empty()) {
			pending += "\n";
		}
		pending += lines[i];
		i++;
	}

	_append_markdown_range(pending, document);
	return document;
}
