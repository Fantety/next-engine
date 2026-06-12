/**************************************************************************/
/*  ai_markdown_label.cpp                                                 */
/**************************************************************************/

#include "ai_markdown_label.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/gui/markdown_viewer.h"

namespace {

String _escape_plain_markdown(const String &p_text) {
	String escaped;
	bool line_start = true;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if (line_start && (c == '#' || c == '-' || c == '+' || c == '*' || c == '>' || c == '|')) {
			escaped += '\\';
		}
		if (c == '\\' || c == '`' || c == '*' || c == '_' || c == '[' || c == ']' || c == '(' || c == ')' || c == '~' || c == '|') {
			escaped += '\\';
		}
		escaped += c;
		line_start = c == '\n' || c == '\r';
	}
	return escaped;
}

String _flatten_inlines(const Vector<MarkdownViewerInline> &p_inlines) {
	String text;
	for (const MarkdownViewerInline &run : p_inlines) {
		if (run.children.is_empty()) {
			text += run.text;
		} else {
			text += _flatten_inlines(run.children);
		}
	}
	return text;
}

void _append_joined(String &r_text, const String &p_text) {
	if (p_text.is_empty()) {
		return;
	}
	if (!r_text.is_empty()) {
		r_text += "\n";
	}
	r_text += p_text;
}

String _flatten_block(const MarkdownViewerBlock &p_block) {
	switch (p_block.type) {
		case MarkdownViewerBlock::TYPE_PARAGRAPH:
		case MarkdownViewerBlock::TYPE_HEADING:
		case MarkdownViewerBlock::TYPE_IMAGE: {
			return !p_block.plain_text.is_empty() ? p_block.plain_text : _flatten_inlines(p_block.inlines);
		}
		case MarkdownViewerBlock::TYPE_CODE_BLOCK: {
			return p_block.plain_text;
		}
		case MarkdownViewerBlock::TYPE_LIST:
		case MarkdownViewerBlock::TYPE_BLOCK_QUOTE: {
			String text;
			for (const MarkdownViewerBlock &child : p_block.children) {
				_append_joined(text, _flatten_block(child));
			}
			return text;
		}
		case MarkdownViewerBlock::TYPE_LIST_ITEM: {
			String text = !p_block.plain_text.is_empty() ? p_block.plain_text : _flatten_inlines(p_block.inlines);
			for (const MarkdownViewerBlock &child : p_block.children) {
				_append_joined(text, _flatten_block(child));
			}
			return text;
		}
		case MarkdownViewerBlock::TYPE_TABLE: {
			String text;
			for (const MarkdownViewerTableRow &row : p_block.table_rows) {
				String row_text;
				for (const MarkdownViewerTableCell &cell : row.cells) {
					if (!row_text.is_empty()) {
						row_text += "\t";
					}
					row_text += cell.plain_text;
				}
				_append_joined(text, row_text);
			}
			return text;
		}
		case MarkdownViewerBlock::TYPE_THEMATIC_BREAK: {
			return String();
		}
	}
	return String();
}

String _flatten_document(const MarkdownViewerDocument &p_document) {
	String text;
	for (const MarkdownViewerBlock &block : p_document.blocks) {
		_append_joined(text, _flatten_block(block));
	}
	return text;
}

} // namespace

void AIMarkdownLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_markdown", "markdown"), &AIMarkdownLabel::set_markdown);
	ClassDB::bind_method(D_METHOD("get_markdown"), &AIMarkdownLabel::get_markdown);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "markdown"), "set_markdown", "get_markdown");
}

AIMarkdownLabel::AIMarkdownLabel() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);

	markdown_viewer = memnew(MarkdownViewer);
	markdown_viewer->set_name(SNAME("AIMessageMarkdownViewer"));
	markdown_viewer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	markdown_viewer->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	markdown_viewer->set_remote_images_enabled(false);
	markdown_viewer->set_open_links_enabled(false);
	markdown_viewer->set_scroll_enabled(false);
	markdown_viewer->connect(SceneStringName(minimum_size_changed), callable_mp(this, &AIMarkdownLabel::_markdown_viewer_minimum_size_changed), CONNECT_DEFERRED);
	add_child(markdown_viewer);
}

void AIMarkdownLabel::_markdown_viewer_minimum_size_changed() {
	_invalidate_cached_layout();
	_queue_markdown_viewer_minimum_size_sync();
}

void AIMarkdownLabel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_THEME_CHANGED: {
			_invalidate_cached_layout();
			_queue_markdown_viewer_minimum_size_sync();
		} break;
	}
}

real_t AIMarkdownLabel::_get_layout_width() const {
	if (!markdown_viewer) {
		return 0.0;
	}

	real_t layout_width = get_size().x;
	if (layout_width <= 1.0) {
		layout_width = markdown_viewer->get_size().x;
	}
	return layout_width;
}

