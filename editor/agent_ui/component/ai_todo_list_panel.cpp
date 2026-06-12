/**************************************************************************/
/*  ai_todo_list_panel.cpp                                                */
/**************************************************************************/

#include "ai_todo_list_panel.h"

#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/resources/style_box_flat.h"
#include "servers/text/text_server.h"

namespace {

constexpr int TODO_PANEL_RADIUS = 8;
constexpr int TODO_PANEL_MARGIN_X = 12;
constexpr int TODO_PANEL_MARGIN_TOP = 10;
constexpr int TODO_PANEL_MARGIN_BOTTOM = 9;
constexpr int TODO_TITLE_FONT_SIZE = 12;
constexpr int TODO_ROW_FONT_SIZE = 12;
constexpr int TODO_ROW_HEIGHT = 22;
constexpr int TODO_STATUS_WIDTH = 16;
constexpr int TODO_STATUS_TEXT_GAP = 7;

String _todo_status(const Dictionary &p_todo) {
	return String(p_todo.get("status", String())).strip_edges().to_lower();
}

bool _todo_is_done(const Dictionary &p_todo) {
	const String status = _todo_status(p_todo);
	return status == "completed" || status == "cancelled";
}

String _todo_summary_text(int p_completed, int p_total) {
	return vformat(String::utf8(u8"已完成 %d 个任务（共 %d 个）"), p_completed, p_total);
}

Color _with_alpha(Color p_color, float p_alpha) {
	p_color.a = p_alpha;
	return p_color;
}

Color _font_color(const Control *p_control) {
	return p_control->get_theme_color(SceneStringName(font_color), EditorStringName(Editor));
}

Color _panel_bg_color(const Control *p_control) {
	Color color = p_control->get_theme_color(SNAME("dark_color_1"), EditorStringName(Editor));
	color.a = 0.94;
	return color;
}

Color _panel_border_color(const Control *p_control) {
	return _with_alpha(_font_color(p_control), 0.14);
}

Ref<StyleBoxFlat> _make_panel_style(const Control *p_control) {
	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(_panel_bg_color(p_control));
	style->set_border_color(_panel_border_color(p_control));
	style->set_border_width_all(MAX(1, int(EDSCALE)));
	style->set_corner_radius_all(TODO_PANEL_RADIUS * EDSCALE);
	style->set_content_margin_all(0);
	return style;
}

class AITodoListItemRow : public HBoxContainer {
	String content;
	String status;
	Label *status_label = nullptr;
	Label *content_label = nullptr;

	String _status_text() const {
		if (status == "completed") {
			return String::utf8(u8"✓");
		}
		if (status == "in_progress") {
			return String::utf8(u8"•");
		}
		if (status == "cancelled") {
			return "-";
		}
		return String::utf8(u8"○");
	}

public:
	AITodoListItemRow() {
		set_h_size_flags(Control::SIZE_EXPAND_FILL);
		set_custom_minimum_size(Size2(0, TODO_ROW_HEIGHT * EDSCALE));
		add_theme_constant_override(SNAME("separation"), TODO_STATUS_TEXT_GAP * EDSCALE);

		status_label = memnew(Label);
		status_label->set_custom_minimum_size(Size2(TODO_STATUS_WIDTH * EDSCALE, 0));
		status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		status_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		status_label->add_theme_font_size_override(SceneStringName(font_size), TODO_ROW_FONT_SIZE * EDSCALE);
		add_child(status_label);

		content_label = memnew(Label);
		content_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		content_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		content_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		content_label->add_theme_font_size_override(SceneStringName(font_size), TODO_ROW_FONT_SIZE * EDSCALE);
		add_child(content_label);
	}

	void set_todo(const Dictionary &p_todo) {
		content = String(p_todo.get("content", String())).strip_edges();
		status = _todo_status(p_todo);
		if (status_label) {
			status_label->set_text(_status_text());
		}
		if (content_label) {
			content_label->set_text(content);
			content_label->set_tooltip_text(content);
		}
		update_minimum_size();
	}
};

} // namespace

void AITodoListPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_todos", "todos"), &AITodoListPanel::set_todos);
	ClassDB::bind_method(D_METHOD("get_todos"), &AITodoListPanel::get_todos);
}

void AITodoListPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
		_apply_theme();
	}
}

AITodoListPanel::AITodoListPanel() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hide();

	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override(SNAME("margin_left"), TODO_PANEL_MARGIN_X * EDSCALE);
	margin->add_theme_constant_override(SNAME("margin_top"), TODO_PANEL_MARGIN_TOP * EDSCALE);
	margin->add_theme_constant_override(SNAME("margin_right"), TODO_PANEL_MARGIN_X * EDSCALE);
	margin->add_theme_constant_override(SNAME("margin_bottom"), TODO_PANEL_MARGIN_BOTTOM * EDSCALE);
	add_child(margin);

	VBoxContainer *content_box = memnew(VBoxContainer);
	content_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content_box->add_theme_constant_override(SNAME("separation"), 4 * EDSCALE);
	margin->add_child(content_box);

	summary_label = memnew(Label);
	summary_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	summary_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	summary_label->add_theme_font_size_override(SceneStringName(font_size), TODO_TITLE_FONT_SIZE * EDSCALE);
	content_box->add_child(summary_label);

	items_box = memnew(VBoxContainer);
	items_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	items_box->add_theme_constant_override(SNAME("separation"), 0);
	content_box->add_child(items_box);
}

void AITodoListPanel::_apply_theme() {
	if (applying_theme || !is_inside_tree()) {
		return;
	}

	applying_theme = true;
	add_theme_style_override(SceneStringName(panel), _make_panel_style(this));
	if (summary_label) {
		summary_label->add_theme_color_override(SceneStringName(font_color), _font_color(this));
	}
	applying_theme = false;
}

void AITodoListPanel::_refresh() {
	if (!summary_label || !items_box) {
		return;
	}

	while (items_box->get_child_count() > 0) {
		Node *child = items_box->get_child(0);
		items_box->remove_child(child);
		memdelete(child);
	}

	int total = 0;
	int completed = 0;
	Array visible_todos;
	for (int i = 0; i < todos.size(); i++) {
		if (todos[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary todo = todos[i];
		const String content = String(todo.get("content", String())).strip_edges();
		if (content.is_empty()) {
			continue;
		}
		visible_todos.push_back(todo.duplicate(true));
		total++;
		if (_todo_is_done(todo)) {
			completed++;
		}
	}

	if (total <= 0) {
		summary_label->set_text(String());
		hide();
		return;
	}

	const String summary = _todo_summary_text(completed, total);
	summary_label->set_text(summary);
	summary_label->set_tooltip_text(summary);

	for (int i = 0; i < visible_todos.size(); i++) {
		AITodoListItemRow *row = memnew(AITodoListItemRow);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->set_todo(visible_todos[i]);
		items_box->add_child(row);
	}

	show();
}

void AITodoListPanel::set_todos(const Array &p_todos) {
	todos = p_todos.duplicate(true);
	_refresh();
}

Array AITodoListPanel::get_todos() const {
	return todos.duplicate(true);
}
