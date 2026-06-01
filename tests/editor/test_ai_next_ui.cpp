/**************************************************************************/
/*  test_ai_next_ui.cpp                                                   */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/object/message_queue.h"
#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/next/ai_next_workflow_snapshot.h"
#include "editor/ai_component/next/ai_next_agent_settings.h"
#include "editor/ai_component/next/ai_next_workflow_store.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/ui/next/ai_agent_next_dock.h"
#include "editor/ai_component/ui/next/ai_next_panel.h"
#include "editor/ai_component/ui/next/ai_next_milestone_list.h"
#include "editor/ai_component/ui/next/ai_next_task_tree.h"
#include "editor/ai_component/ui/next/ai_settings_next_page.h"
#include "editor/run/editor_run_bar.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/foldable_container.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/scroll_container.h"
#include "servers/text/text_server.h"

TEST_FORCE_LINK(test_ai_next_ui);

namespace TestAINextUI {

static void cleanup_next_workflows(const String &p_project_scope) {
	Ref<AINextWorkflowStore> store;
	store.instantiate();
	store->set_project_scope(p_project_scope);

	Array workflows = store->list_workflows();
	for (int i = 0; i < workflows.size(); i++) {
		if (Variant(workflows[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary workflow = workflows[i];
		store->delete_workflow(String(workflow.get("id", String())));
	}
}

static void flush_message_queue() {
	if (MessageQueue::get_main_singleton()) {
		MessageQueue::get_main_singleton()->flush();
		MessageQueue::get_main_singleton()->flush();
	}
}

static OptionButton *find_first_option_button(Node *p_root) {
	if (!p_root) {
		return nullptr;
	}
	OptionButton *option_button = Object::cast_to<OptionButton>(p_root);
	if (option_button) {
		return option_button;
	}
	for (int i = 0; i < p_root->get_child_count(); i++) {
		OptionButton *child_option_button = find_first_option_button(p_root->get_child(i));
		if (child_option_button) {
			return child_option_button;
		}
	}
	return nullptr;
}

static Button *find_button_by_text(Node *p_root, const String &p_text) {
	if (!p_root) {
		return nullptr;
	}
	Button *button = Object::cast_to<Button>(p_root);
	if (button && button->get_text() == p_text) {
		return button;
	}
	for (int i = 0; i < p_root->get_child_count(); i++) {
		Button *child_button = find_button_by_text(p_root->get_child(i), p_text);
		if (child_button) {
			return child_button;
		}
	}
	return nullptr;
}

static FoldableContainer *find_foldable_by_title(Node *p_root, const String &p_title) {
	if (!p_root) {
		return nullptr;
	}
	FoldableContainer *foldable = Object::cast_to<FoldableContainer>(p_root);
	if (foldable && foldable->get_title() == p_title) {
		return foldable;
	}
	for (int i = 0; i < p_root->get_child_count(); i++) {
		FoldableContainer *child_foldable = find_foldable_by_title(p_root->get_child(i), p_title);
		if (child_foldable) {
			return child_foldable;
		}
	}
	return nullptr;
}

static Label *find_label_by_name(Node *p_root, const StringName &p_name) {
	if (!p_root) {
		return nullptr;
	}
	Label *label = Object::cast_to<Label>(p_root);
	if (label && label->get_name() == p_name) {
		return label;
	}
	for (int i = 0; i < p_root->get_child_count(); i++) {
		Label *child_label = find_label_by_name(p_root->get_child(i), p_name);
		if (child_label) {
			return child_label;
		}
	}
	return nullptr;
}

TEST_CASE("[Editor][AI][NEXT] run bar uses a switch control for next mode") {
	EditorRunBar *run_bar = memnew(EditorRunBar);

	Button *next_mode_control = find_button_by_text(run_bar, "NEXT");
	REQUIRE(next_mode_control != nullptr);
	CHECK(Object::cast_to<CheckButton>(next_mode_control) != nullptr);

	run_bar->set_next_mode_enabled(true);
	CHECK(next_mode_control->is_pressed());

	run_bar->set_next_mode_enabled(false);
	CHECK_FALSE(next_mode_control->is_pressed());

	memdelete(run_bar);
}

TEST_CASE("[Editor][AI][NEXT] next dock owns a next session") {
	AIAgentNextDock *dock = memnew(AIAgentNextDock);

	CHECK(dock->get_next_session_for_test() != nullptr);
	CHECK(dock->get_next_session_for_test()->get_project_state().is_valid());

	memdelete(dock);
}

TEST_CASE("[Editor][AI][NEXT] next dock wraps long panel content in a scroll container") {
	AIAgentNextDock *dock = memnew(AIAgentNextDock);

	REQUIRE(dock->get_child_count() >= 2);
	ScrollContainer *scroll = Object::cast_to<ScrollContainer>(dock->get_child(1));
	CHECK(scroll != nullptr);
	if (scroll) {
		CHECK(scroll->get_horizontal_scroll_mode() == ScrollContainer::SCROLL_MODE_DISABLED);
		CHECK(scroll->get_vertical_scroll_mode() == ScrollContainer::SCROLL_MODE_AUTO);
		CHECK((scroll->get_h_size_flags() & Control::SIZE_EXPAND_FILL) == Control::SIZE_EXPAND_FILL);
		CHECK((scroll->get_v_size_flags() & Control::SIZE_EXPAND_FILL) == Control::SIZE_EXPAND_FILL);

		AINextPanel *panel = nullptr;
		for (int i = 0; i < scroll->get_child_count(); i++) {
			panel = Object::cast_to<AINextPanel>(scroll->get_child(i));
			if (panel) {
				break;
			}
		}
		REQUIRE(panel != nullptr);
		CHECK((panel->get_h_size_flags() & Control::SIZE_EXPAND_FILL) == Control::SIZE_EXPAND_FILL);
	}

	memdelete(dock);
}

TEST_CASE("[Editor][AI][NEXT] next dock applies configured model profile to next session") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	Dictionary original_next_settings = AINextAgentSettings::get_agent_model_storage_for_test();
	AIModelSettings::clear_model_profiles_for_test();
	AINextAgentSettings::clear_agent_models_for_test();
	const String profile_id = AIModelSettings::add_model_profile("NEXT Test", "deepseek", "deepseek-chat", "next-test-key", "https://api.deepseek.com", false);
	CHECK_FALSE(profile_id.is_empty());

	AIAgentNextDock *dock = memnew(AIAgentNextDock);
	AIAgentNextSession *session = dock->get_next_session_for_test();
	REQUIRE(session != nullptr);
	Ref<AIAgentBase> planning_agent = session->get_agent_for_test("planning_agent");
	REQUIRE(planning_agent.is_valid());
	CHECK(planning_agent->get_provider_config().api_key == "next-test-key");
	CHECK(planning_agent->get_provider_config().model == "deepseek-chat");

	memdelete(dock);
	AINextAgentSettings::set_agent_model_storage_for_test(original_next_settings);
	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
}

TEST_CASE("[Editor][AI][NEXT] milestone list renders milestone icon before each title") {
	const String project_scope = "test_next_ui_milestone_icons";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);
	CHECK_FALSE(session->get_project_state()->create_milestone("Core Movement", "Build movement.").is_empty());

	AINextMilestoneList *list = memnew(AINextMilestoneList);
	list->set_next_session(session);

	REQUIRE(list->get_child_count() == 1);
	HBoxContainer *row = Object::cast_to<HBoxContainer>(list->get_child(0));
	REQUIRE(row != nullptr);
	Button *title = Object::cast_to<Button>(row->get_child(0));
	REQUIRE(title != nullptr);
	CHECK(title->get_button_icon().is_valid());

	memdelete(list);
	session->delete_workflow(session->get_workflow_id());
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] task tree renders status icons before task titles") {
	const String project_scope = "test_next_ui_task_status_icons";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);
	const String milestone_id = session->get_project_state()->create_milestone("Core Movement", "Build movement.");
	const String completed_task = session->get_project_state()->add_task(milestone_id, "Completed script", "script_agent", Array());
	const String ready_task = session->get_project_state()->add_task(milestone_id, "Ready scene", "scene_agent", Array());
	const String failed_task = session->get_project_state()->add_task(milestone_id, "Failed shader", "shader_agent", Array());
	session->get_project_state()->mark_task_completed(completed_task, "Done.", Array());
	session->get_project_state()->mark_task_failed(failed_task, "Failed.");

