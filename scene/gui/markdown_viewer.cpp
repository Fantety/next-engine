/**************************************************************************/
/*  markdown_viewer.cpp                                                   */
/**************************************************************************/

#include "markdown_viewer.h"

#include "core/input/input_event.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/message_queue.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "scene/gui/markdown_viewer_draw.h"
#include "servers/display/display_server.h"

namespace {

struct MarkdownViewerAsyncParseRequest {
	ObjectID owner_id;
	int64_t generation = 0;
	String markdown;
	MarkdownViewerDocument document;
};

} // namespace

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
	ClassDB::bind_method(D_METHOD("set_async_parsing_enabled", "enabled"), &MarkdownViewer::set_async_parsing_enabled);
	ClassDB::bind_method(D_METHOD("is_async_parsing_enabled"), &MarkdownViewer::is_async_parsing_enabled);
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
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "async_parsing_enabled"), "set_async_parsing_enabled", "is_async_parsing_enabled");
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
		case NOTIFICATION_RESIZED: {
			const Size2 current_size = get_size();
			if (layout_dirty || !Math::is_equal_approx(current_size.x, last_layout_size.x)) {
				_mark_layout_dirty();
			} else {
				last_layout_size = current_size;
				_clamp_scroll_offset();
				queue_redraw();
			}
		} break;
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
	_clear_measured_layout_cache();
	queue_redraw();
	update_minimum_size();
	if (_should_parse_async()) {
		_start_async_parse();
	}
}

void MarkdownViewer::_mark_layout_dirty() {
	layout_dirty = true;
	_clear_measured_layout_cache();
	queue_redraw();
	update_minimum_size();
}

void MarkdownViewer::_clear_measured_layout_cache() {
	measured_layout_cache.clear();
	measured_layout_size = Size2(-1.0, -1.0);
	measured_layout_valid = false;
}

void MarkdownViewer::_ensure_document() {
	if (!parse_dirty) {
		return;
	}
	if (async_parse_pending) {
		return;
	}
	if (_should_parse_async() && _start_async_parse()) {
		return;
	}
	MarkdownViewerDocumentBuilder builder;
	document = builder.build(markdown);
	document_build_count_for_test++;
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
	const Size2 current_size = get_size();
	if (parse_dirty && async_parse_pending) {
		if (layout_dirty) {
			const real_t previous_content_height = content_height;
			layout.clear();
			content_height = 0.0;
			last_layout_size = current_size;
			layout_dirty = false;
			_clamp_scroll_offset();
			if (!scroll_enabled && !Math::is_equal_approx(previous_content_height, content_height)) {
				update_minimum_size();
			}
		}
		return;
	}
	if (!layout_dirty && Math::is_equal_approx(current_size.x, last_layout_size.x)) {
		if (!Math::is_equal_approx(current_size.y, last_layout_size.y)) {
			last_layout_size = current_size;
			_clamp_scroll_offset();
		}
		return;
	}

	if (measured_layout_valid && Math::is_equal_approx(current_size.x, measured_layout_size.x)) {
		const real_t previous_content_height = content_height;
		layout = measured_layout_cache;
		content_height = layout.content_height;
		last_layout_size = current_size;
		layout_dirty = false;
		_clamp_scroll_offset();
		if (!scroll_enabled && !Math::is_equal_approx(previous_content_height, content_height)) {
			update_minimum_size();
		}
		return;
	}

	_build_layout(current_size);
}

void MarkdownViewer::_build_layout(const Size2 &p_layout_size) {
	MarkdownViewerLayoutBuilder builder;
	builder.set_image_loader(image_loader);
	const real_t previous_content_height = content_height;
	layout = builder.build(document, p_layout_size, _make_layout_theme());
	layout_build_count_for_test++;
	content_height = layout.content_height;
	last_layout_size = p_layout_size;
	layout_dirty = false;
	_clamp_scroll_offset();
	if (!scroll_enabled && !Math::is_equal_approx(previous_content_height, content_height)) {
		update_minimum_size();
	}
}

bool MarkdownViewer::_should_parse_async() const {
	return async_parsing_enabled && !markdown.is_empty() && markdown.length() >= async_parse_minimum_length;
}

bool MarkdownViewer::_start_async_parse() {
	if (async_parse_pending && async_parse_generation == document_generation) {
		return true;
	}

	WorkerThreadPool *worker_pool = WorkerThreadPool::get_singleton();
	CallQueue *main_queue = MessageQueue::get_main_singleton();
	if (!worker_pool || !main_queue) {
		return false;
	}

	MarkdownViewerAsyncParseRequest *request = memnew(MarkdownViewerAsyncParseRequest);
	request->owner_id = get_instance_id();
	request->generation = document_generation;
	request->markdown = markdown;

	async_parse_generation = document_generation;
	async_parse_pending = true;

	const WorkerThreadPool::TaskID task_id = worker_pool->add_native_task(&MarkdownViewer::_run_async_document_parse, request, false, "MarkdownViewer document parse");
	if (task_id == WorkerThreadPool::INVALID_TASK_ID) {
		async_parse_pending = false;
		memdelete(request);
		return false;
	}

	return true;
}

