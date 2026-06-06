/**************************************************************************/
/*  markdown_viewer_draw.cpp                                              */
/**************************************************************************/

#include "markdown_viewer_draw.h"

#include "scene/gui/markdown_viewer.h"

namespace {

Color _get_code_span_color(MarkdownViewerCodeSpan::Type p_type, const MarkdownViewerLayoutTheme &p_theme) {
	switch (p_type) {
		case MarkdownViewerCodeSpan::TYPE_KEYWORD:
			return p_theme.link_color;
		case MarkdownViewerCodeSpan::TYPE_STRING:
			return Color(0.86, 0.62, 0.38);
		case MarkdownViewerCodeSpan::TYPE_COMMENT:
			return p_theme.muted_font_color;
		case MarkdownViewerCodeSpan::TYPE_NUMBER:
			return Color(0.7, 0.8, 0.55);
		case MarkdownViewerCodeSpan::TYPE_NORMAL:
		default:
			return p_theme.font_color;
	}
}

Ref<Font> _get_layout_span_font(const MarkdownViewerLayoutSpan &p_span, const MarkdownViewerLayoutTheme &p_theme) {
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

Color _get_layout_span_color(const MarkdownViewerLayoutSpan &p_span, const MarkdownViewerLayoutTheme &p_theme) {
	if (p_span.style_flags & MarkdownViewerLayoutSpan::STYLE_LINK) {
		return p_theme.link_color;
	}
	return p_theme.font_color;
}

real_t _get_font_height(const Ref<Font> &p_font, int p_font_size) {
	if (p_font.is_valid()) {
		return p_font->get_height(p_font_size);
	}
	return p_font_size;
}

real_t _get_font_ascent(const Ref<Font> &p_font, int p_font_size) {
	if (p_font.is_valid()) {
		return p_font->get_ascent(p_font_size);
	}
	return p_font_size;
}

real_t _get_line_advance(const Ref<Font> &p_font, int p_font_size, const MarkdownViewerLayoutTheme &p_theme) {
	return MAX(real_t(p_font_size), _get_font_height(p_font, p_font_size)) + p_theme.line_spacing;
}

} // namespace

Vector<int> MarkdownViewerDrawHelper::collect_visible_item_indices_for_test(const MarkdownViewerLayout &p_layout, const Rect2 &p_viewport_rect, real_t p_scroll_offset) {
	Vector<int> visible;
	for (int i = 0; i < p_layout.items.size(); i++) {
		Rect2 rect = p_layout.items[i].rect;
		rect.position.y -= p_scroll_offset;
		if (rect.intersects(p_viewport_rect)) {
			visible.push_back(i);
		}
	}
	return visible;
}

void MarkdownViewerDrawHelper::_draw_text_lines(MarkdownViewer &p_viewer, const MarkdownViewerLayoutItem &p_item, const MarkdownViewerLayoutTheme &p_theme, real_t p_scroll_offset) {
	Ref<Font> font = p_theme.font;
	if (p_item.type == MarkdownViewerBlock::TYPE_CODE_BLOCK) {
		font = p_theme.mono_font.is_valid() ? p_theme.mono_font : font;
	}
	if (font.is_null()) {
		return;
	}

	const int font_size = p_item.font_size > 0 ? p_item.font_size : (p_item.type == MarkdownViewerBlock::TYPE_CODE_BLOCK ? p_theme.code_font_size : p_theme.normal_font_size);
	const Color color = p_item.type == MarkdownViewerBlock::TYPE_BLOCK_QUOTE ? p_theme.muted_font_color : p_theme.font_color;

	if (p_item.marker_unordered || !p_item.marker_text.is_empty()) {
		Rect2 marker_rect = p_item.marker_rect;
		marker_rect.position.y -= p_scroll_offset;
		const real_t marker_baseline = !p_item.inline_lines.is_empty() ? p_item.inline_lines[0].baseline : _get_font_ascent(font, font_size);
		if (p_item.marker_unordered) {
			p_viewer.draw_circle(Point2(marker_rect.position.x + marker_rect.size.x * 0.5, marker_rect.position.y + marker_baseline - font_size * 0.35), 3.0, p_theme.font_color);
		} else {
			p_viewer.draw_string(font, Point2(marker_rect.position.x, marker_rect.position.y + marker_baseline), p_item.marker_text, HORIZONTAL_ALIGNMENT_LEFT, marker_rect.size.x, font_size, p_theme.muted_font_color);
		}
	}

	if (!p_item.inline_lines.is_empty()) {
		for (const MarkdownViewerLayoutLine &line : p_item.inline_lines) {
			for (const MarkdownViewerLayoutSpan &span : line.spans) {
				Rect2 span_rect = span.rect;
				span_rect.position.y -= p_scroll_offset;
				if (span.style_flags & MarkdownViewerLayoutSpan::STYLE_CODE) {
					Rect2 code_rect = span_rect.grow_individual(3.0, 1.0, 3.0, 1.0);
					p_viewer.draw_rect(code_rect, p_theme.code_background_color, true);
					p_viewer.draw_rect(code_rect, p_theme.code_border_color, false, 1.0);
				}

				Ref<Font> span_font = _get_layout_span_font(span, p_theme);
				if (span_font.is_null()) {
					continue;
				}

				const Color span_color = _get_layout_span_color(span, p_theme);
				p_viewer.draw_string(span_font, Point2(span_rect.position.x, span_rect.position.y + line.baseline), span.text, HORIZONTAL_ALIGNMENT_LEFT, span_rect.size.x, font_size, span_color);
				if (span.style_flags & MarkdownViewerLayoutSpan::STYLE_STRIKETHROUGH) {
					const real_t strike_y = span_rect.position.y + line.baseline - font_size * 0.35;
					p_viewer.draw_line(Point2(span_rect.position.x, strike_y), Point2(span_rect.position.x + span_rect.size.x, strike_y), span_color, 1.0);
				}
			}
		}
		return;
	}

	real_t y = p_item.rect.position.y - p_scroll_offset + _get_font_ascent(font, font_size);
	const real_t x = p_item.rect.position.x + (p_item.type == MarkdownViewerBlock::TYPE_BLOCK_QUOTE ? 14.0 : 0.0);
	const real_t line_height = _get_line_advance(font, font_size, p_theme);
	for (const String &line : p_item.lines) {
		p_viewer.draw_string(font, Point2(x, y), line, HORIZONTAL_ALIGNMENT_LEFT, p_item.rect.size.x, font_size, color);
		y += line_height;
	}
}

void MarkdownViewerDrawHelper::draw(MarkdownViewer &p_viewer, const MarkdownViewerLayout &p_layout, const MarkdownViewerLayoutTheme &p_theme, real_t p_scroll_offset) {
	const Rect2 viewport_rect(Point2(), p_viewer.get_size());
	const Vector<int> visible = collect_visible_item_indices_for_test(p_layout, viewport_rect, p_scroll_offset);

	for (int item_index : visible) {
		const MarkdownViewerLayoutItem &item = p_layout.items[item_index];
		Rect2 rect = item.rect;
		rect.position.y -= p_scroll_offset;

		switch (item.type) {
			case MarkdownViewerBlock::TYPE_CODE_BLOCK: {
				p_viewer.draw_rect(rect, p_theme.code_background_color, true);
				p_viewer.draw_rect(rect, p_theme.code_border_color, false, 1.0);
				if (!item.secondary_text.is_empty() && p_theme.font.is_valid()) {
					p_viewer.draw_string(p_theme.font, rect.position + Point2(p_theme.code_padding, 20.0), item.secondary_text, HORIZONTAL_ALIGNMENT_LEFT, rect.size.x, p_theme.normal_font_size, p_theme.muted_font_color);
				}
				if (p_theme.code_copy_enabled && p_theme.font.is_valid()) {
					p_viewer.draw_rect(Rect2(rect.position.x + rect.size.x - 66.0, rect.position.y + 7.0, 54.0, 22.0), p_theme.code_border_color, true);
					p_viewer.draw_string(p_theme.font, Point2(rect.position.x + rect.size.x - 56.0, rect.position.y + 23.0), "Copy", HORIZONTAL_ALIGNMENT_LEFT, 48.0, p_theme.code_font_size, p_theme.font_color);
				}

				Ref<Font> code_font = p_theme.mono_font.is_valid() ? p_theme.mono_font : p_theme.font;
				if (code_font.is_valid()) {
					real_t y = rect.position.y + 28.0 + _get_font_ascent(code_font, p_theme.code_font_size);
					const real_t start_x = rect.position.x + p_theme.code_padding;
					const real_t limit = MAX(0.0, rect.size.x - p_theme.code_padding * 2.0);
					const real_t code_line_height = _get_line_advance(code_font, p_theme.code_font_size, p_theme);
					for (const MarkdownViewerLayoutItem::CodeLine &line : item.code_lines) {
						real_t x = start_x;
						for (const MarkdownViewerCodeSpan &span : line.spans) {
							const Color span_color = _get_code_span_color(span.type, p_theme);
							p_viewer.draw_string(code_font, Point2(x, y), span.text, HORIZONTAL_ALIGNMENT_LEFT, limit, p_theme.code_font_size, span_color);
							x += code_font->get_string_size(span.text, HORIZONTAL_ALIGNMENT_LEFT, -1, p_theme.code_font_size).x;
						}
						y += code_line_height;
					}
				}
			} break;
			case MarkdownViewerBlock::TYPE_TABLE: {
				for (int i = 0; i < item.cell_rects.size(); i++) {
					Rect2 cell_rect = item.cell_rects[i];
					cell_rect.position.y -= p_scroll_offset;
					p_viewer.draw_rect(cell_rect, item.cell_headers[i] ? p_theme.table_header_background_color : p_theme.table_row_background_color, true);
					p_viewer.draw_rect(cell_rect, p_theme.table_border_color, false, 1.0);
					if (p_theme.font.is_valid()) {
						p_viewer.draw_string(p_theme.font, cell_rect.position + Point2(p_theme.table_cell_padding, p_theme.table_cell_padding + _get_font_ascent(p_theme.font, p_theme.normal_font_size)), item.cell_texts[i], HORIZONTAL_ALIGNMENT_LEFT, cell_rect.size.x - p_theme.table_cell_padding * 2.0, p_theme.normal_font_size, p_theme.font_color);
					}
				}
			} break;
			case MarkdownViewerBlock::TYPE_IMAGE: {
				p_viewer.draw_rect(rect, p_theme.image_background_color, true);
				if (item.texture.is_valid()) {
					p_viewer.draw_texture_rect(item.texture, rect, false);
				} else if (p_theme.font.is_valid()) {
					String label = item.image_status == MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED ? "Remote image disabled" : (item.image_status == MarkdownViewerImageStatus::STATUS_FAILED ? "Image failed" : "Loading image");
					Color color = item.image_status == MarkdownViewerImageStatus::STATUS_FAILED ? p_theme.image_error_color : p_theme.muted_font_color;
					p_viewer.draw_string(p_theme.font, rect.position + Point2(10.0, 24.0), label, HORIZONTAL_ALIGNMENT_LEFT, rect.size.x - 20.0, p_theme.normal_font_size, color);
					if (!item.secondary_text.is_empty()) {
						p_viewer.draw_string(p_theme.font, rect.position + Point2(10.0, 48.0), item.secondary_text, HORIZONTAL_ALIGNMENT_LEFT, rect.size.x - 20.0, p_theme.code_font_size, p_theme.muted_font_color);
					}
				}
			} break;
			case MarkdownViewerBlock::TYPE_BLOCK_QUOTE: {
				p_viewer.draw_line(Point2(rect.position.x + 3.0, rect.position.y), Point2(rect.position.x + 3.0, rect.position.y + rect.size.y), p_theme.blockquote_color, 3.0);
				_draw_text_lines(p_viewer, item, p_theme, p_scroll_offset);
			} break;
			case MarkdownViewerBlock::TYPE_THEMATIC_BREAK: {
				const real_t y = rect.position.y + rect.size.y * 0.5;
				p_viewer.draw_line(Point2(rect.position.x, y), Point2(rect.position.x + rect.size.x, y), p_theme.table_border_color, 1.0);
			} break;
			default: {
				_draw_text_lines(p_viewer, item, p_theme, p_scroll_offset);
			} break;
		}
	}

	for (const MarkdownViewerHitTest &hit : p_layout.hit_tests) {
		if (hit.type != MarkdownViewerHitTest::TYPE_LINK) {
			continue;
		}
		Rect2 rect = hit.rect;
		rect.position.y -= p_scroll_offset;
		if (rect.intersects(viewport_rect)) {
			p_viewer.draw_line(rect.position + Point2(0.0, rect.size.y - 2.0), rect.position + Point2(rect.size.x, rect.size.y - 2.0), p_theme.link_color, 1.0);
		}
	}

	if (p_layout.content_height > p_viewer.get_size().y && p_viewer.is_scroll_enabled()) {
		const real_t ratio = p_viewer.get_size().y / p_layout.content_height;
		const real_t thumb_height = MAX(real_t(24.0), p_viewer.get_size().y * ratio);
		const real_t max_scroll = MAX(real_t(1.0), p_layout.content_height - p_viewer.get_size().y);
		const real_t thumb_y = (p_viewer.get_size().y - thumb_height) * CLAMP(p_scroll_offset / max_scroll, real_t(0.0), real_t(1.0));
		p_viewer.draw_rect(Rect2(p_viewer.get_size().x - 5.0, thumb_y, 4.0, thumb_height), p_theme.muted_font_color, true);
	}
}
