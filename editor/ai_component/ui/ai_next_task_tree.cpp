/**************************************************************************/
/*  ai_next_task_tree.cpp                                                 */
/**************************************************************************/

#include "ai_next_task_tree.h"

#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "servers/text/text_server.h"

void AINextTaskTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextTaskTree::refresh);
}

AINextTaskTree::AINextTaskTree() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);
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
		Dictionary computed_task = next_session->get_project_state()->get_task(String(task.get("id", String())));
		const String status_text = String(computed_task.get("status", task.get("status", "pending")));

		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_theme_constant_override("separation", 6 * EDSCALE);
		add_child(row);

		ColorRect *dot = memnew(ColorRect);
		dot->set_custom_minimum_size(Size2(7, 7) * EDSCALE);
		dot->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
		if (status_text == "completed") {
			dot->set_color(Color(0.24, 0.78, 0.43));
		} else if (status_text == "failed" || status_text == "blocked") {
			dot->set_color(Color(0.92, 0.25, 0.25));
		} else if (status_text == "in_progress" || status_text == "ready") {
			dot->set_color(Color(0.96, 0.69, 0.20));
		} else {
			dot->set_color(Color(0.45, 0.45, 0.45));
		}
		row->add_child(dot);

		Label *title = memnew(Label);
		title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		title->set_text(String(task.get("title", TTR("Task"))));
		title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		title->set_tooltip_text(String(task.get("description", String())));
		row->add_child(title);

		Label *agent = memnew(Label);
		agent->set_text(String(task.get("assigned_agent_id", String())).replace("_agent", ""));
		agent->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		row->add_child(agent);
	}
}
