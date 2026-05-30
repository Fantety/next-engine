/**************************************************************************/
/*  test_ai_agent_next_session.cpp                                        */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/next/ai_agent_next_session.h"

TEST_FORCE_LINK(test_ai_agent_next_session);

namespace TestAIAgentNextSession {

class NextScriptedRuntimeClient : public AIAgentRuntimeClient {
	GDCLASS(NextScriptedRuntimeClient, AIAgentRuntimeClient);

	Vector<AIAgentRuntimeResponse> responses;

public:
	int request_count = 0;
	Array last_tool_schemas;

	void push_response(const AIAgentRuntimeResponse &p_response) {
		responses.push_back(p_response);
	}

	virtual AIAgentRuntimeResponse complete(const Array &p_messages, const Array &p_tool_schemas) override {
		(void)p_messages;
		request_count++;
		last_tool_schemas = p_tool_schemas;
		if (responses.is_empty()) {
			AIAgentRuntimeResponse response;
			response.error = "No scripted NEXT response.";
			return response;
		}
		AIAgentRuntimeResponse response = responses[0];
		responses.remove_at(0);
		return response;
	}

	virtual AIAgentRuntimeResponse complete_streaming(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback) override {
		(void)p_partial_response_callback;
		return complete(p_messages, p_tool_schemas);
	}
};

TEST_CASE("[Editor][AI][NEXT] session initializes independent agents") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);

	CHECK(session->get_project_state().is_valid());
	CHECK(session->has_agent("planning_agent"));
	CHECK(session->has_agent("script_agent"));
	CHECK(session->has_agent("scene_agent"));
	CHECK(session->has_agent("shader_agent"));
	CHECK(session->has_agent("review_agent"));

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session generates structured plan through NEXT tool") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_replace_plan";
	call.tool_name = "ai_next.manage_project";
	call.arguments["action"] = "replace_plan";

	Dictionary milestone;
	milestone["title"] = "Core Movement";
	milestone["description"] = "Build movement.";
	Array tasks;
	tasks.push_back(Dictionary());
	Dictionary task = tasks[0];
	task["id"] = "task_script";
	task["title"] = "Create player script";
	task["assigned_agent_id"] = "script_agent";
	tasks.set(0, task);
	milestone["tasks"] = tasks;
	Array milestones;
	milestones.push_back(milestone);
	call.arguments["milestones"] = milestones;
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "Plan generated.";
	client->push_response(final_response);

	session->get_agent_for_test("planning_agent")->set_runtime_client(client);
	session->submit_brief("Build a 2D movement prototype.");
	session->generate_plan();

	CHECK(session->get_project_state()->get_brief() == "Build a 2D movement prototype.");
	CHECK(session->get_project_state()->get_milestone_count() == 1);
	CHECK(session->get_project_state()->get_session_state() == AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	CHECK(client->request_count == 2);
	CHECK(client->last_tool_schemas.size() >= 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session runs ready milestone tasks serially") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Created player_controller.gd";
	client->push_response(final_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	session->run_active_milestone();

	Dictionary task = state->get_task(task_id);
	CHECK(String(task["status"]) == "completed");
	CHECK(String(task["result_summary"]) == "Created player_controller.gd");
	CHECK(state->get_session_state() == AI_NEXT_SESSION_WAITING_PLAYTEST);
	CHECK(client->request_count == 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session batches ready tasks up to the NEXT scheduling limit") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String script_task = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	const String scene_task = state->add_task(milestone_id, "Assemble player scene", "scene_agent", Array());
	const String shader_task = state->add_task(milestone_id, "Create dash shader", "shader_agent", Array());
	CHECK_FALSE(script_task.is_empty());
	CHECK_FALSE(scene_task.is_empty());
	CHECK_FALSE(shader_task.is_empty());

	Ref<NextScriptedRuntimeClient> script_client;
	script_client.instantiate();
	AIAgentRuntimeResponse script_response;
	script_response.content = "Created player_controller.gd";
	script_client->push_response(script_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(script_client);

	Ref<NextScriptedRuntimeClient> scene_client;
	scene_client.instantiate();
	AIAgentRuntimeResponse scene_response;
	scene_response.content = "Created player.tscn";
	scene_client->push_response(scene_response);
	session->get_agent_for_test("scene_agent")->set_runtime_client(scene_client);

	Ref<NextScriptedRuntimeClient> shader_client;
	shader_client.instantiate();
	AIAgentRuntimeResponse shader_response;
	shader_response.content = "Created dash.shader";
	shader_client->push_response(shader_response);
	session->get_agent_for_test("shader_agent")->set_runtime_client(shader_client);

	session->run_active_milestone();

	Array events = session->get_event_log()->get_events();
	Vector<int> batch_sizes;
	for (int i = 0; i < events.size(); i++) {
		Dictionary event = events[i];
		if (String(event.get("event_type", String())) == "task_batch_started") {
			Dictionary metadata = event.get("metadata", Dictionary());
			batch_sizes.push_back((int)metadata.get("batch_size", 0));
		}
	}
	REQUIRE(batch_sizes.size() == 2);
	CHECK(batch_sizes[0] == 2);
	CHECK(batch_sizes[1] == 1);
	CHECK(script_client->request_count == 1);
	CHECK(scene_client->request_count == 1);
	CHECK(shader_client->request_count == 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session serializes ready tasks with planned output conflicts") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String script_task = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	const String scene_task = state->add_task(milestone_id, "Assemble player scene", "scene_agent", Array());

	Array shared_outputs;
	shared_outputs.push_back("res://player.tscn");
	Dictionary patch;
	patch["output_paths"] = shared_outputs;
	String error;
	CHECK(state->update_task(script_task, patch, error));
	CHECK(state->update_task(scene_task, patch, error));

	Ref<NextScriptedRuntimeClient> script_client;
	script_client.instantiate();
	AIAgentRuntimeResponse script_response;
	script_response.content = "Created script.";
	script_client->push_response(script_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(script_client);

	Ref<NextScriptedRuntimeClient> scene_client;
	scene_client.instantiate();
	AIAgentRuntimeResponse scene_response;
	scene_response.content = "Created scene.";
	scene_client->push_response(scene_response);
	session->get_agent_for_test("scene_agent")->set_runtime_client(scene_client);

	session->run_active_milestone();

	Array events = session->get_event_log()->get_events();
	Vector<int> batch_sizes;
	for (int i = 0; i < events.size(); i++) {
		Dictionary event = events[i];
		if (String(event.get("event_type", String())) == "task_batch_started") {
			Dictionary metadata = event.get("metadata", Dictionary());
			batch_sizes.push_back((int)metadata.get("batch_size", 0));
		}
	}
	REQUIRE(batch_sizes.size() == 2);
	CHECK(batch_sizes[0] == 1);
	CHECK(batch_sizes[1] == 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session turns feedback into NEXT tasks") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	state->mark_task_completed(task_id, "Created player_controller.gd", Array());

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_append_tasks";
	call.tool_name = "ai_next.manage_project";
	call.arguments["action"] = "append_tasks";
	call.arguments["milestone_id"] = milestone_id;

	Dictionary repair_task;
	repair_task["id"] = "task_fix_jump";
	repair_task["title"] = "Tune jump landing feedback";
	repair_task["assigned_agent_id"] = "script_agent";
	Array repair_tasks;
	repair_tasks.push_back(repair_task);
	call.arguments["tasks"] = repair_tasks;
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "Feedback tasks generated.";
	client->push_response(final_response);
	session->get_agent_for_test("planning_agent")->set_runtime_client(client);

	session->generate_feedback_tasks("Jump is too floaty.");

	CHECK(state->get_task_count(milestone_id) == 2);
	CHECK(state->get_session_state() == AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	CHECK(client->request_count == 2);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session records review agent findings") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	state->mark_task_completed(task_id, "Created player_controller.gd", Array());

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Finding: Jump landing feedback is unclear.";
	client->push_response(final_response);
	session->get_agent_for_test("review_agent")->set_runtime_client(client);

	session->review_active_milestone();

	Array events = session->get_event_log()->get_events();
	Dictionary review_event;
	for (int i = 0; i < events.size(); i++) {
		Dictionary event = events[i];
		if (String(event.get("event_type", String())) == "review_completed") {
			review_event = event;
		}
	}
	REQUIRE_FALSE(review_event.is_empty());
	CHECK(String(review_event["milestone_id"]) == milestone_id);
	CHECK(String(review_event["message"]) == "Finding: Jump landing feedback is unclear.");
	CHECK(state->get_session_state() == AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	CHECK(client->request_count == 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session locks completed active milestone") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	state->mark_task_completed(task_id, "Created player_controller.gd", Array());

	session->accept_and_lock_active_milestone();

	Dictionary milestone = state->get_milestone(milestone_id);
	CHECK(String(milestone["status"]) == "locked");
	CHECK(state->get_session_state() == AI_NEXT_SESSION_READY_TO_LOCK);

	memdelete(session);
}

} // namespace TestAIAgentNextSession
