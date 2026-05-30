/**************************************************************************/
/*  test_ai_next_project_state.cpp                                        */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/next/ai_next_project_state.h"

TEST_FORCE_LINK(test_ai_next_project_state);

namespace TestAINextProjectState {

TEST_CASE("[Editor][AI][NEXT] project state marks tasks ready only after dependencies complete") {
	Ref<AINextProjectState> state;
	state.instantiate();

	String milestone_id = state->create_milestone("Core Movement", "Player movement baseline.");
	String script_task = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	Array scene_dependencies;
	scene_dependencies.push_back(script_task);
	String scene_task = state->add_task(milestone_id, "Assemble player scene", "scene_agent", scene_dependencies);

	Array ready = state->get_ready_tasks(milestone_id);
	CHECK(ready.size() == 1);

	Array output_paths;
	output_paths.push_back("res://player_controller.gd");
	state->mark_task_completed(script_task, "Created player_controller.gd", output_paths);
	ready = state->get_ready_tasks(milestone_id);
	REQUIRE(ready.size() == 1);
	CHECK(String(Dictionary(ready[0]).get("id", "")) == scene_task);
}

} // namespace TestAINextProjectState
