/**************************************************************************/
/*  ai_next_plan_rows.cpp                                                 */
/**************************************************************************/

#include "ai_next_plan_rows.h"

#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/resources/style_box_flat.h"
#include "servers/text/text_server.h"

namespace {

Label *_make_compact_label(const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	return label;
}

void _set_plan_drag_forwarding(Control *p_control, const Callable &p_get_drag_data, const Callable &p_can_drop_data, const Callable &p_drop_data) {
	ERR_FAIL_NULL(p_control);
	p_control->set_drag_forwarding(p_get_drag_data.bind(p_control), p_can_drop_data.bind(p_control), p_drop_data.bind(p_control));
}

void _connect_row_button_pressed(Button *p_button, const Callable &p_callable) {
	ERR_FAIL_NULL(p_button);
	p_button->connect(SceneStringName(pressed), p_callable, Object::CONNECT_DEFERRED);
}

String _get_milestone_visual_state(const String &p_status) {
	if (p_status == "locked" || p_status == "ready_to_lock") {
		return "completed";
	}
	if (p_status == "executing" || p_status == "waiting_playtest") {
		return "running";
	}
	return "pending";
}

String _get_task_visual_state(const String &p_status) {
	if (p_status == "completed" || p_status == "skipped") {
		return "completed";
	}
	if (p_status == "in_progress") {
		return "running";
	}
	return "pending";
}

Color _get_editor_theme_color(const Control *p_control, const StringName &p_name, const Color &p_fallback) {
	if (!p_control) {
		return p_fallback;
	}
	const Color color = p_control->get_theme_color(p_name, SNAME("Editor"));
	return color.a > 0.0f ? color : p_fallback;
}

void _configure_plan_row_visual(Control *p_row, const String &p_visual_state, double &r_pulse_phase) {
	ERR_FAIL_NULL(p_row);
	p_row->set_meta(SNAME("ai_next_visual_state"), p_visual_state);
	if (p_visual_state == "running") {
		p_row->set_meta(SNAME("ai_next_running_visual"), "row_breathe");
	} else {
		p_row->remove_meta(SNAME("ai_next_running_visual"));
	}
	r_pulse_phase = 0.0;
	p_row->set_custom_minimum_size(Size2(0, 30) * EDSCALE);
	p_row->set_process(p_visual_state == "running");
	p_row->queue_redraw();
}

void _draw_plan_row_visual(Control *p_row, double p_pulse_phase) {
	ERR_FAIL_NULL(p_row);
	const String visual_state = String(p_row->get_meta(SNAME("ai_next_visual_state"), "pending"));
	const Color success = _get_editor_theme_color(p_row, SNAME("success_color"), Color(0.34, 0.86, 0.45));
	const Color dark_base = _get_editor_theme_color(p_row, SNAME("dark_color_2"), Color(0.09, 0.095, 0.105));
	const Color font_color = _get_editor_theme_color(p_row, SceneStringName(font_color), Color(0.86, 0.88, 0.9));

	Color bg = dark_base.darkened(0.22);
	bg.a = 0.92;
	Color border = font_color;
	border.a = 0.13;

	if (visual_state == "completed") {
		bg = success.darkened(0.70);
		bg.a = 0.78;
		border = success;
		border.a = 0.40;
	} else if (visual_state == "running") {
		const float pulse = 0.5f + 0.5f * Math::sin(p_pulse_phase);
		bg = success.darkened(Math::lerp(0.72f, 0.45f, pulse));
		bg.a = Math::lerp(0.56f, 0.88f, pulse);
		border = success;
		border.a = Math::lerp(0.26f, 0.68f, pulse);
	}

	Ref<StyleBoxFlat> panel;
	panel.instantiate();
	panel->set_bg_color(bg);
	panel->set_border_color(border);
	panel->set_border_width_all(MAX(1, int(EDSCALE)));
	panel->set_corner_radius_all(6 * EDSCALE);

	const Rect2 panel_rect = Rect2(Point2(0, 1 * EDSCALE), p_row->get_size() - Size2(0, 2 * EDSCALE));
	p_row->draw_style_box(panel, panel_rect);
}

void _process_plan_row_visual(Control *p_row, double &r_pulse_phase) {
	ERR_FAIL_NULL(p_row);
	r_pulse_phase += p_row->get_process_delta_time() * 3.2;
	if (r_pulse_phase > Math::TAU) {
		r_pulse_phase -= Math::TAU;
	}
	p_row->queue_redraw();
}

} // namespace

void setup_ai_next_plan_icon_button(const Control *p_theme_owner, Button *p_button, const StringName &p_icon_name, const String &p_tooltip) {
	ERR_FAIL_NULL(p_theme_owner);
	ERR_FAIL_NULL(p_button);
	p_button->set_text(String());
	p_button->set_flat(true);
	p_button->set_tooltip_text(p_tooltip);
	p_button->set_accessibility_name(p_tooltip.trim_suffix("."));
	p_button->set_custom_minimum_size(Size2(24, 0) * EDSCALE);
	p_button->set_button_icon(p_theme_owner->get_editor_theme_icon(p_icon_name));
}

