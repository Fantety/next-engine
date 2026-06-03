/**************************************************************************/
/*  test_ai_next_project_state.cpp                                        */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/next/ai_next_event_log.h"
#include "editor/ai_component/next/ai_next_project_state.h"
#include "editor/ai_component/next/ai_next_project_store.h"
#include "editor/ai_component/next/ai_next_workflow_snapshot.h"
#include "editor/ai_component/next/ai_next_workflow_store.h"
#include "editor/ai_component/storage/ai_storage_base.h"

TEST_FORCE_LINK(test_ai_next_project_state);

namespace TestAINextProjectState {

class ExposedAIStorageBase : public AIStorageBase {
	GDCLASS(ExposedAIStorageBase, AIStorageBase);

public:
	void set_base_dir_for_test(const String &p_base_dir) {
		base_dir = p_base_dir;
	}

	String sanitize_for_test(const String &p_segment, const String &p_fallback = "global") const {
		return _sanitize_path_segment(p_segment, p_fallback);
	}

	String get_file_path_for_test(const String &p_id) const {
		return _get_file_path(p_id);
	}
};

static AIAgentMessage _make_next_test_message(AIAgentRole p_role, const String &p_content) {
	AIAgentMessage message;
	message.role = p_role;
	message.content = p_content;
	return message;
}

TEST_CASE("[Editor][AI] Storage base sanitizes path segments and builds JSON paths") {
	Ref<ExposedAIStorageBase> store;
	store.instantiate();
	store->set_base_dir_for_test("user://ai_agent/test_store");

	CHECK(store->sanitize_for_test("") == "global");
	CHECK(store->sanitize_for_test("  ") == "global");
	CHECK(store->sanitize_for_test("bad/name") == String("bad/name").validate_filename());
	CHECK(store->get_file_path_for_test("bad/name").ends_with(String("bad/name").validate_filename() + ".json"));
}

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

TEST_CASE("[Editor][AI][NEXT] workflow snapshot round trips project state event log checkpoint and agent runs") {
	Ref<AINextProjectState> state;
	state.instantiate();
	const String milestone_id = state->create_milestone("Inventory", "Basic inventory loop.");
	const String task_id = state->add_task(milestone_id, "Create inventory script", "script_agent", Array());
	state->set_task_status(task_id, AI_NEXT_TASK_IN_PROGRESS);

	AINextWorkflowSnapshot snapshot;
	snapshot.id = "workflow_roundtrip";
	snapshot.title = "Inventory workflow";
	snapshot.created_at = 100;
	snapshot.updated_at = 200;
	snapshot.project_state = state;

	snapshot.event_log.push_back(Dictionary());
	Dictionary event = snapshot.event_log[0];
	event["event_type"] = "task_started";
	event["milestone_id"] = milestone_id;
	event["task_id"] = task_id;
	event["agent_id"] = "script_agent";
	event["message"] = "Started.";
	snapshot.event_log.set(0, event);

	snapshot.checkpoint.status = "running";
	snapshot.checkpoint.operation = "run_task";
	snapshot.checkpoint.workflow_run_id = "workflow_run_1";
	snapshot.checkpoint.agent_run_id = "agent_run_1";
	snapshot.checkpoint.agent_id = "script_agent";
	snapshot.checkpoint.milestone_id = milestone_id;
	snapshot.checkpoint.task_id = task_id;
	snapshot.checkpoint.single_task_run = true;
	snapshot.checkpoint.selected_task_id = task_id;

	AINextAgentRunState agent_run;
	agent_run.run_id = "agent_run_1";
	agent_run.workflow_id = snapshot.id;
	agent_run.agent_id = "script_agent";
	agent_run.operation = "run_task";
	agent_run.milestone_id = milestone_id;
	agent_run.task_id = task_id;
	agent_run.status = "running";
	agent_run.messages.push_back(_make_next_test_message(AI_AGENT_ROLE_USER, "Run the task."));
	snapshot.agent_runs.push_back(agent_run);

	Dictionary serialized = snapshot.to_dict();
	AINextWorkflowSnapshot restored;
	CHECK(restored.load_from_dict(serialized));

	REQUIRE(restored.project_state.is_valid());
	CHECK(restored.id == snapshot.id);
	CHECK(restored.title == snapshot.title);
	CHECK(restored.project_state->get_milestone_count() == 1);
	CHECK(String(restored.project_state->get_task(task_id).get("status", String())) == "in_progress");
	REQUIRE(restored.event_log.size() == 1);
	CHECK(String(Dictionary(restored.event_log[0]).get("event_type", String())) == "task_started");
	CHECK(restored.checkpoint.operation == "run_task");
	CHECK(restored.checkpoint.agent_run_id == "agent_run_1");
	REQUIRE(restored.agent_runs.size() == 1);
	CHECK(restored.agent_runs[0].messages.size() == 1);
	CHECK(restored.agent_runs[0].messages[0].content == "Run the task.");

	Dictionary metadata = restored.to_metadata().to_dict();
	CHECK(String(metadata.get("id", String())) == snapshot.id);
	CHECK(String(metadata.get("title", String())) == snapshot.title);
	CHECK(bool(metadata.get("has_resumable_checkpoint", false)));
	CHECK((int)metadata.get("milestone_count", 0) == 1);
	CHECK((int)metadata.get("task_count", 0) == 1);
}

TEST_CASE("[Editor][AI][NEXT] workflow store isolates workflows by project scope and lists most recent first") {
	Ref<AINextWorkflowStore> first_store;
	first_store.instantiate();
	first_store->set_project_scope("test_next_workflow_scope_a");

	Ref<AINextWorkflowStore> second_store;
	second_store.instantiate();
	second_store->set_project_scope("test_next_workflow_scope_b");

	first_store->delete_workflow("workflow_old");
	first_store->delete_workflow("workflow_new");
	second_store->delete_workflow("workflow_old");
	second_store->delete_workflow("workflow_new");

	AINextWorkflowSnapshot old_snapshot;
	old_snapshot.id = "workflow_old";
	old_snapshot.title = "Old workflow";
	old_snapshot.created_at = 100;
	old_snapshot.updated_at = 100;
	old_snapshot.project_state.instantiate();
	CHECK_FALSE(old_snapshot.project_state->create_milestone("Old", "Old milestone.").is_empty());

	AINextWorkflowSnapshot new_snapshot;
	new_snapshot.id = "workflow_new";
	new_snapshot.title = "New workflow";
	new_snapshot.created_at = 200;
	new_snapshot.updated_at = 300;
	new_snapshot.project_state.instantiate();
	CHECK_FALSE(new_snapshot.project_state->create_milestone("New", "New milestone.").is_empty());

	CHECK(first_store->save_workflow(old_snapshot) == OK);
	CHECK(first_store->save_workflow(new_snapshot) == OK);
	CHECK(second_store->save_workflow(old_snapshot) == OK);

	AINextWorkflowSnapshot loaded;
	CHECK(first_store->load_workflow("workflow_new", loaded));
	CHECK(loaded.title == "New workflow");
	CHECK_FALSE(second_store->load_workflow("workflow_new", loaded));
	CHECK(first_store->get_base_dir_for_test() != second_store->get_base_dir_for_test());

	Array listed = first_store->list_workflows();
	REQUIRE(listed.size() >= 2);
	Dictionary first = listed[0];
	CHECK(String(first.get("id", String())) == "workflow_new");

	String most_recent_id;
	CHECK(first_store->get_most_recent_workflow_id(most_recent_id));
	CHECK(most_recent_id == "workflow_new");

	CHECK(first_store->delete_workflow("workflow_old"));
	CHECK(first_store->delete_workflow("workflow_new"));
	CHECK(second_store->delete_workflow("workflow_old"));
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

TEST_CASE("[Editor][AI][NEXT] project state supports failed task recovery") {
	Ref<AINextProjectState> state;
	state.instantiate();
	const String milestone_id = state->create_milestone("Core Movement", "Player movement baseline.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());

	CHECK(state->mark_task_failed(task_id, "Script tool failed."));
	CHECK(String(state->get_task(task_id)["status"]) == "failed");

	CHECK(state->retry_task(task_id));
	CHECK(String(state->get_task(task_id)["status"]) == "ready");
	CHECK(String(state->get_task(task_id)["error"]).is_empty());

	CHECK(state->reassign_task(task_id, "scene_agent"));
	CHECK(String(state->get_task(task_id)["assigned_agent_id"]) == "scene_agent");

	CHECK(state->mark_task_failed(task_id, "Needs to be split."));
	Dictionary first_split;
	first_split["title"] = "Create movement resource";
	first_split["assigned_agent_id"] = "script_agent";
	Dictionary second_split;
	second_split["title"] = "Assemble movement scene";
	second_split["assigned_agent_id"] = "scene_agent";
	Array split_tasks;
	split_tasks.push_back(first_split);
	split_tasks.push_back(second_split);

	String error;
	CHECK(state->split_task(task_id, split_tasks, error));
	CHECK(error.is_empty());
	CHECK(state->get_task_count(milestone_id) == 3);
	CHECK(String(state->get_task(task_id)["status"]) == "skipped");
}

TEST_CASE("[Editor][AI][NEXT] project state supports user plan editing operations") {
	Ref<AINextProjectState> state;
	state.instantiate();

	const String first_milestone = state->create_milestone("Core Movement", "Build movement.");
	const String second_milestone = state->create_milestone("Combat", "Build combat.");
	const String third_milestone = state->create_milestone("Polish", "Add juice.");
	const String script_task = state->add_task(first_milestone, "Create movement script", "script_agent", Array());
	Array scene_dependencies;
	scene_dependencies.push_back(script_task);
	const String scene_task = state->add_task(first_milestone, "Assemble movement scene", "scene_agent", scene_dependencies);
	const String combat_task = state->add_task(second_milestone, "Create attack scene", "scene_agent", Array());
	CHECK_FALSE(third_milestone.is_empty());
	CHECK_FALSE(combat_task.is_empty());

	String error;
	CHECK(state->move_task(scene_task, first_milestone, 0, error));
	CHECK(error.is_empty());
	CHECK(String(Dictionary(Array(state->get_milestone(first_milestone).get("tasks", Array()))[0]).get("id", String())) == scene_task);

	CHECK(state->move_task(scene_task, first_milestone, 1, error));
	CHECK(error.is_empty());
	CHECK(String(Dictionary(Array(state->get_milestone(first_milestone).get("tasks", Array()))[1]).get("id", String())) == scene_task);

	CHECK(state->move_task(scene_task, second_milestone, 0, error));
	CHECK(error.is_empty());
	CHECK(state->get_task_milestone_id(scene_task) == second_milestone);
	CHECK(String(Dictionary(Array(state->get_milestone(second_milestone).get("tasks", Array()))[0]).get("id", String())) == scene_task);

	CHECK(state->move_milestone(second_milestone, 0, error));
	CHECK(error.is_empty());
	Array milestones = state->get_milestones_as_array();
	REQUIRE(milestones.size() == 3);
	CHECK(String(Dictionary(milestones[0]).get("id", String())) == second_milestone);
	CHECK(state->get_active_milestone_id() == first_milestone);

	CHECK(state->set_task_dependencies(scene_task, Array(), error));
	CHECK(error.is_empty());
	CHECK(Array(state->get_task(scene_task).get("depends_on", Array())).is_empty());

	Array reverse_dependencies;
	reverse_dependencies.push_back(scene_task);
	CHECK(state->set_task_dependencies(script_task, reverse_dependencies, error));
	CHECK(error.is_empty());

	Array cyclic_dependencies;
	cyclic_dependencies.push_back(script_task);
	CHECK_FALSE(state->set_task_dependencies(scene_task, cyclic_dependencies, error));
	CHECK(error.contains("cycle"));
	CHECK(Array(state->get_task(scene_task).get("depends_on", Array())).is_empty());

	CHECK(state->delete_task(script_task, error));
	CHECK(error.is_empty());
	CHECK_FALSE(state->has_task(script_task));
	CHECK_FALSE(Array(state->get_task(scene_task).get("depends_on", Array())).has(script_task));

	CHECK(state->merge_milestones(second_milestone, first_milestone, error));
	CHECK(error.is_empty());
	CHECK_FALSE(state->has_milestone(first_milestone));
	CHECK(state->get_task_milestone_id(scene_task) == second_milestone);
	CHECK(state->get_milestone_count() == 2);

	CHECK(state->delete_milestone(third_milestone, error));
	CHECK(error.is_empty());
	CHECK_FALSE(state->has_milestone(third_milestone));
	CHECK(state->get_milestone_count() == 1);
}

TEST_CASE("[Editor][AI][NEXT] project state rolls back task patch when dependency validation fails") {
	Ref<AINextProjectState> state;
	state.instantiate();

	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create movement script", "script_agent", Array());

	Dictionary patch;
	patch["title"] = "Edited movement script";
	patch["assigned_agent_id"] = "scene_agent";
	Array dependencies;
	dependencies.push_back("missing_task");
	patch["depends_on"] = dependencies;

	String error;
	CHECK_FALSE(state->update_task(task_id, patch, error));
	CHECK(error.contains("dependency"));

	Dictionary task = state->get_task(task_id);
	CHECK(String(task.get("title", String())) == "Create movement script");
	CHECK(String(task.get("assigned_agent_id", String())) == "script_agent");
	CHECK(Array(task.get("depends_on", Array())).is_empty());
}

TEST_CASE("[Editor][AI][NEXT] project state rejects edits to locked milestones") {
	Ref<AINextProjectState> state;
	state.instantiate();

	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create movement script", "script_agent", Array());
	CHECK(state->mark_task_completed(task_id, "Done.", Array()));

	String error;
	CHECK(state->lock_milestone(milestone_id, error));
	CHECK(error.is_empty());

	Dictionary milestone_patch;
	milestone_patch["title"] = "Edited movement";
	CHECK_FALSE(state->update_milestone(milestone_id, milestone_patch, error));
	CHECK(error.contains("locked"));
	CHECK(String(state->get_milestone(milestone_id).get("title", String())) == "Core Movement");

	Dictionary task_patch;
	task_patch["title"] = "Edited task";
	CHECK_FALSE(state->update_task(task_id, task_patch, error));
	CHECK(error.contains("locked"));
	CHECK(String(state->get_task(task_id).get("title", String())) == "Create movement script");

	CHECK(state->add_task(milestone_id, "New locked task", "script_agent", Array()).is_empty());
	CHECK(state->get_last_error().contains("locked"));
	CHECK(state->get_task_count(milestone_id) == 1);

	CHECK_FALSE(state->delete_task(task_id, error));
	CHECK(error.contains("locked"));
	CHECK_FALSE(state->delete_milestone(milestone_id, error));
	CHECK(error.contains("locked"));
	CHECK_FALSE(state->move_milestone(milestone_id, 0, error));
	CHECK(error.contains("locked"));
}

} // namespace TestAINextProjectState
