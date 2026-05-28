/**************************************************************************/
/*  markdown_viewer_layout.cpp                                            */
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

Vector<MarkdownViewerLayoutLine> _wrap_inline_spans(const Vector<MarkdownViewerLayoutSpan> &p_spans, int p_chars_per_line) {
	Vector<MarkdownViewerLayoutLine> lines;
	MarkdownViewerLayoutLine line;
	const int max_chars = MAX(1, p_chars_per_line);

	for (const MarkdownViewerLayoutSpan &span : p_spans) {
		for (int i = 0; i < span.text.length(); i++) {
			const char32_t c = span.text[i];
			if (c == '\n') {
				lines.push_back(line);
				line = MarkdownViewerLayoutLine();
				continue;
			}

			_append_span_text(line, String::chr(c), span.style_flags, span.source);
			if (line.text.length() >= max_chars) {
				lines.push_back(line);
				line = MarkdownViewerLayoutLine();
			}
		}
	}

	if (!line.text.is_empty() || lines.is_empty()) {
		lines.push_back(line);
	}
	return lines;
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
	if (p_font.is_valid()) {
		return p_font->get_string_size(p_text, HORIZONTAL_ALIGNMENT_LEFT, -1, p_font_size).x;
	}
	return p_text.length() * p_font_size * 0.55;
}

void _assign_inline_span_rects(MarkdownViewerLayoutItem &r_item, const MarkdownViewerLayoutTheme &p_theme) {
	const real_t line_height = r_item.font_size + p_theme.line_spacing;
	for (int line_index = 0; line_index < r_item.inline_lines.size(); line_index++) {
		MarkdownViewerLayoutLine &line = r_item.inline_lines.write[line_index];
		real_t x = r_item.rect.position.x;
		const real_t line_y = r_item.rect.position.y + line_index * line_height;
		for (int span_index = 0; span_index < line.spans.size(); span_index++) {
			MarkdownViewerLayoutSpan &span = line.spans.write[span_index];
			const Ref<Font> font = _get_span_font(span, p_theme);
			const real_t width = _measure_text(font, span.text, r_item.font_size);
			span.rect = Rect2(x, line_y, width, line_height);
			x += width;
		}
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
	const int chars_per_line = MAX(8, int(available_width / MAX(4, p_font_size * 0.55)));
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
	item.inline_lines = _wrap_inline_spans(spans, chars_per_line);
	for (const MarkdownViewerLayoutLine &line : item.inline_lines) {
		item.lines.push_back(line.text);
	}
	item.rect = Rect2(left, r_y, available_width, MAX(real_t(p_font_size + p_theme.line_spacing), item.inline_lines.size() * (p_font_size + p_theme.line_spacing)));
	item.accent_color = p_theme.font_color;
	item.marker_text = p_marker_text;
	item.marker_unordered = p_marker_unordered;
	if (has_marker) {
		item.marker_rect = Rect2(marker_left, r_y, marker_width, p_font_size + p_theme.line_spacing);
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
			const real_t height = p_theme.code_padding * 2.0 + 24.0 + MAX(1, code.lines.size()) * (p_theme.code_font_size + p_theme.line_spacing);
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
			const real_t column_width = width / column_count;
			const real_t row_height = p_theme.normal_font_size + p_theme.table_cell_padding * 2.0;

			MarkdownViewerLayoutItem table;
			table.type = MarkdownViewerBlock::TYPE_TABLE;
			table.font_size = p_theme.normal_font_size;
			table.rect = Rect2(left, r_y, width, row_height * p_block.table_rows.size());
			for (int row = 0; row < p_block.table_rows.size(); row++) {
				for (int column = 0; column < column_count; column++) {
					table.cell_rects.push_back(Rect2(left + column * column_width, r_y + row * row_height, column_width, row_height));
					const MarkdownViewerTableCell &cell = p_block.table_rows[row].cells[column];
					table.cell_texts.push_back(cell.plain_text);
					table.cell_headers.push_back(p_block.table_rows[row].header);
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