Control *make_ai_next_plan_drag_preview(const String &p_text) {
	Label *preview = memnew(Label);
	preview->set_text(p_text);
	preview->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	return preview;
}

void AINextMilestoneRow::setup(const Dictionary &p_milestone, int p_index, int p_milestone_count, bool p_active, bool p_can_edit, bool p_workflow_active, const Callbacks &p_callbacks) {
	const String milestone_id = String(p_milestone.get("id", String()));
	const String milestone_status = String(p_milestone.get("status", String()));
	const bool locked = milestone_status == "locked";

	set_name(SNAME("MilestoneRow"));
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);
	set_meta(SNAME("ai_next_milestone_id"), milestone_id);
	set_meta(SNAME("ai_next_index"), p_index);
	_configure_plan_row_visual(this, _get_milestone_visual_state(milestone_status), pulse_phase);
	_set_plan_drag_forwarding(this, p_callbacks.get_drag_data, p_callbacks.can_drop_data, p_callbacks.drop_data);

	Button *title = memnew(Button);
	title->set_name(SNAME("AIPlanMilestoneTitle"));
	title->set_flat(true);
	title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title->set_button_icon(get_editor_theme_icon(SNAME("AIPlanMilestone")));
	title->set_text(vformat("%s%02d %s", p_active ? "> " : "", p_index + 1, String(p_milestone.get("title", TTR("Milestone")))));
	title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	title->set_tooltip_text(String(p_milestone.get("description", String())));
	title->set_disabled(p_workflow_active);
	_connect_row_button_pressed(title, p_callbacks.select);
	add_child(title);

	Button *drag = memnew(Button);
	drag->set_name(SNAME("AIPlanDragMilestoneHandle"));
	setup_ai_next_plan_icon_button(this, drag, SNAME("AIPlanDrag"), TTR("Reorder milestone."));
	drag->set_disabled(!p_can_edit || locked || p_milestone_count < 2);
	drag->set_meta(SNAME("ai_next_milestone_id"), milestone_id);
	drag->set_meta(SNAME("ai_next_index"), p_index);
	_set_plan_drag_forwarding(drag, p_callbacks.get_drag_data, p_callbacks.can_drop_data, p_callbacks.drop_data);
	add_child(drag);

	Label *count = _make_compact_label(itos(Array(p_milestone.get("tasks", Array())).size()));
	count->set_tooltip_text(TTR("Tasks"));
	add_child(count);

	Label *status = _make_compact_label(String(p_milestone.get("status", "draft")).capitalize());
	add_child(status);

	Button *edit = memnew(Button);
	edit->set_name(SNAME("AIPlanEditMilestoneButton"));
	setup_ai_next_plan_icon_button(this, edit, SNAME("AIPlanEdit"), TTR("Edit milestone."));
	edit->set_disabled(!p_can_edit || locked);
	_connect_row_button_pressed(edit, p_callbacks.edit);
	add_child(edit);

	Button *merge = memnew(Button);
	merge->set_name(SNAME("AIPlanMergeMilestoneButton"));
	setup_ai_next_plan_icon_button(this, merge, SNAME("AIPlanMerge"), TTR("Merge milestone."));
	merge->set_disabled(!p_can_edit || locked || p_milestone_count < 2);
	_connect_row_button_pressed(merge, p_callbacks.merge);
	add_child(merge);

	Button *move_up = memnew(Button);
	move_up->set_name(SNAME("AIPlanMoveUpMilestoneButton"));
	setup_ai_next_plan_icon_button(this, move_up, SNAME("AIPlanMoveUp"), TTR("Move milestone up."));
	move_up->set_disabled(!p_can_edit || locked || p_index == 0);
	_connect_row_button_pressed(move_up, p_callbacks.move_up);
	add_child(move_up);

	Button *move_down = memnew(Button);
	move_down->set_name(SNAME("AIPlanMoveDownMilestoneButton"));
	setup_ai_next_plan_icon_button(this, move_down, SNAME("AIPlanMoveDown"), TTR("Move milestone down."));
	move_down->set_disabled(!p_can_edit || locked || p_index >= p_milestone_count - 1);
	_connect_row_button_pressed(move_down, p_callbacks.move_down);
	add_child(move_down);

	Button *remove = memnew(Button);
	remove->set_name(SNAME("AIPlanDeleteMilestoneButton"));
	setup_ai_next_plan_icon_button(this, remove, SNAME("AIPlanDelete"), TTR("Delete milestone."));
	remove->set_disabled(!p_can_edit || locked);
	_connect_row_button_pressed(remove, p_callbacks.remove);
	add_child(remove);
}

void AINextMilestoneRow::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		_draw_plan_row_visual(this, pulse_phase);
	} else if (p_what == NOTIFICATION_PROCESS) {
		_process_plan_row_visual(this, pulse_phase);
	}
}