void AIMarkdownLabel::_invalidate_cached_layout() {
	cached_layout_width = -1.0;
}

void AIMarkdownLabel::_queue_markdown_viewer_minimum_size_sync() {
	if (layout_sync_queued) {
		return;
	}

	layout_sync_queued = true;
	callable_mp(this, &AIMarkdownLabel::_flush_markdown_viewer_minimum_size_sync).call_deferred();
}

void AIMarkdownLabel::_flush_markdown_viewer_minimum_size_sync() {
	layout_sync_queued = false;
	_sync_markdown_viewer_minimum_size();
	update_minimum_size();
	if (Control *parent_control = get_parent_control()) {
		parent_control->update_minimum_size();
	}
}

void AIMarkdownLabel::_sync_markdown_viewer_minimum_size() const {
	if (!markdown_viewer) {
		return;
	}

	const real_t layout_width = _get_layout_width();
	if (layout_width <= 1.0) {
		return;
	}

	if (Math::is_equal_approx(cached_layout_width, layout_width)) {
		return;
	}

	cached_layout_width = layout_width;
	cached_content_height = markdown_viewer->get_content_height_for_width(layout_width);
	const Size2 viewer_minimum_size(0, cached_content_height);
	if (markdown_viewer->get_custom_minimum_size() != viewer_minimum_size) {
		markdown_viewer->set_custom_minimum_size(viewer_minimum_size);
	}
}

Size2 AIMarkdownLabel::get_minimum_size() const {
	if (!markdown_viewer) {
		return VBoxContainer::get_minimum_size();
	}

	const real_t layout_width = _get_layout_width();
	if (layout_width <= 1.0) {
		return VBoxContainer::get_minimum_size();
	}

	if (!layout_sync_queued && !Math::is_equal_approx(cached_layout_width, layout_width)) {
		_sync_markdown_viewer_minimum_size();
	}

	return Size2(0, cached_content_height);
}

void AIMarkdownLabel::set_markdown(const String &p_markdown) {
	if (markdown_text == p_markdown) {
		return;
	}

	markdown_text = p_markdown;
	MarkdownViewerDocumentBuilder builder;
	const MarkdownViewerDocument document = builder.build(markdown_text);
	parsed_text = _flatten_document(document);
	if (markdown_viewer) {
		markdown_viewer->set_markdown_document(markdown_text, document);
	}
	_invalidate_cached_layout();
	_queue_markdown_viewer_minimum_size_sync();
}

String AIMarkdownLabel::get_markdown() const {
	return markdown_text;
}

void AIMarkdownLabel::clear() {
	if (markdown_text.is_empty() && parsed_text.is_empty()) {
		return;
	}

	markdown_text.clear();
	parsed_text.clear();
	if (markdown_viewer) {
		markdown_viewer->set_markdown(String());
	}
	_invalidate_cached_layout();
	_queue_markdown_viewer_minimum_size_sync();
}

void AIMarkdownLabel::add_text(const String &p_text) {
	const String escaped_text = _escape_plain_markdown(p_text);
	if (markdown_text == escaped_text && parsed_text == p_text) {
		return;
	}

	markdown_text = escaped_text;
	parsed_text = p_text;
	if (markdown_viewer) {
		markdown_viewer->set_markdown(markdown_text);
	}
	_invalidate_cached_layout();
	_queue_markdown_viewer_minimum_size_sync();
}

String AIMarkdownLabel::get_parsed_text() const {
	return parsed_text;
}

MarkdownViewer *AIMarkdownLabel::get_markdown_viewer() const {
	return markdown_viewer;
}

void AIMarkdownLabel::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	(void)p_mode;
}

void AIMarkdownLabel::add_theme_font_size_override(const StringName &p_name, int p_font_size) {
	if (markdown_viewer) {
		const StringName mapped_name = (p_name == SNAME("normal_font_size") || p_name == SNAME("bold_font_size")) ? SNAME("font_size") : p_name;
		markdown_viewer->add_theme_font_size_override(mapped_name, p_font_size);
	}
	_invalidate_cached_layout();
	_queue_markdown_viewer_minimum_size_sync();
}

void AIMarkdownLabel::remove_theme_font_size_override(const StringName &p_name) {
	if (markdown_viewer) {
		const StringName mapped_name = (p_name == SNAME("normal_font_size") || p_name == SNAME("bold_font_size")) ? SNAME("font_size") : p_name;
		markdown_viewer->remove_theme_font_size_override(mapped_name);
	}
	_invalidate_cached_layout();
	_queue_markdown_viewer_minimum_size_sync();
}
