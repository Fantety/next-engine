/**************************************************************************/
/*  test_ai_next_ui.cpp                                                   */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/next/ai_next_agent_settings.h"
#include "editor/ai_component/next/ai_next_workflow_store.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/ui/ai_agent_next_dock.h"
#include "editor/ai_component/ui/ai_next_panel.h"
#include "editor/ai_component/ui/ai_next_milestone_list.h"
#include "editor/ai_component/ui/ai_next_task_tree.h"
#include "editor/ai_component/ui/ai_settings_next_page.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/scroll_container.h"

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
