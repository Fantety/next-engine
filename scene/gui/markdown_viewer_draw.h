/**************************************************************************/
/*  markdown_viewer_draw.h                                                */
/**************************************************************************/

#pragma once

#include "scene/gui/markdown_viewer_layout.h"

class MarkdownViewer;

class MarkdownViewerDrawHelper {
	static void _draw_text_lines(MarkdownViewer &p_viewer, const MarkdownViewerLayoutItem &p_item, const MarkdownViewerLayoutTheme &p_theme, real_t p_scroll_offset);

public:
	static Vector<int> collect_visible_item_indices_for_test(const MarkdownViewerLayout &p_layout, const Rect2 &p_viewport_rect, real_t p_scroll_offset);
	static void draw(MarkdownViewer &p_viewer, const MarkdownViewerLayout &p_layout, const MarkdownViewerLayoutTheme &p_theme, real_t p_scroll_offset, real_t p_table_horizontal_scroll_offset);
};
