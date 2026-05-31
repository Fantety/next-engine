/**************************************************************************/
/*  ai_next_panel.cpp                                                     */
/**************************************************************************/

#include "ai_next_panel.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/ui/ai_next_feedback_panel.h"
#include "editor/ai_component/ui/ai_next_milestone_list.h"
#include "editor/ai_component/ui/ai_next_task_inspector.h"
#include "editor/ai_component/ui/ai_next_task_tree.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/separator.h"
#include "scene/gui/text_edit.h"
#include "servers/text/text_server.h"

void AINextPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextPanel::refresh);
}

void AINextPanel::_notification(int p_what) {
	if (p_what != NOTIFICATION_PROCESS || !next_session || !next_session->is_workflow_active()) {
		return;
	}

	spinner_elapsed += get_process_delta_time();
	if (spinner_elapsed < 0.16) {
		return;
	}
	spinner_elapsed = 0.0;
	spinner_frame++;
	_update_operation_label();
}

AINextPanel::AINextPanel() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 6 * EDSCALE);
	set_process(true);

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(header);

	Label *title = memnew(Label);
	title->set_text(TTR("NEXT"));
	title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title->add_theme_font_size_override(SceneStringName(font_size), int(15 * EDSCALE));
	header->add_child(title);

	state_label = memnew(Label);
	state_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	header->add_child(state_label);

	operation_label = memnew(Label);
	operation_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	operation_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	add_child(operation_label);

	Label *steps = memnew(Label);
	steps->set_text(TTR("Brief > Plan > Execute > Test > Lock"));
	steps->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	steps->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	add_child(steps);

	add_child(memnew(HSeparator));
	_add_section_label(TTR("Brief"));

	brief_input = memnew(TextEdit);
	brief_input->set_custom_minimum_size(Size2(0, 72) * EDSCALE);
	brief_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	brief_input->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	brief_input->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	brief_input->set_placeholder(TTR("Build a playable prototype..."));
	brief_input->connect(SNAME("text_changed"), callable_mp(this, &AINextPanel::refresh));
	add_child(brief_input);

	HBoxContainer *brief_actions = memnew(HBoxContainer);
	brief_actions->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	brief_actions->add_theme_constant_override("separation", 4 * EDSCALE);
	add_child(brief_actions);

	submit_button = memnew(Button);
	submit_button->set_text(TTR("Submit Brief"));
	submit_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_submit_brief_pressed));
	brief_actions->add_child(submit_button);

	plan_button = memnew(Button);
	plan_button->set_text(TTR("Generate Milestones"));
	plan_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_generate_plan_pressed));
	brief_actions->add_child(plan_button);

	add_child(memnew(HSeparator));
	_add_section_label(TTR("Milestones"));
	milestone_list = memnew(AINextMilestoneList);
	add_child(milestone_list);

	add_child(memnew(HSeparator));
	_add_section_label(TTR("Tasks"));
	task_tree = memnew(AINextTaskTree);
	add_child(task_tree);

	add_child(memnew(HSeparator));
	_add_section_label(TTR("Task Inspector"));
	task_inspector = memnew(AINextTaskInspector);
	add_child(task_inspector);

	HBoxContainer *run_actions = memnew(HBoxContainer);
	run_actions->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	run_actions->add_theme_constant_override("separation", 4 * EDSCALE);
	add_child(run_actions);

	run_button = memnew(Button);
	run_button->set_text(TTR("Run Milestone"));
	run_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_run_milestone_pressed));
	run_actions->add_child(run_button);

	review_button = memnew(Button);
	review_button->set_text(TTR("Review Milestone"));
	review_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_review_milestone_pressed));
	run_actions->add_child(review_button);

	add_child(memnew(HSeparator));
	_add_section_label(TTR("Activity"));
	activity_list = memnew(VBoxContainer);
	activity_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	activity_list->add_theme_constant_override("separation", 3 * EDSCALE);
	add_child(activity_list);

	add_child(memnew(HSeparator));
	_add_section_label(TTR("Playtest Feedback"));
	feedback_panel = memnew(AINextFeedbackPanel);
	add_child(feedback_panel);
}

Label *AINextPanel::_add_section_label(const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->add_theme_font_size_override(SceneStringName(font_size), int(12 * EDSCALE));
	add_child(label);
	return label;
}

void AINextPanel::set_next_session(AIAgentNextSession *p_session) {
	if (next_session == p_session) {
		return;
	}
	next_session = p_session;
	if (next_session) {
		next_session->connect("project_state_changed", callable_mp(this, &AINextPanel::refresh), CONNECT_DEFERRED);
		next_session->connect("agent_progress_changed", callable_mp(this, &AINextPanel::refresh), CONNECT_DEFERRED);
	}
	if (milestone_list) {
		milestone_list->set_next_session(next_session);
	}
	if (task_tree) {
		task_tree->set_next_session(next_session);
	}
	if (task_inspector) {
		task_inspector->set_next_session(next_session);
	}
	if (feedback_panel) {
		feedback_panel->set_next_session(next_session);
	}
	_refresh();
}