void AINextTaskRow::setup(const Dictionary &p_task, int p_index, int p_task_count, const String &p_status_text, bool p_selected, bool p_locked, bool p_can_edit, bool p_workflow_active, bool p_can_run, const Callbacks &p_callbacks) {
	const String task_id = String(p_task.get("id", String()));

	set_name(SNAME("TaskRow"));
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);
	set_meta(SNAME("ai_next_task_id"), task_id);
	set_meta(SNAME("ai_next_index"), p_index);
	_configure_plan_row_visual(this, _get_task_visual_state(p_status_text), pulse_phase);
	_set_plan_drag_forwarding(this, p_callbacks.get_drag_data, p_callbacks.can_drop_data, p_callbacks.drop_data);

	Button *title = memnew(Button);
	title->set_name(SNAME("AIPlanTaskTitle"));
	title->set_flat(true);
	title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title->set_button_icon(get_editor_theme_icon(SNAME("AIPlanTask")));
	title->set_text((p_selected ? "> " : "") + String(p_task.get("title", TTR("Task"))));
	title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	title->set_tooltip_text(String(p_task.get("description", String())));
	title->set_disabled(p_workflow_active);
	_connect_row_button_pressed(title, p_callbacks.select);
	add_child(title);

	Button *drag = memnew(Button);
	drag->set_name(SNAME("AIPlanDragTaskHandle"));
	setup_ai_next_plan_icon_button(this, drag, SNAME("AIPlanDrag"), TTR("Reorder task."));
	drag->set_disabled(!p_can_edit || p_locked || p_task_count < 2);
	drag->set_meta(SNAME("ai_next_task_id"), task_id);
	drag->set_meta(SNAME("ai_next_index"), p_index);
	_set_plan_drag_forwarding(drag, p_callbacks.get_drag_data, p_callbacks.can_drop_data, p_callbacks.drop_data);
	add_child(drag);

	Label *agent = _make_compact_label(String(p_task.get("assigned_agent_id", String())).replace("_agent", ""));
	add_child(agent);

	Label *status = _make_compact_label(p_status_text.capitalize());
	add_child(status);

	Button *edit = memnew(Button);
	edit->set_name(SNAME("AIPlanEditTaskButton"));
	setup_ai_next_plan_icon_button(this, edit, SNAME("AIPlanEdit"), TTR("Edit task."));
	edit->set_disabled(!p_can_edit || p_locked);
	_connect_row_button_pressed(edit, p_callbacks.edit);
	add_child(edit);

	Button *dependencies = memnew(Button);
	dependencies->set_name(SNAME("AIPlanDependencyTaskButton"));
	setup_ai_next_plan_icon_button(this, dependencies, SNAME("AIPlanDependency"), TTR("Edit dependencies."));
	dependencies->set_disabled(!p_can_edit || p_locked);
	_connect_row_button_pressed(dependencies, p_callbacks.dependencies);
	add_child(dependencies);

	Button *move_up = memnew(Button);
	move_up->set_name(SNAME("AIPlanMoveUpTaskButton"));
	setup_ai_next_plan_icon_button(this, move_up, SNAME("AIPlanMoveUp"), TTR("Move task up."));
	move_up->set_disabled(!p_can_edit || p_locked || p_index == 0);
	_connect_row_button_pressed(move_up, p_callbacks.move_up);
	add_child(move_up);

	Button *move_down = memnew(Button);
	move_down->set_name(SNAME("AIPlanMoveDownTaskButton"));
	setup_ai_next_plan_icon_button(this, move_down, SNAME("AIPlanMoveDown"), TTR("Move task down."));
	move_down->set_disabled(!p_can_edit || p_locked || p_index >= p_task_count - 1);
	_connect_row_button_pressed(move_down, p_callbacks.move_down);
	add_child(move_down);

	Button *remove = memnew(Button);
	remove->set_name(SNAME("AIPlanDeleteTaskButton"));
	setup_ai_next_plan_icon_button(this, remove, SNAME("AIPlanDelete"), TTR("Delete task."));
	remove->set_disabled(!p_can_edit || p_locked);
	_connect_row_button_pressed(remove, p_callbacks.remove);
	add_child(remove);

	Button *session = memnew(Button);
	session->set_name(SNAME("AIPlanTaskSessionButton"));
	setup_ai_next_plan_icon_button(this, session, SNAME("Session"), TTR("Open task session."));
	session->set_disabled(task_id.is_empty());
	_connect_row_button_pressed(session, p_callbacks.session);
	add_child(session);

	Button *run = memnew(Button);
	run->set_name(SNAME("AIPlanRunTaskButton"));
	setup_ai_next_plan_icon_button(this, run, SNAME("MainPlay"), p_status_text == "in_progress" ? TTR("Running task.") : TTR("Run task."));
	run->set_disabled(!p_can_run);
	_connect_row_button_pressed(run, p_callbacks.run);
	add_child(run);
}

void AINextTaskRow::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		_draw_plan_row_visual(this, pulse_phase);
	} else if (p_what == NOTIFICATION_PROCESS) {
		_process_plan_row_visual(this, pulse_phase);
	}
}
