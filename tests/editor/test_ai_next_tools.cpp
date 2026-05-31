/**************************************************************************/
/*  test_ai_next_tools.cpp                                                */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/next/ai_next_manage_project_tool.h"
#include "editor/ai_component/next/ai_next_project_state.h"

TEST_FORCE_LINK(test_ai_next_tools);

namespace TestAINextTools {

static Dictionary _make_task(const String &p_id, const String &p_title, const String &p_agent_id, const Array &p_depends_on = Array()) {
	Dictionary task;
	task["id"] = p_id;
	task["title"] = p_title;
	task["assigned_agent_id"] = p_agent_id;
	task["depends_on"] = p_depends_on;
	return task;
}

TEST_CASE("[Editor][AI][NEXT] manage project tool creates structured milestones") {
	Ref<AINextProjectState> state;
	state.instantiate();
	Ref<AINextManageProjectTool> tool;
	tool.instantiate();
	tool->set_project_state(state);

	Dictionary milestone;
	milestone["title"] = "Core Movement";
	milestone["description"] = "Build player movement.";
	milestone["tasks"] = Array();

	Array milestones;
	milestones.push_back(milestone);

	Dictionary args;
	args["action"] = "replace_plan";
	args["milestones"] = milestones;

	AIToolResult result = tool->execute(args);
	CHECK(result.error.is_empty());
	CHECK(state->get_milestone_count() == 1);
}

TEST_CASE("[Editor][AI][NEXT] manage project tool rejects invalid plans without partial writes") {
	Ref<AINextProjectState> state;
	state.instantiate();
	Ref<AINextManageProjectTool> tool;
	tool.instantiate();
	tool->set_project_state(state);

	Array duplicate_tasks;
	duplicate_tasks.push_back(_make_task("task_same", "Create player script", "script_agent"));
	duplicate_tasks.push_back(_make_task("task_same", "Assemble player scene", "scene_agent"));

	Dictionary duplicate_milestone;
	duplicate_milestone["title"] = "Core Movement";
	duplicate_milestone["tasks"] = duplicate_tasks;

	Array duplicate_milestones;
	duplicate_milestones.push_back(duplicate_milestone);

	Dictionary duplicate_args;
	duplicate_args["action"] = "replace_plan";
	duplicate_args["milestones"] = duplicate_milestones;

	AIToolResult duplicate_result = tool->execute(duplicate_args);
	CHECK(duplicate_result.is_error());
	CHECK(state->get_milestone_count() == 0);

	Array unknown_agent_tasks;
	unknown_agent_tasks.push_back(_make_task("task_unknown", "Do work", "unknown_agent"));

	Dictionary unknown_agent_milestone;
	unknown_agent_milestone["title"] = "Unknown Agent";
	unknown_agent_milestone["tasks"] = unknown_agent_tasks;

	Array unknown_agent_milestones;
	unknown_agent_milestones.push_back(unknown_agent_milestone);

	Dictionary unknown_agent_args;
	unknown_agent_args["action"] = "replace_plan";
	unknown_agent_args["milestones"] = unknown_agent_milestones;

	AIToolResult unknown_agent_result = tool->execute(unknown_agent_args);
	CHECK(unknown_agent_result.is_error());
	CHECK(state->get_milestone_count() == 0);

	Array t1_depends;
	t1_depends.push_back("task_two");
	Array t2_depends;
	t2_depends.push_back("task_one");

	Array cycle_tasks;
	cycle_tasks.push_back(_make_task("task_one", "First", "script_agent", t1_depends));
	cycle_tasks.push_back(_make_task("task_two", "Second", "scene_agent", t2_depends));

	Dictionary cycle_milestone;
	cycle_milestone["title"] = "Cycle";
	cycle_milestone["tasks"] = cycle_tasks;

	Array cycle_milestones;
	cycle_milestones.push_back(cycle_milestone);

	Dictionary cycle_args;
	cycle_args["action"] = "replace_plan";
	cycle_args["milestones"] = cycle_milestones;

	AIToolResult cycle_result = tool->execute(cycle_args);
	CHECK(cycle_result.is_error());
	CHECK(state->get_milestone_count() == 0);
}

TEST_CASE("[Editor][AI][NEXT] manage project tool accepts acyclic shared dependencies") {
	Ref<AINextProjectState> state;
	state.instantiate();
	Ref<AINextManageProjectTool> tool;
	tool.instantiate();
	tool->set_project_state(state);

	Array build_scene_depends;
	build_scene_depends.push_back("task_script");
	Array polish_depends;
	polish_depends.push_back("task_script");
	polish_depends.push_back("task_scene");

	Array tasks;
	tasks.push_back(_make_task("task_script", "Create player script", "script_agent"));
	tasks.push_back(_make_task("task_scene", "Assemble player scene", "scene_agent", build_scene_depends));
	tasks.push_back(_make_task("task_polish", "Polish player feedback", "script_agent", polish_depends));

	Dictionary milestone;
	milestone["title"] = "Core Movement";
	milestone["tasks"] = tasks;

	Array milestones;
	milestones.push_back(milestone);

	Dictionary args;
	args["action"] = "replace_plan";
	args["milestones"] = milestones;

	AIToolResult result = tool->execute(args);
	CHECK(result.error.is_empty());
	CHECK(state->get_milestone_count() == 1);
}

TEST_CASE("[Editor][AI][NEXT] manage project tool rejects cyclic task updates without partial writes") {
	Ref<AINextProjectState> state;
	state.instantiate();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String first_task = state->add_task(milestone_id, "First", "script_agent", Array());
	const String second_task = state->add_task(milestone_id, "Second", "scene_agent", Array());

	Ref<AINextManageProjectTool> tool;
	tool.instantiate();
	tool->set_project_state(state);

	Array first_depends;
	first_depends.push_back(second_task);
	Dictionary first_patch;
	first_patch["depends_on"] = first_depends;

	Dictionary first_args;
	first_args["action"] = "update_task";
	first_args["task_id"] = first_task;
	first_args["task"] = first_patch;
	CHECK_FALSE(tool->execute(first_args).is_error());

	Array second_depends;
	second_depends.push_back(first_task);
	Dictionary second_patch;
	second_patch["depends_on"] = second_depends;

	Dictionary second_args;
	second_args["action"] = "update_task";
	second_args["task_id"] = second_task;
	second_args["task"] = second_patch;
	CHECK(tool->execute(second_args).is_error());

	Dictionary second_after = state->get_task(second_task);
	Array persisted_depends = second_after["depends_on"];
	CHECK(persisted_depends.is_empty());
}

} // namespace TestAINextTools
