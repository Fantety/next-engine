/**************************************************************************/
/*  markdown_viewer.cpp                                                   */
/**************************************************************************/

#include "markdown_viewer.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/gui/markdown_viewer_draw.h"
#include "servers/display/display_server.h"

void MarkdownViewer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_markdown", "markdown"), &MarkdownViewer::set_markdown);
	ClassDB::bind_method(D_METHOD("get_markdown"), &MarkdownViewer::get_markdown);
	ClassDB::bind_method(D_METHOD("set_remote_images_enabled", "enabled"), &MarkdownViewer::set_remote_images_enabled);
	ClassDB::bind_method(D_METHOD("is_remote_images_enabled"), &MarkdownViewer::is_remote_images_enabled);
	ClassDB::bind_method(D_METHOD("set_open_links_enabled", "enabled"), &MarkdownViewer::set_open_links_enabled);
	ClassDB::bind_method(D_METHOD("is_open_links_enabled"), &MarkdownViewer::is_open_links_enabled);
	ClassDB::bind_method(D_METHOD("set_scroll_enabled", "enabled"), &MarkdownViewer::set_scroll_enabled);
	ClassDB::bind_method(D_METHOD("is_scroll_enabled"), &MarkdownViewer::is_scroll_enabled);
	ClassDB::bind_method(D_METHOD("set_syntax_highlighting_enabled", "enabled"), &MarkdownViewer::set_syntax_highlighting_enabled);
	ClassDB::bind_method(D_METHOD("is_syntax_highlighting_enabled"), &MarkdownViewer::is_syntax_highlighting_enabled);
	ClassDB::bind_method(D_METHOD("set_code_copy_enabled", "enabled"), &MarkdownViewer::set_code_copy_enabled);
	ClassDB::bind_method(D_METHOD("is_code_copy_enabled"), &MarkdownViewer::is_code_copy_enabled);
	ClassDB::bind_method(D_METHOD("set_image_max_width", "width"), &MarkdownViewer::set_image_max_width);
	ClassDB::bind_method(D_METHOD("get_image_max_width"), &MarkdownViewer::get_image_max_width);
	ClassDB::bind_method(D_METHOD("set_image_max_height", "height"), &MarkdownViewer::set_image_max_height);
	ClassDB::bind_method(D_METHOD("get_image_max_height"), &MarkdownViewer::get_image_max_height);
	ClassDB::bind_method(D_METHOD("get_content_height"), &MarkdownViewer::get_content_height);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "markdown", PROPERTY_HINT_MULTILINE_TEXT), "set_markdown", "get_markdown");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "remote_images_enabled"), "set_remote_images_enabled", "is_remote_images_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "open_links_enabled"), "set_open_links_enabled", "is_open_links_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_enabled"), "set_scroll_enabled", "is_scroll_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "syntax_highlighting_enabled"), "set_syntax_highlighting_enabled", "is_syntax_highlighting_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "code_copy_enabled"), "set_code_copy_enabled", "is_code_copy_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "image_max_width", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), "set_image_max_width", "get_image_max_width");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "image_max_height", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), "set_image_max_height", "get_image_max_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "content_height", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_content_height");

	ADD_SIGNAL(MethodInfo("link_clicked", PropertyInfo(Variant::STRING, "url")));
	ADD_SIGNAL(MethodInfo("image_loaded", PropertyInfo(Variant::STRING, "source")));
	ADD_SIGNAL(MethodInfo("image_failed", PropertyInfo(Variant::STRING, "source"), PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("code_block_copied", PropertyInfo(Variant::STRING, "language"), PropertyInfo(Variant::STRING, "code")));
}

void MarkdownViewer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			_ensure_layout();
			MarkdownViewerDrawHelper::draw(*this, layout, _make_layout_theme(), scroll_offset);
		} break;
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_THEME_CHANGED: {
			_mark_layout_dirty();
		} break;
	}
}

