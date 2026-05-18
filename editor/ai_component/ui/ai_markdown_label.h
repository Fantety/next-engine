/**************************************************************************/
/*  ai_markdown_label.h                                                   */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "servers/text/text_server.h"

class RichTextLabel;

class AIMarkdownLabel : public VBoxContainer {
	GDCLASS(AIMarkdownLabel, VBoxContainer);

	String markdown_text;
	RichTextLabel *rich_text_label = nullptr;

protected:
	static void _bind_methods();

public:
	AIMarkdownLabel();

	void set_markdown(const String &p_markdown);
	String get_markdown() const;

	void clear();
	void add_text(const String &p_text);
	String get_parsed_text() const;
	RichTextLabel *get_rich_text_label() const;

	void set_autowrap_mode(TextServer::AutowrapMode p_mode);
	void add_theme_font_size_override(const StringName &p_name, int p_font_size);
	void remove_theme_font_size_override(const StringName &p_name);
};
