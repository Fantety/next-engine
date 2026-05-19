/**************************************************************************/
/*  ai_markdown_label.cpp                                                 */
/**************************************************************************/

#include "ai_markdown_label.h"

#include "ai_markdown_renderer.h"

#include "core/markdown/markdown_parser.h"
#include "core/object/class_db.h"
#include "core/templates/vector.h"
#include "scene/gui/rich_text_label.h"
#include "servers/text/text_server.h"

namespace {

struct AIMarkdownTableBlock {
	Vector<Vector<String>> rows;
};

bool _is_table_separator_cell(const String &p_cell) {
	String cell = p_cell.strip_edges();
	if (cell.begins_with(":")) {
		cell = cell.substr(1).strip_edges();
	}
	if (cell.ends_with(":")) {
		cell = cell.substr(0, cell.length() - 1).strip_edges();
	}
	if (cell.length() < 3) {
		return false;
	}
	for (int i = 0; i < cell.length(); i++) {
		if (cell[i] != '-') {
			return false;
		}
	}
	return true;
}

Vector<String> _split_table_row(const String &p_line) {
	String line = p_line.strip_edges();
	if (line.begins_with("|")) {
		line = line.substr(1);
	}
	if (line.ends_with("|")) {
		line = line.substr(0, line.length() - 1);
	}

	Vector<String> cells;
	String current;
	bool escaped = false;
	for (int i = 0; i < line.length(); i++) {
		const char32_t c = line[i];
		if (escaped) {
			if (c != '|') {
				current += '\\';
			}
			current += c;
			escaped = false;
			continue;
		}
		if (c == '\\') {
			escaped = true;
			continue;
		}
		if (c == '|') {
			cells.push_back(current.strip_edges());
			current.clear();
			continue;
		}
		current += c;
	}
	if (escaped) {
		current += '\\';
	}
	cells.push_back(current.strip_edges());
	return cells;
}

bool _is_table_separator_line(const String &p_line, int p_expected_columns) {
	Vector<String> cells = _split_table_row(p_line);
	if (cells.size() != p_expected_columns) {
		return false;
	}
	for (int i = 0; i < cells.size(); i++) {
		if (!_is_table_separator_cell(cells[i])) {
			return false;
		}
	}
	return true;
}

bool _looks_like_table_row(const String &p_line, int p_expected_columns) {
	if (!p_line.contains("|")) {
		return false;
	}
	Vector<String> cells = _split_table_row(p_line);
	return cells.size() == p_expected_columns;
}

bool _try_parse_table_block(const PackedStringArray &p_lines, int p_start, AIMarkdownTableBlock &r_table, int &r_end) {
	if (p_start + 1 >= p_lines.size()) {
		return false;
	}

	Vector<String> header = _split_table_row(p_lines[p_start]);
	if (header.size() < 2 || !_is_table_separator_line(p_lines[p_start + 1], header.size())) {
		return false;
	}

	r_table.rows.clear();
	r_table.rows.push_back(header);
	int line_index = p_start + 2;
	while (line_index < p_lines.size() && _looks_like_table_row(p_lines[line_index], header.size())) {
		r_table.rows.push_back(_split_table_row(p_lines[line_index]));
		line_index++;
	}

	r_end = line_index;
	return true;
}

bool _is_fenced_code_line(const String &p_line, String &r_fence_marker) {
	String line = p_line.strip_edges();
	if (line.begins_with("```")) {
		r_fence_marker = "```";
		return true;
	}
	if (line.begins_with("~~~")) {
		r_fence_marker = "~~~";
		return true;
	}
	return false;
}

void _append_plain_markdown(RichTextLabel *p_label, AIMarkdownRenderer &p_renderer, const String &p_markdown) {
	if (p_markdown.strip_edges().is_empty()) {
		return;
	}

	MarkdownParser parser;
	Ref<MarkdownNode> root = parser.parse_markdown(p_markdown);
	if (root.is_null()) {
		p_label->add_text(p_markdown);
		return;
	}

	p_renderer.render_append(p_label, root);
}

void _append_table(RichTextLabel *p_label, AIMarkdownRenderer &p_renderer, const AIMarkdownTableBlock &p_table) {
	if (p_table.rows.is_empty()) {
		return;
	}

	const int column_count = p_table.rows[0].size();
	if (column_count <= 0) {
		return;
	}

	p_label->push_table(column_count);
	for (int i = 0; i < column_count; i++) {
		p_label->set_table_column_expand(i, true);
	}

	for (int row = 0; row < p_table.rows.size(); row++) {
		for (int column = 0; column < column_count; column++) {
			p_label->push_cell();
			const bool header = row == 0;
			const bool pushed_bold = header && p_label->get_theme_font(SNAME("bold_font")).is_valid();
			if (pushed_bold) {
				p_label->push_bold();
			}
			p_renderer.render_inline_markdown(p_label, p_table.rows[row][column]);
			if (pushed_bold) {
				p_label->pop();
			}
			p_label->pop();
		}
	}
	p_label->pop();
	p_label->add_newline();
	p_label->add_newline();
}

void _render_markdown_with_tables(RichTextLabel *p_label, AIMarkdownRenderer &p_renderer, const String &p_markdown) {
	PackedStringArray lines = p_markdown.split("\n", true);
	String pending_markdown;
	bool in_fenced_code = false;
	String fence_marker;
	int i = 0;
	while (i < lines.size()) {
		if (!in_fenced_code) {
			AIMarkdownTableBlock table;
			int table_end = i;
			if (_try_parse_table_block(lines, i, table, table_end)) {
				_append_plain_markdown(p_label, p_renderer, pending_markdown);
				pending_markdown.clear();
				_append_table(p_label, p_renderer, table);
				i = table_end;
				continue;
			}
		}

		if (!pending_markdown.is_empty()) {
			pending_markdown += "\n";
		}
		pending_markdown += lines[i];

		String current_fence_marker;
		if (in_fenced_code) {
			if (lines[i].strip_edges().begins_with(fence_marker)) {
				in_fenced_code = false;
				fence_marker.clear();
			}
		} else if (_is_fenced_code_line(lines[i], current_fence_marker)) {
			in_fenced_code = true;
			fence_marker = current_fence_marker;
		}
		i++;
	}

	_append_plain_markdown(p_label, p_renderer, pending_markdown);
}

} // namespace

void AIMarkdownLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_markdown", "markdown"), &AIMarkdownLabel::set_markdown);
	ClassDB::bind_method(D_METHOD("get_markdown"), &AIMarkdownLabel::get_markdown);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "markdown"), "set_markdown", "get_markdown");
}

AIMarkdownLabel::AIMarkdownLabel() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);

	rich_text_label = memnew(RichTextLabel);
	rich_text_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	rich_text_label->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	rich_text_label->set_fit_content(true);
	rich_text_label->set_selection_enabled(true);
	rich_text_label->set_use_bbcode(false);
	rich_text_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	add_child(rich_text_label);
}

void AIMarkdownLabel::set_markdown(const String &p_markdown) {
	markdown_text = p_markdown;
	clear();

	if (markdown_text.is_empty()) {
		return;
	}

	AIMarkdownRenderer renderer;
	renderer.set_heading_base_size(rich_text_label->get_theme_font_size(SNAME("normal_font_size")));
	_render_markdown_with_tables(rich_text_label, renderer, markdown_text);
}

String AIMarkdownLabel::get_markdown() const {
	return markdown_text;
}

void AIMarkdownLabel::clear() {
	rich_text_label->clear();
}

void AIMarkdownLabel::add_text(const String &p_text) {
	rich_text_label->add_text(p_text);
}

String AIMarkdownLabel::get_parsed_text() const {
	return rich_text_label->get_parsed_text();
}

RichTextLabel *AIMarkdownLabel::get_rich_text_label() const {
	return rich_text_label;
}

void AIMarkdownLabel::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	rich_text_label->set_autowrap_mode(p_mode);
}

void AIMarkdownLabel::add_theme_font_size_override(const StringName &p_name, int p_font_size) {
	rich_text_label->add_theme_font_size_override(p_name, p_font_size);
}

void AIMarkdownLabel::remove_theme_font_size_override(const StringName &p_name) {
	rich_text_label->remove_theme_font_size_override(p_name);
}