void MarkdownViewer::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mouse_button = p_event;
	if (mouse_button.is_valid() && mouse_button->is_pressed()) {
		if (scroll_enabled && mouse_button->get_button_index() == MouseButton::WHEEL_DOWN) {
			scroll_offset += 48.0;
			_clamp_scroll_offset();
			queue_redraw();
			accept_event();
			return;
		}
		if (scroll_enabled && mouse_button->get_button_index() == MouseButton::WHEEL_UP) {
			scroll_offset -= 48.0;
			_clamp_scroll_offset();
			queue_redraw();
			accept_event();
			return;
		}
		if (mouse_button->get_button_index() == MouseButton::LEFT) {
			MarkdownViewerHitTest hit;
			if (_resolve_hit_test(mouse_button->get_position(), hit)) {
				if (hit.type == MarkdownViewerHitTest::TYPE_LINK) {
					emit_signal(SNAME("link_clicked"), hit.payload);
					if (open_links_enabled) {
						OS::get_singleton()->shell_open(hit.payload);
					}
					accept_event();
				} else if (hit.type == MarkdownViewerHitTest::TYPE_CODE_COPY && code_copy_enabled) {
					if (DisplayServer::get_singleton()) {
						DisplayServer::get_singleton()->clipboard_set(hit.payload);
					}
					emit_signal(SNAME("code_block_copied"), hit.secondary_payload, hit.payload);
					accept_event();
				}
			}
		}
	}
}

void MarkdownViewer::_mark_parse_dirty() {
	parse_dirty = true;
	layout_dirty = true;
	queue_redraw();
	update_minimum_size();
}

void MarkdownViewer::_mark_layout_dirty() {
	layout_dirty = true;
	queue_redraw();
	update_minimum_size();
}

void MarkdownViewer::_ensure_document() {
	if (!parse_dirty) {
		return;
	}
	MarkdownViewerDocumentBuilder builder;
	document = builder.build(markdown);
	parse_dirty = false;
	layout_dirty = true;
}

MarkdownViewerLayoutTheme MarkdownViewer::_make_layout_theme() const {
	MarkdownViewerLayoutTheme theme;
	theme.font = get_theme_font(SNAME("font"), SNAME("Label"));
	theme.bold_font = get_theme_font(SNAME("bold_font"), SNAME("RichTextLabel"));
	theme.italic_font = get_theme_font(SNAME("italics_font"), SNAME("RichTextLabel"));
	theme.bold_italic_font = get_theme_font(SNAME("bold_italics_font"), SNAME("RichTextLabel"));
	theme.mono_font = get_theme_font(SNAME("mono_font"), SNAME("RichTextLabel"));
	if (theme.bold_font.is_null()) {
		theme.bold_font = theme.font;
	}
	if (theme.italic_font.is_null()) {
		theme.italic_font = theme.font;
	}
	if (theme.bold_italic_font.is_null()) {
		theme.bold_italic_font = theme.bold_font;
	}
	if (theme.mono_font.is_null()) {
		theme.mono_font = get_theme_font(SNAME("font"), SNAME("CodeEdit"));
	}

	const int default_size = get_theme_font_size(SNAME("font_size"), SNAME("Label"));
	if (default_size > 0) {
		theme.normal_font_size = default_size;
		theme.code_font_size = MAX(10, default_size - 1);
		theme.h1_font_size = default_size + 10;
		theme.h2_font_size = default_size + 6;
		theme.h3_font_size = default_size + 3;
	}

	theme.image_max_width = image_max_width;
	theme.image_max_height = image_max_height;
	theme.code_copy_enabled = code_copy_enabled;
	theme.syntax_highlighting_enabled = syntax_highlighting_enabled;
	return theme;
}

void MarkdownViewer::_ensure_layout() {
	_ensure_document();
	if (!layout_dirty) {
		return;
	}

	MarkdownViewerLayoutBuilder builder;
	builder.set_image_loader(image_loader);
	layout = builder.build(document, get_size(), _make_layout_theme());
	content_height = layout.content_height;
	layout_dirty = false;
	_clamp_scroll_offset();
}

void MarkdownViewer::_clamp_scroll_offset() {
	const real_t max_scroll = MAX(real_t(0.0), content_height - get_size().y);
	scroll_offset = CLAMP(scroll_offset, real_t(0.0), max_scroll);
}

bool MarkdownViewer::_resolve_hit_test(const Point2 &p_position, MarkdownViewerHitTest &r_hit) {
	_ensure_layout();
	const Point2 document_position = p_position + Point2(0.0, scroll_offset);
	for (const MarkdownViewerHitTest &hit : layout.hit_tests) {
		if (hit.rect.has_point(document_position)) {
			r_hit = hit;
			return true;
		}
	}
	return false;
}

