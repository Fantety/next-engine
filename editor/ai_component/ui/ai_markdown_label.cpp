/**************************************************************************/
/*  ai_markdown_label.cpp                                                 */
/**************************************************************************/

#include "ai_markdown_label.h"

#include "ai_markdown_renderer.h"

#include "core/markdown/markdown_parser.h"
#include "core/object/class_db.h"
#include "scene/gui/rich_text_label.h"
#include "servers/text/text_server.h"

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

	MarkdownParser parser;
	Ref<MarkdownNode> root = parser.parse_markdown(markdown_text);
	if (root.is_null()) {
		add_text(markdown_text);
		return;
	}

	AIMarkdownRenderer renderer;
	renderer.set_heading_base_size(rich_text_label->get_theme_font_size(SNAME("normal_font_size")));
	renderer.render(rich_text_label, root);
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