void AINextPanel::_submit_brief_pressed() {
	if (!next_session || !brief_input) {
		return;
	}
	next_session->submit_brief(brief_input->get_text());
}

void AINextPanel::_generate_plan_pressed() {
	if (!next_session || !brief_input) {
		return;
	}
	if (!brief_input->get_text().strip_edges().is_empty()) {
		next_session->submit_brief(brief_input->get_text());
	}
	next_session->generate_plan();
}

void AINextPanel::_run_milestone_pressed() {
	if (!next_session) {
		return;
	}
	next_session->run_active_milestone();
}

void AINextPanel::_review_milestone_pressed() {
	if (!next_session) {
		return;
	}
	next_session->review_active_milestone();
}

void AINextPanel::_update_operation_label() {
	if (!operation_label) {
		return;
	}
	if (!next_session) {
		operation_label->set_text(String());
		return;
	}

	if (next_session->is_workflow_active()) {
		static const char *frames[] = { "-", "\\", "|", "/" };
		const String operation_name = next_session->get_active_operation_name();
		operation_label->set_text(vformat("%s %s", String(frames[spinner_frame % 4]), operation_name.is_empty() ? String("Running NEXT workflow") : operation_name));
		return;
	}

	operation_label->set_text(TTR("Ready"));
}

void AINextPanel::_refresh_activity() {
	if (!activity_list) {
		return;
	}

	while (activity_list->get_child_count() > 0) {
		Node *child = activity_list->get_child(0);
		activity_list->remove_child(child);
		memdelete(child);
	}

	if (!next_session) {
		return;
	}

	int rows_added = 0;
	Array runtime_messages = next_session->get_runtime_messages();
	const int runtime_start = MAX(0, runtime_messages.size() - 3);
	for (int i = runtime_start; i < runtime_messages.size(); i++) {
		if (Variant(runtime_messages[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary message = runtime_messages[i];
		Label *row = memnew(Label);
		row->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		row->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		row->set_text(vformat("%s: %s",
				String(message.get("agent_id", String())).replace("_agent", ""),
				String(message.get("content", String()))));
		activity_list->add_child(row);
		rows_added++;
	}

	if (next_session->get_event_log().is_valid()) {
		Array events = next_session->get_event_log()->get_events();
		const int event_start = MAX(0, events.size() - (6 - rows_added));
		for (int i = event_start; i < events.size() && rows_added < 6; i++) {
			if (Variant(events[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary event = events[i];
			const String type = String(event.get("event_type", String())).replace("_", " ").capitalize();
			const String agent = String(event.get("agent_id", String())).replace("_agent", "");
			const String message = String(event.get("message", String())).strip_edges();

			Label *row = memnew(Label);
			row->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
			row->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
			row->set_text(agent.is_empty() ? vformat("%s: %s", type, message) : vformat("%s [%s]: %s", type, agent, message));
			activity_list->add_child(row);
			rows_added++;
		}
	}

	if (rows_added == 0) {
		Label *empty = memnew(Label);
		empty->set_text(TTR("No activity yet"));
		empty->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		activity_list->add_child(empty);
	}
}

void AINextPanel::_refresh() {
	if (!next_session || next_session->get_project_state().is_null()) {
		if (state_label) {
			state_label->set_text(String());
		}
		if (operation_label) {
			operation_label->set_text(String());
		}
		return;
	}
	const bool running = next_session->is_workflow_active();
	const bool has_brief = brief_input && (!brief_input->get_text().strip_edges().is_empty() || !next_session->get_project_state()->get_brief().strip_edges().is_empty());
	if (state_label) {
		state_label->set_text(next_session->get_project_state()->get_session_state_name().capitalize());
	}
	_update_operation_label();
	if (brief_input) {
		brief_input->set_editable(!running);
	}
	if (submit_button) {
		submit_button->set_disabled(running || !brief_input || brief_input->get_text().strip_edges().is_empty());
	}
	if (plan_button) {
		plan_button->set_disabled(running || !has_brief);
	}
	if (run_button) {
		run_button->set_disabled(!next_session->can_run_active_milestone());
	}
	if (review_button) {
		review_button->set_disabled(!next_session->can_review_active_milestone());
	}
	if (milestone_list) {
		milestone_list->refresh();
	}
	if (task_tree) {
		task_tree->refresh();
	}
	if (task_inspector) {
		task_inspector->refresh();
	}
	if (feedback_panel) {
		feedback_panel->refresh();
	}
	_refresh_activity();
}

void AINextPanel::refresh() {
	_refresh();
}
