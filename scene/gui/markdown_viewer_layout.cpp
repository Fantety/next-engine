/**************************************************************************/
/*  markdown_viewer_layout.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "markdown_viewer_layout.h"

namespace {

bool _same_span_style(const MarkdownViewerLayoutSpan &p_span, int p_style_flags, const String &p_source) {
	return p_span.style_flags == p_style_flags && p_span.source == p_source;
}

void _append_span_text(MarkdownViewerLayoutLine &r_line, const String &p_text, int p_style_flags, const String &p_source) {
	if (p_text.is_empty()) {
		return;
	}

	if (!r_line.spans.is_empty() && _same_span_style(r_line.spans[r_line.spans.size() - 1], p_style_flags, p_source)) {
		r_line.spans.write[r_line.spans.size() - 1].text += p_text;
	} else {
		MarkdownViewerLayoutSpan span;
		span.text = p_text;
		span.source = p_source;
		span.style_flags = p_style_flags;
		r_line.spans.push_back(span);
	}
	r_line.text += p_text;
}

String _flatten_inline_text(const Vector<MarkdownViewerInline> &p_inlines) {
	String text;
	for (const MarkdownViewerInline &run : p_inlines) {
		if (run.children.is_empty()) {
			text += run.text;
		} else {
			text += _flatten_inline_text(run.children);
		}
	}
	return text;
}

void _append_layout_span(const String &p_text, int p_style_flags, const String &p_source, Vector<MarkdownViewerLayoutSpan> &r_spans) {
	if (p_text.is_empty()) {
		return;
	}

	if (!r_spans.is_empty() && _same_span_style(r_spans[r_spans.size() - 1], p_style_flags, p_source)) {
		r_spans.write[r_spans.size() - 1].text += p_text;
	} else {
		MarkdownViewerLayoutSpan span;
		span.text = p_text;
		span.source = p_source;
		span.style_flags = p_style_flags;
		r_spans.push_back(span);
	}
}

void _append_layout_spans_from_inlines(const Vector<MarkdownViewerInline> &p_inlines, int p_style_flags, const String &p_link_source, Vector<MarkdownViewerLayoutSpan> &r_spans) {
	for (const MarkdownViewerInline &run : p_inlines) {
		int style_flags = p_style_flags;
		String source = p_link_source;

		switch (run.type) {
			case MarkdownViewerInline::TYPE_STRONG:
				style_flags |= MarkdownViewerLayoutSpan::STYLE_STRONG;
				break;
			case MarkdownViewerInline::TYPE_EMPHASIS:
				style_flags |= MarkdownViewerLayoutSpan::STYLE_EMPHASIS;
				break;
			case MarkdownViewerInline::TYPE_CODE:
				style_flags |= MarkdownViewerLayoutSpan::STYLE_CODE;
				break;
			case MarkdownViewerInline::TYPE_LINK:
				style_flags |= MarkdownViewerLayoutSpan::STYLE_LINK;
				source = run.source;
				break;
			case MarkdownViewerInline::TYPE_STRIKETHROUGH:
				style_flags |= MarkdownViewerLayoutSpan::STYLE_STRIKETHROUGH;
				break;
			default:
				break;
		}

		if (run.children.is_empty() || run.type == MarkdownViewerInline::TYPE_CODE) {
			String text = run.text;
			if (text.is_empty() && run.type == MarkdownViewerInline::TYPE_LINK) {
				text = run.source;
			} else if (text.is_empty() && run.type == MarkdownViewerInline::TYPE_IMAGE) {
				text = run.source;
			}
			_append_layout_span(text, style_flags, source, r_spans);
		} else {
			_append_layout_spans_from_inlines(run.children, style_flags, source, r_spans);
		}
	}
}

Ref<Font> _get_span_font(const MarkdownViewerLayoutSpan &p_span, const MarkdownViewerLayoutTheme &p_theme) {
	if (p_span.style_flags & MarkdownViewerLayoutSpan::STYLE_CODE) {
		return p_theme.mono_font.is_valid() ? p_theme.mono_font : p_theme.font;
	}
	if ((p_span.style_flags & MarkdownViewerLayoutSpan::STYLE_STRONG) && (p_span.style_flags & MarkdownViewerLayoutSpan::STYLE_EMPHASIS) && p_theme.bold_italic_font.is_valid()) {
		return p_theme.bold_italic_font;
	}
	if ((p_span.style_flags & MarkdownViewerLayoutSpan::STYLE_STRONG) && p_theme.bold_font.is_valid()) {
		return p_theme.bold_font;
	}
	if ((p_span.style_flags & MarkdownViewerLayoutSpan::STYLE_EMPHASIS) && p_theme.italic_font.is_valid()) {
		return p_theme.italic_font;
	}
	return p_theme.font;
}

real_t _measure_text(const Ref<Font> &p_font, const String &p_text, int p_font_size) {
	if (p_text.is_empty()) {
		return 0.0;
	}
	if (p_font.is_valid()) {
		const real_t width = p_font->get_string_size(p_text, HORIZONTAL_ALIGNMENT_LEFT, -1, p_font_size).x;
		if (width > 0.0) {
			return width;
		}
	}
	return p_text.length() * p_font_size * 0.55;
}

real_t _get_font_height(const Ref<Font> &p_font, int p_font_size) {
	if (p_font.is_valid()) {
		const real_t height = p_font->get_height(p_font_size);
		if (height > 0.0) {
			return height;
		}
	}
	return p_font_size;
}

real_t _get_font_ascent(const Ref<Font> &p_font, int p_font_size) {
	if (p_font.is_valid()) {
		const real_t ascent = p_font->get_ascent(p_font_size);
		if (ascent > 0.0) {
			return ascent;
		}
	}
	return p_font_size;
}

real_t _get_line_advance(const Ref<Font> &p_font, int p_font_size, const MarkdownViewerLayoutTheme &p_theme) {
	return MAX(real_t(p_font_size), _get_font_height(p_font, p_font_size)) + p_theme.line_spacing;
}

bool _is_inline_wrap_space(char32_t p_char) {
	return p_char == ' ' || p_char == '\t';
}

void _push_inline_line(Vector<MarkdownViewerLayoutLine> &r_lines, MarkdownViewerLayoutLine &r_line, real_t &r_line_width) {
	r_lines.push_back(r_line);
	r_line = MarkdownViewerLayoutLine();
	r_line_width = 0.0;
}

Vector<MarkdownViewerLayoutLine> _wrap_inline_spans(const Vector<MarkdownViewerLayoutSpan> &p_spans, real_t p_available_width, int p_font_size, const MarkdownViewerLayoutTheme &p_theme) {
	Vector<MarkdownViewerLayoutLine> lines;
	MarkdownViewerLayoutLine line;
	real_t line_width = 0.0;
	const real_t max_width = MAX(real_t(1.0), p_available_width);

	for (const MarkdownViewerLayoutSpan &span : p_spans) {
		const Ref<Font> font = _get_span_font(span, p_theme);
		for (int i = 0; i < span.text.length();) {
			const char32_t c = span.text[i];
			if (c == '\n') {
				_push_inline_line(lines, line, line_width);
				i++;
				continue;
			}

			const bool token_is_space = _is_inline_wrap_space(c);
			const int token_start = i;
			while (i < span.text.length()) {
				const char32_t token_char = span.text[i];
				if (token_char == '\n' || _is_inline_wrap_space(token_char) != token_is_space) {
					break;
				}
				i++;
			}

			const String token = span.text.substr(token_start, i - token_start);
			if (token.is_empty()) {
				continue;
			}
			if (token_is_space) {
				if (line.text.is_empty()) {
					continue;
				}
				const real_t token_width = _measure_text(font, token, p_font_size);
				if (line_width + token_width > max_width) {
					_push_inline_line(lines, line, line_width);
					continue;
				}
				_append_span_text(line, token, span.style_flags, span.source);
				line_width += token_width;
				continue;
			}

			const real_t token_width = _measure_text(font, token, p_font_size);
			if (token_width <= max_width) {
				if (!line.text.is_empty() && line_width + token_width > max_width) {
					_push_inline_line(lines, line, line_width);
				}
				_append_span_text(line, token, span.style_flags, span.source);
				line_width += token_width;
				continue;
			}

			for (int char_index = 0; char_index < token.length(); char_index++) {
				const String char_text = String::chr(token[char_index]);
				const real_t char_width = _measure_text(font, char_text, p_font_size);
				if (!line.text.is_empty() && line_width + char_width > max_width) {
					_push_inline_line(lines, line, line_width);
				}
				_append_span_text(line, char_text, span.style_flags, span.source);
				line_width += char_width;
			}
		}
	}

	if (!line.text.is_empty() || lines.is_empty()) {
		lines.push_back(line);
	}
	return lines;
}

real_t _prepare_inline_line_metrics(MarkdownViewerLayoutItem &r_item, const MarkdownViewerLayoutTheme &p_theme) {
	real_t total_height = 0.0;
	for (int line_index = 0; line_index < r_item.inline_lines.size(); line_index++) {
		MarkdownViewerLayoutLine &line = r_item.inline_lines.write[line_index];
		real_t line_height = _get_line_advance(p_theme.font, r_item.font_size, p_theme);
		real_t baseline = _get_font_ascent(p_theme.font, r_item.font_size);
		for (const MarkdownViewerLayoutSpan &span : line.spans) {
			const Ref<Font> span_font = _get_span_font(span, p_theme);
			line_height = MAX(line_height, _get_line_advance(span_font, r_item.font_size, p_theme));
			baseline = MAX(baseline, _get_font_ascent(span_font, r_item.font_size));
		}
		line.height = line_height;
		line.baseline = baseline;
		total_height += line.height;
	}
	return total_height;
}

void _assign_inline_span_rects(MarkdownViewerLayoutItem &r_item, const MarkdownViewerLayoutTheme &p_theme) {
	real_t line_y = r_item.rect.position.y;
	for (int line_index = 0; line_index < r_item.inline_lines.size(); line_index++) {
		MarkdownViewerLayoutLine &line = r_item.inline_lines.write[line_index];
		real_t x = r_item.rect.position.x;
		for (int span_index = 0; span_index < line.spans.size(); span_index++) {
			MarkdownViewerLayoutSpan &span = line.spans.write[span_index];
			const Ref<Font> font = _get_span_font(span, p_theme);
			const real_t width = _measure_text(font, span.text, r_item.font_size);
			span.rect = Rect2(x, line_y, width, line.height);
			x += width;
		}
		line_y += line.height;
	}
}

} // namespace

Vector<String> MarkdownViewerLayoutBuilder::_wrap_text(const String &p_text, int p_chars_per_line) {
	Vector<String> lines;
	const int max_chars = MAX(1, p_chars_per_line);
	Vector<String> source_lines = p_text.split("\n", true);

	for (int i = 0; i < source_lines.size(); i++) {
		String line = source_lines[i];
		if (line.is_empty()) {
			lines.push_back(String());
			continue;
		}
		while (line.length() > max_chars) {
			int break_at = max_chars;
			for (int j = max_chars; j > MAX(0, max_chars - 16); j--) {
				if (line[j] == ' ' || line[j] == '\t') {
					break_at = j;
					break;
				}
			}
			lines.push_back(line.substr(0, break_at).strip_edges());
			line = line.substr(break_at).strip_edges();
		}
		lines.push_back(line);
	}

	return lines;
}

String MarkdownViewerLayoutBuilder::_flatten_block_text(const MarkdownViewerBlock &p_block) {
	if (!p_block.plain_text.is_empty()) {
		return p_block.plain_text;
	}

	return _flatten_inline_text(p_block.inlines);
}

void MarkdownViewerLayoutBuilder::_append_text_block(const MarkdownViewerBlock &p_block, MarkdownViewerLayout &r_layout, real_t &r_y, real_t p_width, const MarkdownViewerLayoutTheme &p_theme, int p_font_size, real_t p_indent, const String &p_marker_text, bool p_marker_unordered) const {
	const bool has_marker = p_marker_unordered || !p_marker_text.is_empty();
	const real_t marker_left = p_theme.document_margin + p_indent;
	const real_t marker_width = has_marker ? MAX(real_t(18.0), _measure_text(p_theme.font, p_marker_text, p_font_size) + 8.0) : 0.0;
	const real_t left = marker_left + marker_width;
	const real_t available_width = MAX(32.0, p_width - left - p_theme.document_margin);
	const String text = _flatten_block_text(p_block);
	Vector<MarkdownViewerLayoutSpan> spans;
	_append_layout_spans_from_inlines(p_block.inlines, MarkdownViewerLayoutSpan::STYLE_NONE, String(), spans);
	if (spans.is_empty() && !text.is_empty()) {
		_append_layout_span(text, MarkdownViewerLayoutSpan::STYLE_NONE, String(), spans);
	}

	MarkdownViewerLayoutItem item;
	item.type = p_block.type;
	item.text = text;
	item.font_size = p_font_size;
	item.inline_lines = _wrap_inline_spans(spans, available_width, p_font_size, p_theme);
	for (const MarkdownViewerLayoutLine &line : item.inline_lines) {
		item.lines.push_back(line.text);
	}
	const real_t inline_height = _prepare_inline_line_metrics(item, p_theme);
	item.rect = Rect2(left, r_y, available_width, MAX(_get_line_advance(p_theme.font, p_font_size, p_theme), inline_height));
	item.accent_color = p_theme.font_color;
	item.marker_text = p_marker_text;
	item.marker_unordered = p_marker_unordered;
	if (has_marker) {
		const real_t marker_height = item.inline_lines.is_empty() ? _get_line_advance(p_theme.font, p_font_size, p_theme) : item.inline_lines[0].height;
		item.marker_rect = Rect2(marker_left, r_y, marker_width, marker_height);
	}
	_assign_inline_span_rects(item, p_theme);

	for (const MarkdownViewerLayoutLine &line : item.inline_lines) {
		for (const MarkdownViewerLayoutSpan &span : line.spans) {
			if (span.style_flags & MarkdownViewerLayoutSpan::STYLE_LINK) {
				MarkdownViewerHitTest hit;
				hit.type = MarkdownViewerHitTest::TYPE_LINK;
				hit.rect = span.rect;
				hit.payload = span.source;
				r_layout.hit_tests.push_back(hit);
			}
		}
	}

	r_layout.items.push_back(item);
	r_y += item.rect.size.y + p_theme.block_spacing;
}

void MarkdownViewerLayoutBuilder::_append_block(const MarkdownViewerBlock &p_block, MarkdownViewerLayout &r_layout, real_t &r_y, real_t p_width, const MarkdownViewerLayoutTheme &p_theme, real_t p_indent) const {
	switch (p_block.type) {
		case MarkdownViewerBlock::TYPE_HEADING: {
			int font_size = p_theme.h3_font_size;
			if (p_block.heading_level <= 1) {
				font_size = p_theme.h1_font_size;
			} else if (p_block.heading_level == 2) {
				font_size = p_theme.h2_font_size;
			}
			_append_text_block(p_block, r_layout, r_y, p_width, p_theme, font_size, p_indent);
		} break;
		case MarkdownViewerBlock::TYPE_LIST: {
			int index = 1;
			for (const MarkdownViewerBlock &child : p_block.children) {
				MarkdownViewerBlock list_item = child;
				list_item.type = MarkdownViewerBlock::TYPE_LIST_ITEM;
				const String marker = p_block.ordered_list ? itos(index) + "." : String();
				const bool unordered_marker = !p_block.ordered_list;
				if (!list_item.inlines.is_empty() || !_flatten_block_text(list_item).is_empty()) {
					_append_text_block(list_item, r_layout, r_y, p_width, p_theme, p_theme.normal_font_size, p_indent + 6.0, marker, unordered_marker);
				}
				for (const MarkdownViewerBlock &nested_child : child.children) {
					_append_block(nested_child, r_layout, r_y, p_width, p_theme, p_indent + 24.0);
				}
				index++;
			}
		} break;
		case MarkdownViewerBlock::TYPE_BLOCK_QUOTE: {
			MarkdownViewerLayoutItem quote;
			quote.type = MarkdownViewerBlock::TYPE_BLOCK_QUOTE;
			quote.text = String();
			quote.font_size = p_theme.normal_font_size;
			const real_t quote_start_y = r_y;
			quote.rect = Rect2(p_theme.document_margin + p_indent, quote_start_y, p_width - p_theme.document_margin * 2.0 - p_indent, 0.0);
			quote.accent_color = p_theme.blockquote_color;
			const int quote_index = r_layout.items.size();
			r_layout.items.push_back(quote);

			if (p_block.children.is_empty()) {
				MarkdownViewerBlock text_block = p_block;
				text_block.type = MarkdownViewerBlock::TYPE_PARAGRAPH;
				_append_text_block(text_block, r_layout, r_y, p_width, p_theme, p_theme.normal_font_size, p_indent + 18.0);
			} else {
				for (const MarkdownViewerBlock &child : p_block.children) {
					_append_block(child, r_layout, r_y, p_width, p_theme, p_indent + 18.0);
				}
			}

			r_layout.items.write[quote_index].rect.size.y = MAX(real_t(p_theme.normal_font_size + p_theme.line_spacing), r_y - quote_start_y - p_theme.block_spacing);
		} break;
		case MarkdownViewerBlock::TYPE_THEMATIC_BREAK: {
			MarkdownViewerLayoutItem thematic_break;
			thematic_break.type = MarkdownViewerBlock::TYPE_THEMATIC_BREAK;
			thematic_break.rect = Rect2(p_theme.document_margin + p_indent, r_y, p_width - p_theme.document_margin * 2.0 - p_indent, 14.0);
			r_layout.items.push_back(thematic_break);
			r_y += thematic_break.rect.size.y + p_theme.block_spacing;
		} break;
		case MarkdownViewerBlock::TYPE_CODE_BLOCK: {
			MarkdownViewerLayoutItem code;
			code.type = MarkdownViewerBlock::TYPE_CODE_BLOCK;
			code.text = p_block.plain_text;
			code.secondary_text = p_block.language;
			code.font_size = p_theme.code_font_size;
			code.lines = p_block.plain_text.split("\n", true);
			if (!code.lines.is_empty() && code.lines[code.lines.size() - 1].is_empty()) {
				code.lines.remove_at(code.lines.size() - 1);
			}
			MarkdownViewerCodeHighlighter highlighter;
			for (const String &line : code.lines) {
				MarkdownViewerLayoutItem::CodeLine code_line;
				code_line.text = line;
				if (p_theme.syntax_highlighting_enabled) {
					code_line.spans = highlighter.highlight_line(p_block.language, line);
				} else {
					MarkdownViewerCodeSpan span;
					span.text = line;
					code_line.spans.push_back(span);
				}
				code.code_lines.push_back(code_line);
			}
			const Ref<Font> code_font = p_theme.mono_font.is_valid() ? p_theme.mono_font : p_theme.font;
			const real_t code_line_height = _get_line_advance(code_font, p_theme.code_font_size, p_theme);
			const real_t height = p_theme.code_padding * 2.0 + 24.0 + MAX(1, code.lines.size()) * code_line_height;
			code.rect = Rect2(p_theme.document_margin + p_indent, r_y, p_width - p_theme.document_margin * 2.0 - p_indent, height);
			r_layout.items.push_back(code);
			if (p_theme.code_copy_enabled) {
				MarkdownViewerHitTest hit;
				hit.type = MarkdownViewerHitTest::TYPE_CODE_COPY;
				hit.rect = Rect2(code.rect.position.x + code.rect.size.x - 66.0, code.rect.position.y + 7.0, 54.0, 22.0);
				hit.payload = p_block.plain_text;
				hit.secondary_payload = p_block.language;
				r_layout.hit_tests.push_back(hit);
			}
			r_y += height + p_theme.block_spacing;
		} break;
		case MarkdownViewerBlock::TYPE_TABLE: {
			const int column_count = p_block.table_rows.is_empty() ? 0 : p_block.table_rows[0].cells.size();
			if (column_count == 0) {
				break;
			}
			const real_t left = p_theme.document_margin + p_indent;
			const real_t width = p_width - p_theme.document_margin * 2.0 - p_indent;
			const real_t minimum_column_width = MAX(real_t(48.0), width / column_count);
			Vector<real_t> column_widths;
			column_widths.resize(column_count);
			for (int column = 0; column < column_count; column++) {
				column_widths.write[column] = minimum_column_width;
			}
			for (const MarkdownViewerTableRow &row : p_block.table_rows) {
				for (int column = 0; column < MIN(column_count, row.cells.size()); column++) {
					const real_t cell_width = _measure_text(p_theme.font, row.cells[column].plain_text, p_theme.normal_font_size) + p_theme.table_cell_padding * 2.0;
					column_widths.write[column] = MAX(column_widths[column], cell_width);
				}
			}
			real_t table_width = 0.0;
			for (int column = 0; column < column_count; column++) {
				table_width += column_widths[column];
			}
			const real_t row_height = _get_font_height(p_theme.font, p_theme.normal_font_size) + p_theme.table_cell_padding * 2.0;

			MarkdownViewerLayoutItem table;
			table.type = MarkdownViewerBlock::TYPE_TABLE;
			table.font_size = p_theme.normal_font_size;
			table.rect = Rect2(left, r_y, table_width, row_height * p_block.table_rows.size());
			table.scroll_viewport_width = width;
			for (int row = 0; row < p_block.table_rows.size(); row++) {
				real_t cell_x = left;
				for (int column = 0; column < column_count; column++) {
					const real_t column_width = column_widths[column];
					table.cell_rects.push_back(Rect2(cell_x, r_y + row * row_height, column_width, row_height));
					const String cell_text = column < p_block.table_rows[row].cells.size() ? p_block.table_rows[row].cells[column].plain_text : String();
					table.cell_texts.push_back(cell_text);
					table.cell_headers.push_back(p_block.table_rows[row].header);
					cell_x += column_width;
				}
			}
			r_layout.items.push_back(table);
			r_y += table.rect.size.y + p_theme.block_spacing;
		} break;
		case MarkdownViewerBlock::TYPE_IMAGE: {
			MarkdownViewerLayoutItem image;
			image.type = MarkdownViewerBlock::TYPE_IMAGE;
			image.text = p_block.plain_text;
			image.secondary_text = p_block.source;
			image.image_status = MarkdownViewerImageStatus::STATUS_EMPTY;
			Size2 image_size(MIN(p_theme.image_max_width, p_width - p_theme.document_margin * 2.0 - p_indent), 140.0);
			if (image_loader) {
				MarkdownViewerImageRequestResult result = const_cast<MarkdownViewerImageLoader *>(image_loader)->ensure_image(p_block.source);
				image.image_status = result.status;
				image.texture = result.texture;
				if (result.texture.is_valid() && result.size.x > 0.0 && result.size.y > 0.0) {
					image_size = result.size;
					const real_t scale = MIN(1.0, MIN(p_theme.image_max_width / image_size.x, p_theme.image_max_height / image_size.y));
					image_size *= scale;
				}
			}
			image.rect = Rect2(p_theme.document_margin + p_indent, r_y, image_size.x, image_size.y);
			r_layout.items.push_back(image);

			MarkdownViewerHitTest hit;
			hit.type = MarkdownViewerHitTest::TYPE_IMAGE;
			hit.rect = image.rect;
			hit.payload = p_block.source;
			r_layout.hit_tests.push_back(hit);

			r_y += image.rect.size.y + p_theme.image_spacing + p_theme.block_spacing;
		} break;
		default: {
			_append_text_block(p_block, r_layout, r_y, p_width, p_theme, p_theme.normal_font_size, p_indent);
		} break;
	}
}

void MarkdownViewerLayoutBuilder::set_image_loader(const MarkdownViewerImageLoader *p_loader) {
	image_loader = p_loader;
}

MarkdownViewerLayout MarkdownViewerLayoutBuilder::build(const MarkdownViewerDocument &p_document, const Size2 &p_viewport_size, const MarkdownViewerLayoutTheme &p_theme) const {
	MarkdownViewerLayout layout;
	real_t y = p_theme.document_margin;
	const real_t width = MAX(real_t(80.0), p_viewport_size.x);

	for (const MarkdownViewerBlock &block : p_document.blocks) {
		_append_block(block, layout, y, width, p_theme);
	}

	layout.content_height = p_document.blocks.is_empty() ? 0.0 : y + p_theme.document_margin - p_theme.block_spacing;
	return layout;
}