void MarkdownViewer::_image_state_changed(const String &p_source) {
	_mark_layout_dirty();
	if (!image_loader) {
		return;
	}
	MarkdownViewerImageRequestResult result = image_loader->ensure_image(p_source);
	if (result.status == MarkdownViewerImageStatus::STATUS_LOADED) {
		emit_signal(SNAME("image_loaded"), p_source);
	} else if (result.status == MarkdownViewerImageStatus::STATUS_FAILED) {
		emit_signal(SNAME("image_failed"), p_source, result.error);
	}
}

Size2 MarkdownViewer::get_minimum_size() const {
	if (!scroll_enabled) {
		const_cast<MarkdownViewer *>(this)->_ensure_layout();
		return Size2(0, content_height);
	}
	return Size2();
}

void MarkdownViewer::set_markdown(const String &p_markdown) {
	if (markdown == p_markdown) {
		return;
	}
	markdown = p_markdown;
	_mark_parse_dirty();
}

String MarkdownViewer::get_markdown() const {
	return markdown;
}

void MarkdownViewer::set_remote_images_enabled(bool p_enabled) {
	if (remote_images_enabled == p_enabled) {
		return;
	}
	remote_images_enabled = p_enabled;
	if (image_loader) {
		image_loader->set_remote_images_enabled(remote_images_enabled);
	}
	_mark_layout_dirty();
}

bool MarkdownViewer::is_remote_images_enabled() const {
	return remote_images_enabled;
}

void MarkdownViewer::set_open_links_enabled(bool p_enabled) {
	open_links_enabled = p_enabled;
}

bool MarkdownViewer::is_open_links_enabled() const {
	return open_links_enabled;
}

void MarkdownViewer::set_scroll_enabled(bool p_enabled) {
	if (scroll_enabled == p_enabled) {
		return;
	}
	scroll_enabled = p_enabled;
	_clamp_scroll_offset();
	_mark_layout_dirty();
}

bool MarkdownViewer::is_scroll_enabled() const {
	return scroll_enabled;
}

void MarkdownViewer::set_syntax_highlighting_enabled(bool p_enabled) {
	syntax_highlighting_enabled = p_enabled;
	_mark_layout_dirty();
}

bool MarkdownViewer::is_syntax_highlighting_enabled() const {
	return syntax_highlighting_enabled;
}

void MarkdownViewer::set_code_copy_enabled(bool p_enabled) {
	if (code_copy_enabled == p_enabled) {
		return;
	}
	code_copy_enabled = p_enabled;
	_mark_layout_dirty();
}

bool MarkdownViewer::is_code_copy_enabled() const {
	return code_copy_enabled;
}

void MarkdownViewer::set_image_max_width(real_t p_width) {
	image_max_width = MAX(real_t(1.0), p_width);
	_mark_layout_dirty();
}

real_t MarkdownViewer::get_image_max_width() const {
	return image_max_width;
}

void MarkdownViewer::set_image_max_height(real_t p_height) {
	image_max_height = MAX(real_t(1.0), p_height);
	_mark_layout_dirty();
}

real_t MarkdownViewer::get_image_max_height() const {
	return image_max_height;
}

real_t MarkdownViewer::get_content_height() const {
	return content_height;
}

void MarkdownViewer::force_layout_for_test() {
	_ensure_layout();
}

bool MarkdownViewer::get_hit_test_at_for_test(const Point2 &p_position, MarkdownViewerHitTest &r_hit) {
	return _resolve_hit_test(p_position, r_hit);
}

void MarkdownViewer::set_scroll_offset_for_test(real_t p_offset) {
	scroll_offset = p_offset;
	_ensure_layout();
	_clamp_scroll_offset();
}

real_t MarkdownViewer::get_scroll_offset_for_test() const {
	return scroll_offset;
}

MarkdownViewer::MarkdownViewer() {
	set_mouse_filter(MOUSE_FILTER_STOP);
	set_clip_contents(true);
	image_loader = memnew(MarkdownViewerImageLoader);
	image_loader->set_remote_images_enabled(remote_images_enabled);
	image_loader->connect("image_state_changed", callable_mp(this, &MarkdownViewer::_image_state_changed));
	add_child(image_loader, false, INTERNAL_MODE_BACK);
}
