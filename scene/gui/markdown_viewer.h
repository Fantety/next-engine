/**************************************************************************/
/*  markdown_viewer.h                                                     */
/**************************************************************************/

#pragma once

#include "scene/gui/control.h"
#include "scene/gui/markdown_viewer_document.h"
#include "scene/gui/markdown_viewer_image_loader.h"
#include "scene/gui/markdown_viewer_layout.h"

class MarkdownViewer : public Control {
	GDCLASS(MarkdownViewer, Control);

	struct SelectableRun {
		int start = 0;
		int end = 0;
		Rect2 rect;
		String text;
		Ref<Font> font;
		int font_size = 16;
		bool table_cell = false;
		real_t table_max_scroll_offset = 0.0;
	};

	String markdown;
	MarkdownViewerDocument document;
	MarkdownViewerLayout layout;
	MarkdownViewerLayout measured_layout_cache;
	MarkdownViewerImageLoader *image_loader = nullptr;

	bool remote_images_enabled = false;
	bool open_links_enabled = false;
	bool scroll_enabled = true;
	bool syntax_highlighting_enabled = true;
	bool code_copy_enabled = true;
	bool async_parsing_enabled = true;
	bool selection_enabled = true;

	real_t image_max_width = 800.0;
	real_t image_max_height = 420.0;
	real_t content_height = 0.0;
	real_t scroll_offset = 0.0;
	real_t table_horizontal_scroll_offset = 0.0;
	Size2 last_layout_size = Size2(-1.0, -1.0);
	Size2 measured_layout_size = Size2(-1.0, -1.0);

	bool parse_dirty = true;
	bool layout_dirty = true;
	bool measured_layout_valid = false;
	bool async_parse_pending = false;
	bool selectable_runs_dirty = true;
	bool selection_dragging = false;
	bool selection_dragged = false;
	bool pending_click_hit_valid = false;
	int async_parse_minimum_length = 1024;
	int64_t document_generation = 0;
	int64_t async_parse_generation = 0;
	int layout_build_count_for_test = 0;
	int document_build_count_for_test = 0;
	int selection_anchor = 0;
	int selection_caret = 0;
	Point2 selection_drag_start;
	MarkdownViewerHitTest pending_click_hit;
	Vector<SelectableRun> selectable_runs;
	String selectable_text;

	void _mark_parse_dirty();
	void _mark_layout_dirty();
	void _clear_measured_layout_cache();
	void _ensure_document();
	void _ensure_layout();
	void _ensure_selectable_runs();
	void _build_layout(const Size2 &p_layout_size);
	bool _should_parse_async() const;
	bool _start_async_parse();
	real_t _get_max_table_horizontal_scroll_offset() const;
	void _clamp_scroll_offset();
	void _clamp_table_horizontal_scroll_offset();
	bool _scroll_table_horizontally(real_t p_delta);
	void _clear_selection();
	void _clamp_selection();
	void _append_selectable_run(const String &p_text, const Rect2 &p_rect, const Ref<Font> &p_font, int p_font_size, bool p_table_cell = false, real_t p_table_max_scroll_offset = 0.0);
	void _append_selectable_separator(const String &p_separator);
	int _get_selection_start() const;
	int _get_selection_end() const;
	bool _copy_selection_to_clipboard() const;
	bool _is_copy_shortcut(const Ref<InputEventKey> &p_key) const;
	bool _is_select_all_shortcut(const Ref<InputEventKey> &p_key) const;
	bool _get_selection_offset_at_position(const Point2 &p_position, int &r_offset, bool p_allow_outside = false);
	real_t _measure_run_text_width(const SelectableRun &p_run, int p_chars) const;
	Vector<Rect2> _get_selection_rects() const;
	void _activate_hit(const MarkdownViewerHitTest &p_hit);
	MarkdownViewerLayoutTheme _make_layout_theme() const;
	bool _resolve_hit_test(const Point2 &p_position, MarkdownViewerHitTest &r_hit);
	void _image_state_changed(const String &p_source);
	static void _run_async_document_parse(void *p_userdata);
	static void _finish_async_document_parse(uint64_t p_request_ptr);

protected:
	void _notification(int p_what);
	void gui_input(const Ref<InputEvent> &p_event) override;
	static void _bind_methods();

public:
	virtual Size2 get_minimum_size() const override;

	void set_markdown(const String &p_markdown);
	void set_markdown_document(const String &p_markdown, const MarkdownViewerDocument &p_document);
	String get_markdown() const;

	void set_remote_images_enabled(bool p_enabled);
	bool is_remote_images_enabled() const;

	void set_open_links_enabled(bool p_enabled);
	bool is_open_links_enabled() const;

	void set_scroll_enabled(bool p_enabled);
	bool is_scroll_enabled() const;

	void set_syntax_highlighting_enabled(bool p_enabled);
	bool is_syntax_highlighting_enabled() const;

	void set_code_copy_enabled(bool p_enabled);
	bool is_code_copy_enabled() const;

	void set_async_parsing_enabled(bool p_enabled);
	bool is_async_parsing_enabled() const;

	void set_selection_enabled(bool p_enabled);
	bool is_selection_enabled() const;
	bool has_selection() const;
	String get_selected_text() const;
	void select_all();
	void deselect();

	void set_image_max_width(real_t p_width);
	real_t get_image_max_width() const;

	void set_image_max_height(real_t p_height);
	real_t get_image_max_height() const;

	real_t get_content_height() const;
	real_t get_content_height_for_width(real_t p_width) const;

	void force_layout_for_test();
	bool get_hit_test_at_for_test(const Point2 &p_position, MarkdownViewerHitTest &r_hit);
	void set_scroll_offset_for_test(real_t p_offset);
	real_t get_scroll_offset_for_test() const;
	void set_table_horizontal_scroll_offset_for_test(real_t p_offset);
	real_t get_table_horizontal_scroll_offset_for_test() const;
	real_t get_max_table_horizontal_scroll_offset_for_test() const;
	Point2 get_text_caret_position_for_test(int p_offset);
	int get_layout_build_count_for_test() const;
	int get_document_build_count_for_test() const;
	bool is_async_parse_pending_for_test() const;
	void set_async_parse_minimum_length_for_test(int p_minimum_length);

	MarkdownViewer();
};
