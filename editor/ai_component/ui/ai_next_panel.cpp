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

AINextPanel::AINextPanel() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 6 * EDSCALE);

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
	add_child(brief_input);

	HBoxContainer *brief_actions = memnew(HBoxContainer);
	brief_actions->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	brief_actions->add_theme_constant_override("separation", 4 * EDSCALE);
	add_child(brief_actions);

	Button *submit_button = memnew(Button);
	submit_button->set_text(TTR("Submit Brief"));
	submit_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_submit_brief_pressed));
	brief_actions->add_child(submit_button);

	Button *plan_button = memnew(Button);
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

	Button *run_button = memnew(Button);
	run_button->set_text(TTR("Run Milestone"));
	run_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_run_milestone_pressed));
	run_actions->add_child(run_button);

	Button *review_button = memnew(Button);
	review_button->set_text(TTR("Review Milestone"));
	review_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_review_milestone_pressed));
	run_actions->add_child(review_button);

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

void AINextPanel::_refresh() {
	if (!next_session || next_session->get_project_state().is_null()) {
		if (state_label) {
			state_label->set_text(String());
		}
		return;
	}
	if (state_label) {
		state_label->set_text(next_session->get_project_state()->get_session_state_name().capitalize());
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
}

void AINextPanel::refresh() {
	_refresh();
}
