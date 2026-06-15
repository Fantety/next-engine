/**************************************************************************/
/*  markdown_viewer_layout.h                                              */
/**************************************************************************/

#pragma once

#include "scene/gui/markdown_viewer_code_highlighter.h"
#include "scene/gui/markdown_viewer_document.h"
#include "scene/gui/markdown_viewer_image_loader.h"
#include "scene/resources/font.h"

struct MarkdownViewerLayoutTheme {
	Ref<Font> font;
	Ref<Font> bold_font;
	Ref<Font> italic_font;
	Ref<Font> bold_italic_font;
	Ref<Font> mono_font;

	int normal_font_size = 16;
	int code_font_size = 14;
	int h1_font_size = 26;
	int h2_font_size = 22;
	int h3_font_size = 19;

	real_t document_margin = 12.0;
	real_t block_spacing = 10.0;
	real_t line_spacing = 4.0;
	real_t table_cell_padding = 8.0;
	real_t code_padding = 10.0;
	real_t image_spacing = 8.0;
	real_t image_max_width = 800.0;
	real_t image_max_height = 420.0;
	bool code_copy_enabled = true;
	bool syntax_highlighting_enabled = true;

	Color font_color = Color(0.88, 0.88, 0.88);
	Color muted_font_color = Color(0.62, 0.62, 0.62);
	Color link_color = Color(0.28, 0.55, 1.0);
	Color code_background_color = Color(0.12, 0.12, 0.13);
	Color code_border_color = Color(0.24, 0.24, 0.26);
	Color table_border_color = Color(0.3, 0.3, 0.32);
	Color table_header_background_color = Color(0.18, 0.18, 0.2);
	Color table_row_background_color = Color(0.14, 0.14, 0.15);
	Color blockquote_color = Color(0.4, 0.55, 0.9);
	Color image_background_color = Color(0.11, 0.11, 0.12);
	Color image_error_color = Color(0.9, 0.35, 0.3);
};

struct MarkdownViewerLayoutSpan {
	enum StyleFlag {
		STYLE_NONE = 0,
		STYLE_STRONG = 1 << 0,
		STYLE_EMPHASIS = 1 << 1,
		STYLE_CODE = 1 << 2,
		STYLE_LINK = 1 << 3,
		STYLE_STRIKETHROUGH = 1 << 4,
	};

	String text;
	String source;
	int style_flags = STYLE_NONE;
	Rect2 rect;
};

struct MarkdownViewerLayoutLine {
	Vector<MarkdownViewerLayoutSpan> spans;
	String text;
	real_t height = 0.0;
	real_t baseline = 0.0;
};

struct MarkdownViewerHitTest {
	enum Type {
		TYPE_LINK,
		TYPE_CODE_COPY,
		TYPE_IMAGE,
	};

	Type type = TYPE_LINK;
	Rect2 rect;
	String payload;
	String secondary_payload;
};

struct MarkdownViewerLayoutItem {
	struct CodeLine {
		String text;
		Vector<MarkdownViewerCodeSpan> spans;
	};

	MarkdownViewerBlock::Type type = MarkdownViewerBlock::TYPE_PARAGRAPH;
	Rect2 rect;
	String text;
	String secondary_text;
	int font_size = 16;
	Vector<String> lines;
	Vector<MarkdownViewerLayoutLine> inline_lines;
	Vector<CodeLine> code_lines;
	Vector<Rect2> cell_rects;
	Vector<String> cell_texts;
	Vector<bool> cell_headers;
	Ref<Texture2D> texture;
	MarkdownViewerImageStatus image_status = MarkdownViewerImageStatus::STATUS_EMPTY;
	Color accent_color;
	String marker_text;
	bool marker_unordered = false;
	Rect2 marker_rect;
	real_t scroll_viewport_width = 0.0;
};

struct MarkdownViewerLayout {
	Vector<MarkdownViewerLayoutItem> items;
	Vector<MarkdownViewerHitTest> hit_tests;
	real_t content_height = 0.0;

	void clear() {
		items.clear();
		hit_tests.clear();
		content_height = 0.0;
	}
};

class MarkdownViewerLayoutBuilder {
	const MarkdownViewerImageLoader *image_loader = nullptr;

	static Vector<String> _wrap_text(const String &p_text, int p_chars_per_line);
	static String _flatten_block_text(const MarkdownViewerBlock &p_block);
	void _append_text_block(const MarkdownViewerBlock &p_block, MarkdownViewerLayout &r_layout, real_t &r_y, real_t p_width, const MarkdownViewerLayoutTheme &p_theme, int p_font_size, real_t p_indent = 0.0, const String &p_marker_text = String(), bool p_marker_unordered = false) const;
	void _append_block(const MarkdownViewerBlock &p_block, MarkdownViewerLayout &r_layout, real_t &r_y, real_t p_width, const MarkdownViewerLayoutTheme &p_theme, real_t p_indent = 0.0) const;

public:
	void set_image_loader(const MarkdownViewerImageLoader *p_loader);
	MarkdownViewerLayout build(const MarkdownViewerDocument &p_document, const Size2 &p_viewport_size, const MarkdownViewerLayoutTheme &p_theme) const;
};
