/**************************************************************************/
/*  test_ai_next_agent_registry.cpp                                       */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/next/ai_next_agent_registry.h"

TEST_FORCE_LINK(test_ai_next_agent_registry);

namespace TestAINextAgentRegistry {

TEST_CASE("[Editor][AI][NEXT] agent registry exposes default agent descriptors") {
	Vector<String> agent_ids = AINextAgentRegistry::get_agent_ids();
	REQUIRE(agent_ids.size() == 5);
	CHECK(agent_ids[0] == "planning_agent");
	CHECK(agent_ids[1] == "script_agent");
	CHECK(agent_ids[2] == "scene_agent");
	CHECK(agent_ids[3] == "shader_agent");
	CHECK(agent_ids[4] == "review_agent");

	CHECK(AINextAgentRegistry::get_display_name("planning_agent") == "Planning Agent");
	CHECK(AINextAgentRegistry::get_display_name("script_agent") == "Script Agent");
	CHECK(AINextAgentRegistry::get_display_name("scene_agent") == "Scene Agent");
	CHECK(AINextAgentRegistry::get_display_name("shader_agent") == "Shader Agent");
	CHECK(AINextAgentRegistry::get_display_name("review_agent") == "Review Agent");
	CHECK(AINextAgentRegistry::get_display_name("unknown_agent") == "unknown_agent");

	CHECK(AINextAgentRegistry::get_default_profile_id("planning_agent") == "ask");
	CHECK(AINextAgentRegistry::get_default_profile_id("script_agent") == "auto");
	CHECK(AINextAgentRegistry::get_default_profile_id("scene_agent") == "auto");
	CHECK(AINextAgentRegistry::get_default_profile_id("shader_agent") == "auto");
	CHECK(AINextAgentRegistry::get_default_profile_id("review_agent") == "ask");
	CHECK(AINextAgentRegistry::get_default_profile_id("unknown_agent").is_empty());

	CHECK(AINextAgentRegistry::is_valid_agent_id("script_agent"));
	CHECK_FALSE(AINextAgentRegistry::is_valid_agent_id("unknown_agent"));
}

TEST_CASE("[Editor][AI][NEXT] agent registry marks only implementation agents assignable to tasks") {
	Vector<String> assignable_agent_ids = AINextAgentRegistry::get_assignable_agent_ids();
	REQUIRE(assignable_agent_ids.size() == 3);
	CHECK(assignable_agent_ids[0] == "script_agent");
	CHECK(assignable_agent_ids[1] == "scene_agent");
	CHECK(assignable_agent_ids[2] == "shader_agent");

	CHECK(AINextAgentRegistry::get_default_task_agent_id() == "script_agent");
	CHECK(AINextAgentRegistry::is_assignable_agent_id("script_agent"));
	CHECK(AINextAgentRegistry::is_assignable_agent_id("scene_agent"));
	CHECK(AINextAgentRegistry::is_assignable_agent_id("shader_agent"));
	CHECK_FALSE(AINextAgentRegistry::is_assignable_agent_id("planning_agent"));
	CHECK_FALSE(AINextAgentRegistry::is_assignable_agent_id("review_agent"));
	CHECK_FALSE(AINextAgentRegistry::is_assignable_agent_id("unknown_agent"));
}

} // namespace TestAINextAgentRegistry
