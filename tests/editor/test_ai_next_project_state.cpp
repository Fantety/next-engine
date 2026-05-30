/**************************************************************************/
/*  test_ai_next_project_state.cpp                                        */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/next/ai_next_event_log.h"
#include "editor/ai_component/next/ai_next_project_state.h"
#include "editor/ai_component/next/ai_next_project_store.h"

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

TEST_CASE("[Editor][AI][NEXT] project store round trips milestones") {
	Ref<AINextProjectStore> store;
	store.instantiate();
	(void)store->delete_project_for_test("test_project");

	Ref<AINextProjectState> state;
	state.instantiate();
	const String milestone_id = state->create_milestone("Inventory", "Basic inventory loop.");
	CHECK_FALSE(milestone_id.is_empty());

	CHECK(store->save("test_project", state).is_empty());
	Ref<AINextProjectState> loaded = store->load("test_project");
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_milestone_count() == 1);

	CHECK(store->delete_project_for_test("test_project"));
}

TEST_CASE("[Editor][AI][NEXT] event log records structured events") {
	Ref<AINextEventLog> event_log;
	event_log.instantiate();

	Dictionary metadata;
	metadata["phase"] = "plan";
	event_log->record_event("planning", "milestone_001", "task_001", "planning_agent", "Generated a plan.", metadata);

	Array events = event_log->get_events();
	REQUIRE(events.size() == 1);
	Dictionary event = events[0];
	CHECK(String(event["event_type"]) == "planning");
	CHECK(String(event["milestone_id"]) == "milestone_001");
	CHECK(String(event["task_id"]) == "task_001");
	CHECK(String(event["agent_id"]) == "planning_agent");
	CHECK(String(Dictionary(event["metadata"])["phase"]) == "plan");
}

} // namespace TestAINextProjectState
