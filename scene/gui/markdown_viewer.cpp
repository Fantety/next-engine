/**************************************************************************/
/*  markdown_viewer.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
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

Ref<Font> _get_selectable_span_font(const MarkdownViewerLayoutSpan &p_span, const MarkdownViewerLayoutTheme &p_theme) {
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

real_t _measure_selectable_text_width(const Ref<Font> &p_font, const String &p_text, int p_font_size) {
	if (p_text.is_empty()) {
		return 0.0;
	}
	if (p_font.is_valid()) {
		const real_t width = p_font->get_string_size(p_text, HORIZONTAL_ALIGNMENT_LEFT, -1, p_font_size).x;
		if (width > 0.0) {
			return width;
		}
	}
	return p_text.length() * p_font_size * 0.55;
}

real_t _get_selectable_font_height(const Ref<Font> &p_font, int p_font_size) {
	if (p_font.is_valid()) {
		const real_t height = p_font->get_height(p_font_size);
		if (height > 0.0) {
			return height;
		}
	}
	return p_font_size;
}

real_t _get_selectable_line_advance(const Ref<Font> &p_font, int p_font_size, const MarkdownViewerLayoutTheme &p_theme) {
	return MAX(real_t(p_font_size), _get_selectable_font_height(p_font, p_font_size)) + p_theme.line_spacing;
}

bool _is_command_or_control_event(const Ref<InputEventKey> &p_key) {
	return p_key->is_command_or_control_pressed() || p_key->is_command_or_control_autoremap();
}

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
	ClassDB::bind_method(D_METHOD("set_selection_enabled", "enabled"), &MarkdownViewer::set_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_selection_enabled"), &MarkdownViewer::is_selection_enabled);
	ClassDB::bind_method(D_METHOD("has_selection"), &MarkdownViewer::has_selection);
	ClassDB::bind_method(D_METHOD("get_selected_text"), &MarkdownViewer::get_selected_text);
	ClassDB::bind_method(D_METHOD("select_all"), &MarkdownViewer::select_all);
	ClassDB::bind_method(D_METHOD("deselect"), &MarkdownViewer::deselect);
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
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selection_enabled"), "set_selection_enabled", "is_selection_enabled");
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
			MarkdownViewerDrawHelper::draw(*this, layout, _make_layout_theme(), scroll_offset, table_horizontal_scroll_offset, _get_selection_rects());
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
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo() && selection_enabled) {
		if (_is_select_all_shortcut(key)) {
			select_all();
			accept_event();
			return;
		}
		if (_is_copy_shortcut(key)) {
			if (_copy_selection_to_clipboard()) {
				accept_event();
			}
			return;
		}
	}

	Ref<InputEventMouseButton> mouse_button = p_event;
	if (mouse_button.is_valid()) {
		if (mouse_button->is_pressed()) {
			if (mouse_button->get_button_index() == MouseButton::WHEEL_RIGHT || (mouse_button->is_shift_pressed() && scroll_enabled && mouse_button->get_button_index() == MouseButton::WHEEL_DOWN)) {
				if (_scroll_table_horizontally(48.0 * mouse_button->get_factor())) {
					accept_event();
					return;
				}
			}
			if (mouse_button->get_button_index() == MouseButton::WHEEL_LEFT || (mouse_button->is_shift_pressed() && scroll_enabled && mouse_button->get_button_index() == MouseButton::WHEEL_UP)) {
				if (_scroll_table_horizontally(-48.0 * mouse_button->get_factor())) {
					accept_event();
					return;
				}
			}
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
				const bool has_hit = _resolve_hit_test(mouse_button->get_position(), hit);
				if (has_hit && hit.type == MarkdownViewerHitTest::TYPE_CODE_COPY && code_copy_enabled) {
					_activate_hit(hit);
					accept_event();
					return;
				}

				if (selection_enabled) {
					int offset = 0;
					if (_get_selection_offset_at_position(mouse_button->get_position(), offset)) {
						grab_focus();
						selection_anchor = offset;
						selection_caret = offset;
						selection_drag_start = mouse_button->get_position();
						selection_dragging = true;
						selection_dragged = false;
						pending_click_hit_valid = has_hit && hit.type == MarkdownViewerHitTest::TYPE_LINK;
						if (pending_click_hit_valid) {
							pending_click_hit = hit;
						}
						queue_redraw();
						accept_event();
						return;
					}

					if (has_selection()) {
						deselect();
						accept_event();
						return;
					}
				}

				if (has_hit && hit.type == MarkdownViewerHitTest::TYPE_LINK) {
					_activate_hit(hit);
					accept_event();
					return;
				}
			}
		} else if (mouse_button->get_button_index() == MouseButton::LEFT && selection_dragging) {
			int offset = 0;
			if (_get_selection_offset_at_position(mouse_button->get_position(), offset, true)) {
				selection_caret = offset;
			}
			const bool has_text_selection = has_selection();
			if (!has_text_selection && pending_click_hit_valid && !selection_dragged) {
				_activate_hit(pending_click_hit);
			}
			selection_dragging = false;
			selection_dragged = false;
			pending_click_hit_valid = false;
			queue_redraw();
			accept_event();
			return;
		}
	}

	Ref<InputEventMouseMotion> mouse_motion = p_event;
	if (mouse_motion.is_valid() && selection_dragging) {
		if (mouse_motion->get_button_mask().has_flag(MouseButtonMask::LEFT)) {
			int offset = 0;
			if (_get_selection_offset_at_position(mouse_motion->get_position(), offset, true)) {
				if (offset != selection_caret) {
					selection_caret = offset;
					selection_dragged = selection_dragged || selection_caret != selection_anchor;
				}
			}
			if (mouse_motion->get_position().distance_squared_to(selection_drag_start) > 4.0) {
				selection_dragged = true;
			}
			queue_redraw();
			accept_event();
			return;
		}
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		const Vector2 delta = pan_gesture->get_delta();
		const real_t abs_delta_x = delta.x < 0.0 ? -delta.x : delta.x;
		const real_t abs_delta_y = delta.y < 0.0 ? -delta.y : delta.y;
		if (abs_delta_x >= abs_delta_y && _scroll_table_horizontally(delta.x * 48.0)) {
			accept_event();
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
	selectable_runs_dirty = true;
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
			selectable_runs_dirty = true;
			_clamp_scroll_offset();
			_clamp_table_horizontal_scroll_offset();
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
			_clamp_table_horizontal_scroll_offset();
		}
		return;
	}

	if (measured_layout_valid && Math::is_equal_approx(current_size.x, measured_layout_size.x)) {
		const real_t previous_content_height = content_height;
		layout = measured_layout_cache;
		content_height = layout.content_height;
		last_layout_size = current_size;
		layout_dirty = false;
		selectable_runs_dirty = true;
		_clamp_scroll_offset();
		_clamp_table_horizontal_scroll_offset();
		if (!scroll_enabled && !Math::is_equal_approx(previous_content_height, content_height)) {
			update_minimum_size();
		}
		return;
	}

	_build_layout(current_size);
}

void MarkdownViewer::_append_selectable_run(const String &p_text, const Rect2 &p_rect, const Ref<Font> &p_font, int p_font_size, bool p_table_cell, real_t p_table_max_scroll_offset) {
	if (p_text.is_empty()) {
		return;
	}

	SelectableRun run;
	run.start = selectable_text.length();
	run.end = run.start + p_text.length();
	run.rect = p_rect;
	run.text = p_text;
	run.font = p_font;
	run.font_size = p_font_size;
	run.table_cell = p_table_cell;
	run.table_max_scroll_offset = p_table_max_scroll_offset;
	selectable_runs.push_back(run);
	selectable_text += p_text;
}

void MarkdownViewer::_append_selectable_separator(const String &p_separator) {
	if (!p_separator.is_empty()) {
		selectable_text += p_separator;
	}
}

void MarkdownViewer::_ensure_selectable_runs() {
	if (!selectable_runs_dirty) {
		return;
	}

	_ensure_layout();
	selectable_runs_dirty = false;
	selectable_runs.clear();
	selectable_text = String();

	const MarkdownViewerLayoutTheme theme = _make_layout_theme();
	for (const MarkdownViewerLayoutItem &item : layout.items) {
		switch (item.type) {
			case MarkdownViewerBlock::TYPE_CODE_BLOCK: {
				if (item.code_lines.is_empty()) {
					break;
				}
				if (!selectable_text.is_empty()) {
					_append_selectable_separator("\n");
				}

				const Ref<Font> code_font = theme.mono_font.is_valid() ? theme.mono_font : theme.font;
				const real_t line_height = _get_selectable_line_advance(code_font, theme.code_font_size, theme);
				real_t y = item.rect.position.y + 28.0;
				const real_t x = item.rect.position.x + theme.code_padding;
				for (int i = 0; i < item.code_lines.size(); i++) {
					if (i > 0) {
						_append_selectable_separator("\n");
					}
					const String &line_text = item.code_lines[i].text;
					const real_t width = MAX(real_t(1.0), _measure_selectable_text_width(code_font, line_text, theme.code_font_size));
					_append_selectable_run(line_text, Rect2(x, y, width, line_height), code_font, theme.code_font_size);
					y += line_height;
				}
			} break;
			case MarkdownViewerBlock::TYPE_TABLE: {
				if (item.cell_rects.is_empty() || item.cell_texts.is_empty()) {
					break;
				}
				if (!selectable_text.is_empty()) {
					_append_selectable_separator("\n");
				}

				int column_count = 1;
				const real_t first_y = item.cell_rects[0].position.y;
				for (int i = 1; i < item.cell_rects.size(); i++) {
					if (!Math::is_equal_approx(item.cell_rects[i].position.y, first_y)) {
						break;
					}
					column_count++;
				}
				column_count = MAX(1, column_count);

				const real_t table_viewport_width = item.scroll_viewport_width > 0.0 ? item.scroll_viewport_width : item.rect.size.x;
				const real_t table_max_scroll_offset = MAX(real_t(0.0), item.rect.size.x - table_viewport_width);
				for (int i = 0; i < item.cell_texts.size(); i++) {
					const int column = i % column_count;
					if (i > 0) {
						_append_selectable_separator(column == 0 ? "\n" : "\t");
					}

					Rect2 text_rect = item.cell_rects[i].grow_individual(-theme.table_cell_padding, -theme.table_cell_padding, -theme.table_cell_padding, -theme.table_cell_padding);
					text_rect.size.x = MAX(real_t(1.0), MIN(text_rect.size.x, _measure_selectable_text_width(theme.font, item.cell_texts[i], theme.normal_font_size)));
					_append_selectable_run(item.cell_texts[i], text_rect, theme.font, theme.normal_font_size, true, table_max_scroll_offset);
				}
			} break;
			default: {
				bool has_text = false;
				for (const MarkdownViewerLayoutLine &line : item.inline_lines) {
					for (const MarkdownViewerLayoutSpan &span : line.spans) {
						if (!span.text.is_empty()) {
							has_text = true;
							break;
						}
					}
					if (has_text) {
						break;
					}
				}
				if (!has_text) {
					break;
				}
				if (!selectable_text.is_empty()) {
					_append_selectable_separator("\n");
				}

				for (const MarkdownViewerLayoutLine &line : item.inline_lines) {
					for (const MarkdownViewerLayoutSpan &span : line.spans) {
						const Ref<Font> span_font = _get_selectable_span_font(span, theme);
						_append_selectable_run(span.text, span.rect, span_font, item.font_size);
					}
				}
			} break;
		}
	}

	_clamp_selection();
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
	selectable_runs_dirty = true;
	_clamp_scroll_offset();
	_clamp_table_horizontal_scroll_offset();
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

real_t MarkdownViewer::_get_max_table_horizontal_scroll_offset() const {
	real_t max_offset = 0.0;
	for (const MarkdownViewerLayoutItem &item : layout.items) {
		if (item.type != MarkdownViewerBlock::TYPE_TABLE) {
			continue;
		}
		const real_t table_viewport_width = item.scroll_viewport_width > 0.0 ? item.scroll_viewport_width : item.rect.size.x;
		max_offset = MAX(max_offset, item.rect.size.x - table_viewport_width);
	}
	return MAX(real_t(0.0), max_offset);
}

void MarkdownViewer::_clamp_table_horizontal_scroll_offset() {
	table_horizontal_scroll_offset = CLAMP(table_horizontal_scroll_offset, real_t(0.0), _get_max_table_horizontal_scroll_offset());
}

bool MarkdownViewer::_scroll_table_horizontally(real_t p_delta) {
	_ensure_layout();
	const real_t previous_offset = table_horizontal_scroll_offset;
	table_horizontal_scroll_offset += p_delta;
	_clamp_table_horizontal_scroll_offset();
	if (Math::is_equal_approx(previous_offset, table_horizontal_scroll_offset)) {
		return false;
	}
	queue_redraw();
	return true;
}

void MarkdownViewer::_clear_selection() {
	selection_anchor = 0;
	selection_caret = 0;
	selection_dragging = false;
	selection_dragged = false;
	pending_click_hit_valid = false;
}

void MarkdownViewer::_clamp_selection() {
	const int text_length = selectable_text.length();
	selection_anchor = CLAMP(selection_anchor, 0, text_length);
	selection_caret = CLAMP(selection_caret, 0, text_length);
}

int MarkdownViewer::_get_selection_start() const {
	return MIN(selection_anchor, selection_caret);
}

int MarkdownViewer::_get_selection_end() const {
	return MAX(selection_anchor, selection_caret);
}

bool MarkdownViewer::_copy_selection_to_clipboard() const {
	const String selected_text = get_selected_text();
	if (selected_text.is_empty() || !DisplayServer::get_singleton()) {
		return false;
	}

	DisplayServer::get_singleton()->clipboard_set(selected_text);
	return true;
}

bool MarkdownViewer::_is_copy_shortcut(const Ref<InputEventKey> &p_key) const {
	if (p_key->is_action("ui_copy", true)) {
		return true;
	}
	return _is_command_or_control_event(p_key) && !p_key->is_alt_pressed() && !p_key->is_shift_pressed() && p_key->get_keycode() == Key::C;
}

bool MarkdownViewer::_is_select_all_shortcut(const Ref<InputEventKey> &p_key) const {
	if (p_key->is_action("ui_text_select_all", true)) {
		return true;
	}
	return _is_command_or_control_event(p_key) && !p_key->is_alt_pressed() && !p_key->is_shift_pressed() && p_key->get_keycode() == Key::A;
}

real_t MarkdownViewer::_measure_run_text_width(const SelectableRun &p_run, int p_chars) const {
	const int clamped_chars = CLAMP(p_chars, 0, p_run.text.length());
	if (clamped_chars == 0) {
		return 0.0;
	}
	return _measure_selectable_text_width(p_run.font, p_run.text.substr(0, clamped_chars), p_run.font_size);
}

bool MarkdownViewer::_get_selection_offset_at_position(const Point2 &p_position, int &r_offset, bool p_allow_outside) {
	_ensure_selectable_runs();
	if (selectable_runs.is_empty()) {
		r_offset = 0;
		return false;
	}

	const Point2 document_position = p_position + Point2(0.0, scroll_offset);
	for (const SelectableRun &run : selectable_runs) {
		Rect2 run_rect = run.rect;
		if (run.table_cell) {
			run_rect.position.x -= CLAMP(table_horizontal_scroll_offset, real_t(0.0), run.table_max_scroll_offset);
		}

		if (document_position.y < run_rect.position.y) {
			if (p_allow_outside) {
				r_offset = run.start;
				return true;
			}
			return false;
		}
		if (document_position.y > run_rect.position.y + run_rect.size.y) {
			continue;
		}

		if (document_position.x <= run_rect.position.x) {
			r_offset = run.start;
			return true;
		}
		if (document_position.x >= run_rect.position.x + run_rect.size.x) {
			r_offset = run.end;
			return true;
		}

		const real_t local_x = document_position.x - run_rect.position.x;
		real_t x = 0.0;
		for (int i = 0; i < run.text.length(); i++) {
			const String character = String::chr(run.text[i]);
			const real_t character_width = _measure_selectable_text_width(run.font, character, run.font_size);
			if (local_x <= x + character_width * 0.5) {
				r_offset = run.start + i;
				return true;
			}
			x += character_width;
		}

		r_offset = run.end;
		return true;
	}

	if (p_allow_outside) {
		r_offset = selectable_text.length();
		return true;
	}

	return false;
}

Vector<Rect2> MarkdownViewer::_get_selection_rects() const {
	const_cast<MarkdownViewer *>(this)->_ensure_selectable_runs();

	Vector<Rect2> rects;
	if (!selection_enabled) {
		return rects;
	}

	const int selection_start = _get_selection_start();
	const int selection_end = _get_selection_end();
	if (selection_start == selection_end) {
		return rects;
	}

	for (const SelectableRun &run : selectable_runs) {
		const int range_start = MAX(selection_start, run.start);
		const int range_end = MIN(selection_end, run.end);
		if (range_start >= range_end) {
			continue;
		}

		const real_t start_x = _measure_run_text_width(run, range_start - run.start);
		const real_t end_x = _measure_run_text_width(run, range_end - run.start);
		Rect2 rect(run.rect.position + Point2(start_x, -scroll_offset), Size2(MAX(real_t(1.0), end_x - start_x), run.rect.size.y));
		if (run.table_cell) {
			rect.position.x -= CLAMP(table_horizontal_scroll_offset, real_t(0.0), run.table_max_scroll_offset);
		}
		rects.push_back(rect);
	}

	return rects;
}

void MarkdownViewer::_activate_hit(const MarkdownViewerHitTest &p_hit) {
	if (p_hit.type == MarkdownViewerHitTest::TYPE_LINK) {
		emit_signal(SNAME("link_clicked"), p_hit.payload);
		if (open_links_enabled) {
			OS::get_singleton()->shell_open(p_hit.payload);
		}
	} else if (p_hit.type == MarkdownViewerHitTest::TYPE_CODE_COPY && code_copy_enabled) {
		if (DisplayServer::get_singleton()) {
			DisplayServer::get_singleton()->clipboard_set(p_hit.payload);
		}
		emit_signal(SNAME("code_block_copied"), p_hit.secondary_payload, p_hit.payload);
	}
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
	table_horizontal_scroll_offset = 0.0;
	document.clear();
	layout.clear();
	content_height = 0.0;
	last_layout_size = Size2(-1.0, -1.0);
	_clear_selection();
	selectable_runs.clear();
	selectable_text = String();
	selectable_runs_dirty = true;
	_mark_parse_dirty();
}

void MarkdownViewer::set_markdown_document(const String &p_markdown, const MarkdownViewerDocument &p_document) {
	if (markdown == p_markdown && !parse_dirty) {
		return;
	}

	markdown = p_markdown;
	document_generation++;
	async_parse_pending = false;
	table_horizontal_scroll_offset = 0.0;
	document = p_document;
	parse_dirty = false;
	_clear_selection();
	selectable_runs.clear();
	selectable_text = String();
	selectable_runs_dirty = true;
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

void MarkdownViewer::set_selection_enabled(bool p_enabled) {
	if (selection_enabled == p_enabled) {
		return;
	}

	selection_enabled = p_enabled;
	set_focus_mode(selection_enabled ? FOCUS_ALL : FOCUS_NONE);
	if (!selection_enabled) {
		_clear_selection();
	}
	queue_redraw();
}

bool MarkdownViewer::is_selection_enabled() const {
	return selection_enabled;
}

bool MarkdownViewer::has_selection() const {
	const_cast<MarkdownViewer *>(this)->_ensure_selectable_runs();
	return selection_enabled && _get_selection_start() != _get_selection_end();
}

String MarkdownViewer::get_selected_text() const {
	const_cast<MarkdownViewer *>(this)->_ensure_selectable_runs();
	if (!selection_enabled) {
		return String();
	}

	const int selection_start = _get_selection_start();
	const int selection_end = _get_selection_end();
	if (selection_start == selection_end) {
		return String();
	}

	return selectable_text.substr(selection_start, selection_end - selection_start);
}

void MarkdownViewer::select_all() {
	if (!selection_enabled) {
		return;
	}

	_ensure_selectable_runs();
	selection_anchor = 0;
	selection_caret = selectable_text.length();
	queue_redraw();
}

void MarkdownViewer::deselect() {
	if (!has_selection() && !selection_dragging) {
		return;
	}

	_clear_selection();
	queue_redraw();
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

void MarkdownViewer::set_table_horizontal_scroll_offset_for_test(real_t p_offset) {
	table_horizontal_scroll_offset = p_offset;
	_ensure_layout();
	_clamp_table_horizontal_scroll_offset();
}

real_t MarkdownViewer::get_table_horizontal_scroll_offset_for_test() const {
	return table_horizontal_scroll_offset;
}

real_t MarkdownViewer::get_max_table_horizontal_scroll_offset_for_test() const {
	const_cast<MarkdownViewer *>(this)->_ensure_layout();
	return _get_max_table_horizontal_scroll_offset();
}

Point2 MarkdownViewer::get_text_caret_position_for_test(int p_offset) {
	_ensure_selectable_runs();
	const int clamped_offset = CLAMP(p_offset, 0, selectable_text.length());
	for (const SelectableRun &run : selectable_runs) {
		if (clamped_offset < run.start || clamped_offset > run.end) {
			continue;
		}

		Rect2 run_rect = run.rect;
		if (run.table_cell) {
			run_rect.position.x -= CLAMP(table_horizontal_scroll_offset, real_t(0.0), run.table_max_scroll_offset);
		}
		const real_t x = run_rect.position.x + _measure_run_text_width(run, clamped_offset - run.start);
		return Point2(x, run_rect.position.y - scroll_offset + run_rect.size.y * 0.5);
	}

	return Point2();
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
	set_focus_mode(FOCUS_ALL);
	image_loader = memnew(MarkdownViewerImageLoader);
	image_loader->set_remote_images_enabled(remote_images_enabled);
	image_loader->connect("image_state_changed", callable_mp(this, &MarkdownViewer::_image_state_changed));
	add_child(image_loader, false, INTERNAL_MODE_BACK);
}
