/**************************************************************************/
/*  test_ai_next_ui.cpp                                                   */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/next/ai_next_agent_settings.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/ui/ai_agent_next_dock.h"
#include "editor/ai_component/ui/ai_settings_next_page.h"

TEST_FORCE_LINK(test_ai_next_ui);

namespace TestAINextUI {

TEST_CASE("[Editor][AI][NEXT] next dock owns a next session") {
	AIAgentNextDock *dock = memnew(AIAgentNextDock);

	CHECK(dock->get_next_session_for_test() != nullptr);
	CHECK(dock->get_next_session_for_test()->get_project_state().is_valid());

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
