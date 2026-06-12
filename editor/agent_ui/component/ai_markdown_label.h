/**************************************************************************/
/*  ai_markdown_label.h                                                   */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "servers/text/text_server.h"

class MarkdownViewer;

class AIMarkdownLabel : public VBoxContainer {
	GDCLASS(AIMarkdownLabel, VBoxContainer);

	String markdown_text;
	String parsed_text;
	MarkdownViewer *markdown_viewer = nullptr;
	mutable real_t cached_layout_width = -1.0;
	mutable real_t cached_content_height = 0.0;
	bool layout_sync_queued = false;

	void _markdown_viewer_minimum_size_changed();
	real_t _get_layout_width() const;
	void _invalidate_cached_layout();
	void _queue_markdown_viewer_minimum_size_sync();
	void _flush_markdown_viewer_minimum_size_sync();
	void _sync_markdown_viewer_minimum_size() const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	AIMarkdownLabel();

	virtual Size2 get_minimum_size() const override;

	void set_markdown(const String &p_markdown);
	String get_markdown() const;

	void clear();
	void add_text(const String &p_text);
	String get_parsed_text() const;
	MarkdownViewer *get_markdown_viewer() const;

	void set_autowrap_mode(TextServer::AutowrapMode p_mode);
	void add_theme_font_size_override(const StringName &p_name, int p_font_size);
	void remove_theme_font_size_override(const StringName &p_name);
};
