/**************************************************************************/
/*  ai_next_milestone_list.cpp                                            */
/**************************************************************************/

#include "ai_next_milestone_list.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "servers/text/text_server.h"

void AINextMilestoneList::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextMilestoneList::refresh);
}

AINextMilestoneList::AINextMilestoneList() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 4 * EDSCALE);
}

void AINextMilestoneList::_clear_rows() {
	while (get_child_count() > 0) {
		Node *child = get_child(0);
		remove_child(child);
		memdelete(child);
	}
}

void AINextMilestoneList::set_next_session(AIAgentNextSession *p_session) {
	next_session = p_session;
	refresh();
}

void AINextMilestoneList::_milestone_pressed(const String &p_milestone_id) {
	if (!next_session) {
		return;
	}
	next_session->select_milestone(p_milestone_id);
}

void AINextMilestoneList::refresh() {
	_clear_rows();
	if (!next_session || next_session->get_project_state().is_null()) {
		return;
	}

	const String active_milestone_id = next_session->get_project_state()->get_active_milestone_id();
	Array milestones = next_session->get_project_state()->get_milestones_as_array();
	if (milestones.is_empty()) {
		Label *empty = memnew(Label);
		empty->set_text(TTR("No milestones"));
		empty->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		add_child(empty);
		return;
	}

	for (int i = 0; i < milestones.size(); i++) {
		if (Variant(milestones[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary milestone = milestones[i];
		const String milestone_id = String(milestone.get("id", String()));
		const bool active = milestone_id == active_milestone_id;
		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_theme_constant_override("separation", 6 * EDSCALE);
		add_child(row);

		Button *title = memnew(Button);
		title->set_flat(true);
		title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		title->set_text(vformat("%s%02d %s", active ? "> " : "", i + 1, String(milestone.get("title", TTR("Milestone")))));
		title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		title->set_tooltip_text(String(milestone.get("description", String())));
		title->set_disabled(next_session->is_workflow_active());
		title->connect(SceneStringName(pressed), callable_mp(this, &AINextMilestoneList::_milestone_pressed).bind(milestone_id));
		row->add_child(title);

		Label *status = memnew(Label);
		status->set_text(String(milestone.get("status", "draft")).capitalize());
		status->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		row->add_child(status);
	}
}
