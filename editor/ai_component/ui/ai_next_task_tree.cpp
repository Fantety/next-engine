/**************************************************************************/
/*  ai_next_task_tree.cpp                                                 */
/**************************************************************************/

#include "ai_next_task_tree.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "servers/text/text_server.h"

namespace {

StringName _get_task_status_icon_name(const String &p_status) {
	if (p_status == "completed" || p_status == "skipped") {
		return SNAME("TaskGreen");
	}
	if (p_status == "failed" || p_status == "blocked") {
		return SNAME("TaskRed");
	}
	return SNAME("TaskYellow");
}

} // namespace

void AINextTaskTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextTaskTree::refresh);
}

AINextTaskTree::AINextTaskTree() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);
}

void AINextTaskTree::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
		refresh();
	}
}

void AINextTaskTree::_clear_rows() {
	while (get_child_count() > 0) {
		Node *child = get_child(0);
		remove_child(child);
		memdelete(child);
	}
}

void AINextTaskTree::set_next_session(AIAgentNextSession *p_session) {
	next_session = p_session;
	refresh();
}

void AINextTaskTree::_task_pressed(const String &p_task_id) {
	if (!next_session) {
		return;
	}
	next_session->select_task(p_task_id);
}

void AINextTaskTree::_run_task_pressed(const String &p_task_id) {
	if (!next_session) {
		return;
	}
	next_session->run_task(p_task_id);
}

void AINextTaskTree::refresh() {
	_clear_rows();
	if (!next_session || next_session->get_project_state().is_null()) {
		return;
	}

	const String milestone_id = next_session->get_project_state()->get_active_milestone_id();
	Dictionary milestone = next_session->get_project_state()->get_milestone(milestone_id);
	Array tasks = milestone.get("tasks", Array());
	if (tasks.is_empty()) {
		Label *empty = memnew(Label);
		empty->set_text(TTR("No tasks"));
		empty->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		add_child(empty);
		return;
	}

	for (int i = 0; i < tasks.size(); i++) {
		if (Variant(tasks[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary task = tasks[i];
		const String task_id = String(task.get("id", String()));
		Dictionary computed_task = next_session->get_project_state()->get_task(task_id);
		const String status_text = String(computed_task.get("status", task.get("status", "pending")));
		const bool selected = task_id == next_session->get_selected_task_id();

		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_theme_constant_override("separation", 6 * EDSCALE);
		add_child(row);

		Button *title = memnew(Button);
		title->set_flat(true);
		title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		title->set_button_icon(get_editor_theme_icon(_get_task_status_icon_name(status_text)));
		title->set_text((selected ? "> " : "") + String(task.get("title", TTR("Task"))));
		title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		title->set_tooltip_text(String(task.get("description", String())));
		title->set_disabled(next_session->is_workflow_active());
		title->connect(SceneStringName(pressed), callable_mp(this, &AINextTaskTree::_task_pressed).bind(task_id));
		row->add_child(title);

		Label *agent = memnew(Label);
		agent->set_text(String(task.get("assigned_agent_id", String())).replace("_agent", ""));
		agent->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		row->add_child(agent);

		Label *status = memnew(Label);
		status->set_text(status_text.capitalize());
		status->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		row->add_child(status);

		Button *run = memnew(Button);
		run->set_text(status_text == "in_progress" ? TTR("Running") : TTR("Run"));
		run->set_disabled(!next_session->can_run_task(task_id));
		run->connect(SceneStringName(pressed), callable_mp(this, &AINextTaskTree::_run_task_pressed).bind(task_id));
		row->add_child(run);
	}
}