void MarkdownViewer::_run_async_document_parse(void *p_userdata) {
	MarkdownViewerAsyncParseRequest *request = static_cast<MarkdownViewerAsyncParseRequest *>(p_userdata);
	ERR_FAIL_NULL(request);

	MarkdownViewerDocumentBuilder builder;
	request->document = builder.build(request->markdown);

	CallQueue *main_queue = MessageQueue::get_main_singleton();
	if (!main_queue) {
		memdelete(request);
		return;
	}

	const Error err = main_queue->push_callable(callable_mp_static(&MarkdownViewer::_finish_async_document_parse), reinterpret_cast<uint64_t>(request));
	if (err != OK) {
		memdelete(request);
	}
}

void MarkdownViewer::_finish_async_document_parse(uint64_t p_request_ptr) {
	MarkdownViewerAsyncParseRequest *request = reinterpret_cast<MarkdownViewerAsyncParseRequest *>(p_request_ptr);
	if (!request) {
		return;
	}

	MarkdownViewer *viewer = ObjectDB::get_instance<MarkdownViewer>(request->owner_id);
	if (viewer && request->generation == viewer->document_generation && request->markdown == viewer->markdown) {
		viewer->document = request->document;
		viewer->parse_dirty = false;
		viewer->async_parse_pending = false;
		viewer->document_build_count_for_test++;
		viewer->_mark_layout_dirty();
	} else if (viewer && request->generation == viewer->async_parse_generation) {
		viewer->async_parse_pending = false;
	}

	memdelete(request);
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
	document_generation++;
	async_parse_pending = false;
	document.clear();
	layout.clear();
	content_height = 0.0;
	last_layout_size = Size2(-1.0, -1.0);
	_mark_parse_dirty();
}

void MarkdownViewer::set_markdown_document(const String &p_markdown, const MarkdownViewerDocument &p_document) {
	if (markdown == p_markdown && !parse_dirty) {
		return;
	}

	markdown = p_markdown;
	document_generation++;
	async_parse_pending = false;
	document = p_document;
	parse_dirty = false;
	_mark_layout_dirty();
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

void MarkdownViewer::set_async_parsing_enabled(bool p_enabled) {
	if (async_parsing_enabled == p_enabled) {
		return;
	}

	async_parsing_enabled = p_enabled;
	if (!async_parsing_enabled) {
		async_parse_pending = false;
		document_generation++;
	}
	if (parse_dirty) {
		_mark_parse_dirty();
	}
}

bool MarkdownViewer::is_async_parsing_enabled() const {
	return async_parsing_enabled;
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

real_t MarkdownViewer::get_content_height_for_width(real_t p_width) const {
	MarkdownViewer *self = const_cast<MarkdownViewer *>(this);
	const real_t layout_width = MAX(real_t(1.0), p_width);
	self->_ensure_document();
	if (self->parse_dirty && self->async_parse_pending) {
		return self->content_height;
	}

	if (!self->layout_dirty && Math::is_equal_approx(self->last_layout_size.x, layout_width)) {
		return self->content_height;
	}
	if (self->measured_layout_valid && Math::is_equal_approx(self->measured_layout_size.x, layout_width)) {
		return self->measured_layout_cache.content_height;
	}

	if (Math::is_equal_approx(get_size().x, layout_width)) {
		self->_ensure_layout();
		return self->content_height;
	}

	MarkdownViewerLayoutBuilder builder;
	builder.set_image_loader(image_loader);
	const Size2 layout_size(layout_width, get_size().y);
	MarkdownViewerLayout measured_layout = builder.build(document, layout_size, _make_layout_theme());
	self->layout_build_count_for_test++;
	self->measured_layout_cache = measured_layout;
	self->measured_layout_size = layout_size;
	self->measured_layout_valid = true;
	return measured_layout.content_height;
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

int MarkdownViewer::get_layout_build_count_for_test() const {
	return layout_build_count_for_test;
}

int MarkdownViewer::get_document_build_count_for_test() const {
	return document_build_count_for_test;
}

bool MarkdownViewer::is_async_parse_pending_for_test() const {
	return async_parse_pending;
}

void MarkdownViewer::set_async_parse_minimum_length_for_test(int p_minimum_length) {
	async_parse_minimum_length = MAX(0, p_minimum_length);
	if (parse_dirty) {
		_mark_parse_dirty();
	}
}

MarkdownViewer::MarkdownViewer() {
	set_mouse_filter(MOUSE_FILTER_STOP);
	set_clip_contents(true);
	image_loader = memnew(MarkdownViewerImageLoader);
	image_loader->set_remote_images_enabled(remote_images_enabled);
	image_loader->connect("image_state_changed", callable_mp(this, &MarkdownViewer::_image_state_changed));
	add_child(image_loader, false, INTERNAL_MODE_BACK);
}
