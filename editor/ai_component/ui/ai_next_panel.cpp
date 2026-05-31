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
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "servers/text/text_server.h"

namespace {

void _setup_icon_button(Button *p_button, const String &p_tooltip) {
	ERR_FAIL_NULL(p_button);
	p_button->set_text(String());
	p_button->set_tooltip_text(p_tooltip);
	p_button->set_accessibility_name(p_tooltip.trim_suffix("."));
	p_button->set_custom_minimum_size(Size2(28, 0) * EDSCALE);
}

Ref<ShaderMaterial> _make_operation_progress_material() {
	Ref<Shader> progress_shader;
	progress_shader.instantiate();
	progress_shader->set_code(R"(
shader_type canvas_item;
render_mode unshaded;

uniform vec4 color_a : source_color = vec4(0.09, 0.68, 1.0, 1.0);
uniform vec4 color_b : source_color = vec4(0.53, 0.36, 1.0, 1.0);
uniform vec4 color_c : source_color = vec4(1.0, 0.33, 0.67, 1.0);
uniform vec4 color_d : source_color = vec4(0.18, 0.92, 0.78, 1.0);
uniform float speed = 0.95;

vec3 palette(float t) {
	vec3 a = color_a.rgb;
	vec3 b = color_b.rgb;
	vec3 c = color_c.rgb;
	vec3 d = color_d.rgb;
	return a + b * cos(TAU * (c * t + d));
}

void fragment() {
	float phase = TIME * speed;
	float wave = UV.x * 2.4 - phase;
	vec3 color = palette(wave);
	color += 0.08 * palette(wave + 0.17);
	color += 0.05 * palette(wave + 0.41);
	float edge = smoothstep(0.0, 0.08, min(UV.y, 1.0 - UV.y));
	COLOR = vec4(clamp(color, 0.0, 1.0), edge);
}
)");

	Ref<ShaderMaterial> progress_material;
	progress_material.instantiate();
	progress_material->set_shader(progress_shader);
	return progress_material;
}

} // namespace

void AINextPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AINextPanel::refresh);
}

void AINextPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_POSTINITIALIZE || p_what == NOTIFICATION_THEME_CHANGED) {
		_refresh_theme_icons();
		_update_operation_label();
		return;
	}

	if (p_what != NOTIFICATION_PROCESS || !next_session || !next_session->is_workflow_active()) {
		return;
	}

	spinner_elapsed += get_process_delta_time();
	if (spinner_elapsed < 0.16) {
		return;
	}
	spinner_elapsed = 0.0;
	spinner_frame++;
	if (operation_progress) {
		operation_progress->queue_redraw();
	}
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

	HBoxContainer *workflow_bar = memnew(HBoxContainer);
	workflow_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	workflow_bar->add_theme_constant_override("separation", 4 * EDSCALE);
	add_child(workflow_bar);

	workflow_selector = memnew(OptionButton);
	workflow_selector->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	workflow_selector->set_custom_minimum_size(Size2(96, 0) * EDSCALE);
	workflow_selector->set_fit_to_longest_item(false);
	workflow_selector->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	workflow_selector->connect(SceneStringName(item_selected), callable_mp(this, &AINextPanel::_workflow_selected));
	workflow_bar->add_child(workflow_selector);

	new_workflow_button = memnew(Button);
	_setup_icon_button(new_workflow_button, TTR("Start a new NEXT workflow."));
	new_workflow_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_new_workflow_pressed));
	workflow_bar->add_child(new_workflow_button);

	delete_workflow_button = memnew(Button);
	_setup_icon_button(delete_workflow_button, TTR("Delete the selected NEXT workflow."));
	delete_workflow_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_delete_workflow_pressed));
	workflow_bar->add_child(delete_workflow_button);

	continue_workflow_button = memnew(Button);
	_setup_icon_button(continue_workflow_button, TTR("Continue the interrupted NEXT workflow."));
	continue_workflow_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_continue_workflow_pressed));
	workflow_bar->add_child(continue_workflow_button);

	terminate_workflow_button = memnew(Button);
	_setup_icon_button(terminate_workflow_button, TTR("Stop the active NEXT workflow."));
	terminate_workflow_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_terminate_workflow_pressed));
	workflow_bar->add_child(terminate_workflow_button);

	HBoxContainer *operation_row = memnew(HBoxContainer);
	operation_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	operation_row->add_theme_constant_override("separation", 5 * EDSCALE);
	add_child(operation_row);

	operation_icon = memnew(TextureRect);
	operation_icon->set_custom_minimum_size(Size2(16, 16) * EDSCALE);
	operation_icon->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
	operation_icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	operation_row->add_child(operation_icon);

	operation_label = memnew(Label);
	operation_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	operation_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	operation_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	operation_row->add_child(operation_label);

	operation_progress = memnew(ColorRect);
	operation_progress->set_color(Color(1, 1, 1, 1));
	operation_progress->set_material(_make_operation_progress_material());
	operation_progress->set_custom_minimum_size(Size2(0, 4) * EDSCALE);
	operation_progress->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	operation_progress->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	operation_progress->hide();
	add_child(operation_progress);

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
	_setup_icon_button(submit_button, TTR("Submit the NEXT brief."));
	submit_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_submit_brief_pressed));
	brief_actions->add_child(submit_button);

	plan_button = memnew(Button);
	_setup_icon_button(plan_button, TTR("Generate NEXT milestones."));
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
	_setup_icon_button(run_button, TTR("Run the active milestone."));
	run_button->connect(SceneStringName(pressed), callable_mp(this, &AINextPanel::_run_milestone_pressed));
	run_actions->add_child(run_button);

	review_button = memnew(Button);
	_setup_icon_button(review_button, TTR("Review the active milestone."));
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
		next_session->connect("workflow_session_changed", callable_mp(this, &AINextPanel::refresh), CONNECT_DEFERRED);
		next_session->connect("project_state_changed", callable_mp(this, &AINextPanel::refresh), CONNECT_DEFERRED);
		next_session->connect("agent_progress_changed", callable_mp(this, &AINextPanel::_refresh_progress), CONNECT_DEFERRED);
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

void AINextPanel::_workflow_selected(int p_index) {
	if (!next_session || !workflow_selector || p_index < 0) {
		return;
	}
	if (next_session->is_workflow_active()) {
		_refresh_workflows();
		return;
	}

	const String workflow_id = workflow_selector->get_item_metadata(p_index);
	if (workflow_id.is_empty() || workflow_id == next_session->get_workflow_id()) {
		return;
	}
	if (next_session->load_workflow(workflow_id)) {
		displayed_workflow_id.clear();
		_refresh();
	}
}

void AINextPanel::_new_workflow_pressed() {
	if (!next_session) {
		return;
	}
	next_session->start_new_workflow();
	displayed_workflow_id.clear();
}

void AINextPanel::_delete_workflow_pressed() {
	if (!next_session) {
		return;
	}
	next_session->delete_workflow(next_session->get_workflow_id());
	displayed_workflow_id.clear();
}

void AINextPanel::_continue_workflow_pressed() {
	if (!next_session) {
		return;
	}
	next_session->continue_workflow();
}