	AINextTaskTree *tree = memnew(AINextTaskTree);
	tree->set_next_session(session);

	REQUIRE(tree->get_child_count() == 3);
	for (int i = 0; i < tree->get_child_count(); i++) {
		HBoxContainer *row = Object::cast_to<HBoxContainer>(tree->get_child(i));
		REQUIRE(row != nullptr);
		Button *title = Object::cast_to<Button>(row->get_child(0));
		REQUIRE(title != nullptr);
		CHECK(title->get_button_icon().is_valid());
	}

	memdelete(tree);
	session->delete_workflow(session->get_workflow_id());
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] progress refresh keeps workflow selector cached") {
	const String project_scope = "test_next_ui_progress_refresh_cached_workflows";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);
	session->submit_brief("Primary workflow.");

	AINextPanel *panel = memnew(AINextPanel);
	panel->set_next_session(session);

	OptionButton *workflow_selector = find_first_option_button(panel);
	REQUIRE(workflow_selector != nullptr);
	const int initial_workflow_count = workflow_selector->get_item_count();
	REQUIRE(initial_workflow_count >= 1);

	AINextWorkflowSnapshot external_snapshot;
	external_snapshot.id = "external_progress_refresh_workflow";
	external_snapshot.title = "External workflow";
	external_snapshot.project_state.instantiate();
	REQUIRE(session->get_workflow_store_for_test()->save_workflow(external_snapshot) == OK);

	session->emit_signal(SNAME("agent_progress_changed"));
	flush_message_queue();

	CHECK(workflow_selector->get_item_count() == initial_workflow_count);

	memdelete(panel);
	session->delete_workflow(session->get_workflow_id());
	session->get_workflow_store_for_test()->delete_workflow(external_snapshot.id);
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] long detail sections use compact collapsed foldables") {
	const String project_scope = "test_next_ui_compact_foldable_sections";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);

	AINextPanel *panel = memnew(AINextPanel);
	panel->set_next_session(session);

	FoldableContainer *task_section = find_foldable_by_title(panel, "Task Inspector");
	FoldableContainer *activity_section = find_foldable_by_title(panel, "Activity");
	CHECK(task_section != nullptr);
	CHECK(activity_section != nullptr);
	if (!task_section || !activity_section) {
		memdelete(panel);
		session->delete_workflow(session->get_workflow_id());
		memdelete(session);
		cleanup_next_workflows(project_scope);
		return;
	}

	CHECK(task_section->is_folded());
	CHECK(activity_section->is_folded());
	CHECK(task_section->get_title_text_overrun_behavior() == TextServer::OVERRUN_TRIM_ELLIPSIS);
	CHECK(activity_section->get_title_text_overrun_behavior() == TextServer::OVERRUN_TRIM_ELLIPSIS);

	Label *task_summary = find_label_by_name(task_section, SNAME("TaskInspectorSummary"));
	Label *activity_summary = find_label_by_name(activity_section, SNAME("ActivitySummary"));
	CHECK(task_summary != nullptr);
	CHECK(activity_summary != nullptr);
	if (!task_summary || !activity_summary) {
		memdelete(panel);
		session->delete_workflow(session->get_workflow_id());
		memdelete(session);
		cleanup_next_workflows(project_scope);
		return;
	}
	CHECK(task_summary->get_text() == "No task selected");
	CHECK(activity_summary->get_text() == "No activity yet");

	memdelete(panel);
	session->delete_workflow(session->get_workflow_id());
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] collapsed task inspector summarizes selected task") {
	const String project_scope = "test_next_ui_task_summary";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);
	const String milestone_id = session->get_project_state()->create_milestone("Core Movement", "Build movement.");
	const String task_id = session->get_project_state()->add_task(milestone_id, "Implement dash controller", "script_agent", Array());
	REQUIRE_FALSE(task_id.is_empty());
	REQUIRE(session->select_task(task_id));

	AINextPanel *panel = memnew(AINextPanel);
	panel->set_next_session(session);

	Label *task_summary = find_label_by_name(panel, SNAME("TaskInspectorSummary"));
	CHECK(task_summary != nullptr);
	if (!task_summary) {
		memdelete(panel);
		session->delete_workflow(session->get_workflow_id());
		memdelete(session);
		cleanup_next_workflows(project_scope);
		return;
	}
	CHECK(task_summary->get_text().contains("Implement dash controller"));
	CHECK(task_summary->get_text().contains("Ready"));
	CHECK(task_summary->get_text().contains("script"));

	memdelete(panel);
	session->delete_workflow(session->get_workflow_id());
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] collapsed activity summarizes recent event") {
	const String project_scope = "test_next_ui_activity_summary";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);
	const String milestone_id = session->get_project_state()->create_milestone("Core Movement", "Build movement.");
	const String task_id = session->get_project_state()->add_task(milestone_id, "Implement dash controller", "script_agent", Array());
	session->get_event_log()->record_event("task_started", milestone_id, task_id, "script_agent", "Writing movement controller.");

	AINextPanel *panel = memnew(AINextPanel);
	panel->set_next_session(session);

	Label *activity_summary = find_label_by_name(panel, SNAME("ActivitySummary"));
	CHECK(activity_summary != nullptr);
	if (!activity_summary) {
		memdelete(panel);
		session->delete_workflow(session->get_workflow_id());
		memdelete(session);
		cleanup_next_workflows(project_scope);
		return;
	}
	CHECK(activity_summary->get_text().contains("Task Started"));
	CHECK(activity_summary->get_text().contains("script"));
	CHECK(activity_summary->get_text().contains("Writing movement controller."));

	memdelete(panel);
	session->delete_workflow(session->get_workflow_id());
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] theme refresh rebuilds milestone and task icon rows") {
	const String project_scope = "test_next_ui_theme_refresh_icon_rows";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);

	AINextMilestoneList *milestone_list = memnew(AINextMilestoneList);
	milestone_list->set_next_session(session);
	REQUIRE(milestone_list->get_child_count() == 1);
	CHECK(Object::cast_to<Label>(milestone_list->get_child(0)) != nullptr);

	const String milestone_id = session->get_project_state()->create_milestone("Theme Ready Milestone", "Build after theme readiness.");
	const String task_id = session->get_project_state()->add_task(milestone_id, "Theme Ready Task", "script_agent", Array());
	REQUIRE_FALSE(task_id.is_empty());
	milestone_list->notification(Control::NOTIFICATION_THEME_CHANGED);

	REQUIRE(milestone_list->get_child_count() == 1);
	HBoxContainer *milestone_row = Object::cast_to<HBoxContainer>(milestone_list->get_child(0));
	REQUIRE(milestone_row != nullptr);
	Button *milestone_title = Object::cast_to<Button>(milestone_row->get_child(0));
	REQUIRE(milestone_title != nullptr);
	CHECK(milestone_title->get_button_icon().is_valid());

	AINextTaskTree *task_tree = memnew(AINextTaskTree);
	task_tree->set_next_session(session);
	REQUIRE(task_tree->get_child_count() == 1);
	session->get_project_state()->mark_task_completed(task_id, "Done.", Array());
	task_tree->notification(Control::NOTIFICATION_THEME_CHANGED);

	REQUIRE(task_tree->get_child_count() == 1);
	HBoxContainer *task_row = Object::cast_to<HBoxContainer>(task_tree->get_child(0));
	REQUIRE(task_row != nullptr);
	Button *task_title = Object::cast_to<Button>(task_row->get_child(0));
	REQUIRE(task_title != nullptr);
	CHECK(task_title->get_button_icon().is_valid());

	memdelete(task_tree);
	memdelete(milestone_list);
	session->delete_workflow(session->get_workflow_id());
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] next settings page stores per-agent model choices") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	Dictionary original_next_settings = AINextAgentSettings::get_agent_model_storage_for_test();
	AIModelSettings::clear_model_profiles_for_test();
	AINextAgentSettings::clear_agent_models_for_test();

	const String planning_profile_id = AIModelSettings::add_model_profile("NEXT Planner", "openai", "gpt-5.4", "planner-key", "https://planner.example.test/v1", false);
	const String writer_profile_id = AIModelSettings::add_model_profile("NEXT Writer", "deepseek", "deepseek-chat", "writer-key", "https://api.deepseek.com", false);
	REQUIRE_FALSE(planning_profile_id.is_empty());
	REQUIRE_FALSE(writer_profile_id.is_empty());

	AISettingsNextPage *page = memnew(AISettingsNextPage);
	page->build_for_test();
	CHECK(page->get_agent_model_row_count_for_test() == 5);

	page->set_agent_model_for_test("planning_agent", planning_profile_id);
	page->set_agent_model_for_test("script_agent", writer_profile_id);

	CHECK(AINextAgentSettings::get_model_profile_id("planning_agent") == planning_profile_id);
	CHECK(AINextAgentSettings::get_model_profile_id("script_agent") == writer_profile_id);
	CHECK(AINextAgentSettings::get_effective_model_profile_id("scene_agent") == planning_profile_id);

	memdelete(page);
	AINextAgentSettings::set_agent_model_storage_for_test(original_next_settings);
	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
}

} // namespace TestAINextUI
