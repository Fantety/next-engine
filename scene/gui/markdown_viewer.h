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

	String markdown;
	MarkdownViewerDocument document;
	MarkdownViewerLayout layout;
	MarkdownViewerImageLoader *image_loader = nullptr;

	bool remote_images_enabled = false;
	bool open_links_enabled = false;
	bool scroll_enabled = true;
	bool syntax_highlighting_enabled = true;
	bool code_copy_enabled = true;

	real_t image_max_width = 800.0;
	real_t image_max_height = 420.0;
	real_t content_height = 0.0;
	real_t scroll_offset = 0.0;

	bool parse_dirty = true;
	bool layout_dirty = true;

	void _mark_parse_dirty();
	void _mark_layout_dirty();
	void _ensure_document();
	void _ensure_layout();
	void _clamp_scroll_offset();
	MarkdownViewerLayoutTheme _make_layout_theme() const;
	bool _resolve_hit_test(const Point2 &p_position, MarkdownViewerHitTest &r_hit);
	void _image_state_changed(const String &p_source);

protected:
	void _notification(int p_what);
	void gui_input(const Ref<InputEvent> &p_event) override;
	static void _bind_methods();

public:
	virtual Size2 get_minimum_size() const override;

	void set_markdown(const String &p_markdown);
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

	void set_image_max_width(real_t p_width);
	real_t get_image_max_width() const;

	void set_image_max_height(real_t p_height);
	real_t get_image_max_height() const;

	real_t get_content_height() const;

	void force_layout_for_test();
	bool get_hit_test_at_for_test(const Point2 &p_position, MarkdownViewerHitTest &r_hit);
	void set_scroll_offset_for_test(real_t p_offset);
	real_t get_scroll_offset_for_test() const;

	MarkdownViewer();
};