void AINextPanel::_terminate_workflow_pressed() {
	if (!next_session) {
		return;
	}
	next_session->cancel_current_operation();
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

void AINextPanel::_refresh_workflows() {
	if (!workflow_selector || !next_session) {
		return;
	}

	workflow_selector->clear();
	Array workflows = next_session->list_workflows();
	for (int i = 0; i < workflows.size(); i++) {
		if (Variant(workflows[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary workflow = workflows[i];
		const String workflow_id = String(workflow.get("id", String()));
		if (workflow_id.is_empty()) {
			continue;
		}
		String title = String(workflow.get("title", TTR("New NEXT Workflow")));
		if (title.length() > 38) {
			title = title.substr(0, 35) + "...";
		}
		const String state = String(workflow.get("session_state", String()));
		if (!state.is_empty()) {
			title += "  " + state.replace("_", " ").capitalize();
		}

		const int index = workflow_selector->get_item_count();
		workflow_selector->add_item(title);
		workflow_selector->set_item_metadata(index, workflow_id);
		if (workflow_id == next_session->get_workflow_id()) {
			workflow_selector->select(index);
		}
	}
}

void AINextPanel::_update_operation_label() {
	if (!operation_label) {
		return;
	}
	if (!next_session) {
		operation_label->set_text(String());
		if (operation_icon) {
			operation_icon->set_texture(Ref<Texture2D>());
			operation_icon->hide();
		}
		if (operation_progress) {
			operation_progress->hide();
		}
		return;
	}

	if (next_session->is_workflow_active()) {
		const String operation_name = next_session->get_active_operation_name();
		operation_label->set_text(operation_name.is_empty() ? TTR("Running NEXT workflow") : operation_name);
		if (operation_icon) {
			operation_icon->set_texture(get_editor_theme_icon(StringName(String("Progress") + itos((spinner_frame % 8) + 1))));
			operation_icon->show();
		}
		if (operation_progress) {
			operation_progress->show();
		}
		return;
	}

	operation_label->set_text(TTR("Ready"));
	if (operation_icon) {
		operation_icon->set_texture(get_editor_theme_icon(SNAME("StatusSuccess")));
		operation_icon->show();
	}
	if (operation_progress) {
		operation_progress->hide();
	}
}

void AINextPanel::_refresh_theme_icons() {
	if (new_workflow_button) {
		new_workflow_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
	}
	if (delete_workflow_button) {
		delete_workflow_button->set_button_icon(get_editor_theme_icon(SNAME("Delete")));
	}
	if (continue_workflow_button) {
		continue_workflow_button->set_button_icon(get_editor_theme_icon(SNAME("DebugContinue")));
	}
	if (terminate_workflow_button) {
		terminate_workflow_button->set_button_icon(get_editor_theme_icon(SNAME("Stop")));
	}
	if (submit_button) {
		submit_button->set_button_icon(get_editor_theme_icon(SNAME("VCSCommit")));
	}
	if (plan_button) {
		plan_button->set_button_icon(get_editor_theme_icon(SNAME("Milestone")));
	}
	if (run_button) {
		run_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
	}
	if (review_button) {
		review_button->set_button_icon(get_editor_theme_icon(SNAME("Search")));
	}
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
	_refresh_workflows();
	if (state_label) {
		state_label->set_text(next_session->get_project_state()->get_session_state_name().capitalize());
	}
	_update_operation_label();
	if (brief_input) {
		brief_input->set_editable(!running);
		if (displayed_workflow_id != next_session->get_workflow_id()) {
			displayed_workflow_id = next_session->get_workflow_id();
			brief_input->set_text(next_session->get_project_state()->get_brief());
		}
	}
	if (workflow_selector) {
		workflow_selector->set_disabled(running);
	}
	if (new_workflow_button) {
		new_workflow_button->set_disabled(running);
	}
	if (delete_workflow_button) {
		delete_workflow_button->set_disabled(running || !workflow_selector || workflow_selector->get_item_count() <= 0);
	}
	if (continue_workflow_button) {
		continue_workflow_button->set_disabled(!next_session->can_continue_workflow());
	}
	if (terminate_workflow_button) {
		terminate_workflow_button->set_disabled(!running);
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

void AINextPanel::_refresh_progress() {
	if (!next_session) {
		return;
	}
	_update_operation_label();
	_refresh_activity();
}

void AINextPanel::refresh() {
	_refresh();
}
