/**************************************************************************/
/*  test_ai_agent_next_session.cpp                                        */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/next/agents/ai_next_planning_agent.h"
#include "editor/ai_component/next/agents/ai_next_review_agent.h"
#include "editor/ai_component/next/agents/ai_next_scene_agent.h"
#include "editor/ai_component/next/agents/ai_next_script_agent.h"
#include "editor/ai_component/next/agents/ai_next_shader_agent.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/next/ai_next_workflow_snapshot.h"
#include "editor/ai_component/next/ai_next_workflow_store.h"
#include "editor/ai_component/tools/project/ai_requirement_form_tool.h"

TEST_FORCE_LINK(test_ai_agent_next_session);

namespace TestAIAgentNextSession {

class NextScriptedRuntimeClient : public AIAgentRuntimeClient {
	GDCLASS(NextScriptedRuntimeClient, AIAgentRuntimeClient);

	Vector<AIAgentRuntimeResponse> responses;

public:
	int request_count = 0;
	Array last_messages;
	Array last_tool_schemas;
	Vector<Array> messages_history;

	void push_response(const AIAgentRuntimeResponse &p_response) {
		responses.push_back(p_response);
	}

	virtual AIAgentRuntimeResponse complete(const Array &p_messages, const Array &p_tool_schemas) override {
		request_count++;
		last_messages = p_messages;
		last_tool_schemas = p_tool_schemas;
		messages_history.push_back(p_messages.duplicate(true));
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

class BlockingNextRuntimeClient : public AIAgentRuntimeClient {
	GDCLASS(BlockingNextRuntimeClient, AIAgentRuntimeClient);

	Vector<AIAgentRuntimeResponse> responses;

public:
	Semaphore request_started;
	Semaphore release_request;
	bool request_was_on_main_thread = true;
	int request_count = 0;

	void push_response(const AIAgentRuntimeResponse &p_response) {
		responses.push_back(p_response);
	}

	virtual AIAgentRuntimeResponse complete(const Array &p_messages, const Array &p_tool_schemas) override {
		(void)p_messages;
		(void)p_tool_schemas;

		request_count++;
		request_was_on_main_thread = Thread::is_main_thread();
		request_started.post();
		if (request_was_on_main_thread) {
			AIAgentRuntimeResponse response;
			response.error = "NEXT runtime ran on the main thread.";
			return response;
		}

		release_request.wait();
		if (responses.is_empty()) {
			AIAgentRuntimeResponse response;
			response.content = "Done.";
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

class StreamingProgressNextRuntimeClient : public NextScriptedRuntimeClient {
	GDCLASS(StreamingProgressNextRuntimeClient, NextScriptedRuntimeClient);

public:
	Semaphore progress_sent;
	Semaphore release_request;
	bool wait_for_release = false;

	virtual AIAgentRuntimeResponse complete_streaming(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback) override {
		Dictionary partial;
		partial["content"] = "Working on NEXT task...";
		p_partial_response_callback.call(partial);
		progress_sent.post();
		if (wait_for_release) {
			release_request.wait();
		}
		return NextScriptedRuntimeClient::complete(p_messages, p_tool_schemas);
	}
};

class BlockingSecondTurnNextRuntimeClient : public AIAgentRuntimeClient {
	GDCLASS(BlockingSecondTurnNextRuntimeClient, AIAgentRuntimeClient);

	Vector<AIAgentRuntimeResponse> responses;

public:
	Semaphore second_request_started;
	Semaphore release_second_request;
	int request_count = 0;
	Array last_messages;
	Array last_tool_schemas;

	void push_response(const AIAgentRuntimeResponse &p_response) {
		responses.push_back(p_response);
	}

	virtual AIAgentRuntimeResponse complete(const Array &p_messages, const Array &p_tool_schemas) override {
		request_count++;
		last_messages = p_messages;
		last_tool_schemas = p_tool_schemas;

		if (request_count == 2) {
			second_request_started.post();
			release_second_request.wait();
		}

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

static bool wait_for_semaphore(const Semaphore &p_semaphore, uint64_t p_timeout_msec = 1000) {
	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	while (OS::get_singleton()->get_ticks_msec() - start_time < p_timeout_msec) {
		if (p_semaphore.try_wait()) {
			return true;
		}
		OS::get_singleton()->delay_usec(1000);
	}
	return false;
}

static void flush_message_queue() {
	if (MessageQueue::get_main_singleton()) {
		MessageQueue::get_main_singleton()->flush();
		MessageQueue::get_main_singleton()->flush();
	}
}

static void wait_for_next_agent(AIAgentNextSession *p_session, const String &p_agent_id) {
	Ref<AIAgentBase> agent = p_session->get_agent_for_test(p_agent_id);
	REQUIRE(agent.is_valid());
	agent->get_runtime_runner()->wait_to_finish();
	flush_message_queue();
}

static AIAgentMessage _make_next_session_message(AIAgentRole p_role, const String &p_content) {
	AIAgentMessage message;
	message.role = p_role;
	message.content = p_content;
	return message;
}

static Dictionary _make_next_image_attachment(const String &p_path) {
	Dictionary attachment;
	attachment["type"] = "image";
	attachment["path"] = p_path;
	attachment["mime_type"] = "image/png";
	attachment["detail"] = "auto";
	return attachment;
}

static Array _make_next_image_attachments(const String &p_path) {
	Array attachments;
	attachments.push_back(_make_next_image_attachment(p_path));
	return attachments;
}

static bool _messages_have_attachment_path(const Array &p_messages, const String &p_path) {
	for (int i = 0; i < p_messages.size(); i++) {
		if (Variant(p_messages[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary message = p_messages[i];
		if (!message.has("metadata") || Variant(message["metadata"]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary metadata = message["metadata"];
		if (!metadata.has("attachments") || Variant(metadata["attachments"]).get_type() != Variant::ARRAY) {
			continue;
		}
		Array attachments = metadata["attachments"];
		for (int j = 0; j < attachments.size(); j++) {
			if (Variant(attachments[j]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary attachment = attachments[j];
			if (String(attachment.get("path", String())) == p_path) {
				return true;
			}
		}
	}
	return false;
}

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

static AIAgentNextSession *make_isolated_next_session() {
	static int isolated_scope_counter = 0;
	const String project_scope = "test_next_session_isolated_" + itos(++isolated_scope_counter);
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);
	return session;
}

TEST_CASE("[Editor][AI][NEXT] session initializes independent agents") {
	AIAgentNextSession *session = make_isolated_next_session();

	CHECK(session->get_project_state().is_valid());
	CHECK(session->has_agent("planning_agent"));
	CHECK(session->has_agent("script_agent"));
	CHECK(session->has_agent("scene_agent"));
	CHECK(session->has_agent("shader_agent"));
	CHECK(session->has_agent("review_agent"));
	CHECK(Object::cast_to<AINextPlanningAgent>(*session->get_agent_for_test("planning_agent")) != nullptr);
	CHECK(Object::cast_to<AINextScriptAgent>(*session->get_agent_for_test("script_agent")) != nullptr);
	CHECK(Object::cast_to<AINextSceneAgent>(*session->get_agent_for_test("scene_agent")) != nullptr);
	CHECK(Object::cast_to<AINextShaderAgent>(*session->get_agent_for_test("shader_agent")) != nullptr);
	CHECK(Object::cast_to<AINextReviewAgent>(*session->get_agent_for_test("review_agent")) != nullptr);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] planning agent prompt defines detailed task planning standards") {
	AIAgentNextSession *session = make_isolated_next_session();
	const Ref<AIAgentBase> planning_agent = session->get_agent_for_test("planning_agent");
	REQUIRE(planning_agent.is_valid());

	const String prompt = planning_agent->get_system_prompt();
	CHECK(prompt.contains("Task template"));
	CHECK(prompt.contains("Acceptance criteria"));
	CHECK(prompt.contains("Self-review"));
	CHECK(prompt.contains("ai_next.manage_project"));

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session manages multiple persisted workflows per project") {
	const String project_scope = "test_next_session_multi_workflows";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);

	const String first_workflow_id = session->get_workflow_id();
	session->submit_brief("First persisted NEXT workflow.");
	CHECK(session->get_workflow_title() == "First persisted NEXT workflow.");

	session->start_new_workflow();
	const String second_workflow_id = session->get_workflow_id();
	session->submit_brief("Second persisted NEXT workflow.");

	CHECK(first_workflow_id != second_workflow_id);
	Array workflows = session->list_workflows();
	CHECK(workflows.size() >= 2);

	CHECK(session->load_workflow(first_workflow_id));
	CHECK(session->get_workflow_id() == first_workflow_id);
	CHECK(session->get_project_state()->get_brief() == "First persisted NEXT workflow.");

	CHECK(session->load_workflow(second_workflow_id));
	CHECK(session->get_workflow_id() == second_workflow_id);
	CHECK(session->get_project_state()->get_brief() == "Second persisted NEXT workflow.");

	CHECK(session->delete_workflow(second_workflow_id));
	CHECK(session->get_workflow_id() == first_workflow_id);
	CHECK(session->get_project_state()->get_brief() == "First persisted NEXT workflow.");

	session->delete_workflow(first_workflow_id);
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] user termination persists resumable checkpoint and ignores late task result") {
	const String project_scope = "test_next_session_user_termination";
	cleanup_next_workflows(project_scope);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());

	Ref<BlockingNextRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Created player_controller.gd";
	client->push_response(final_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	CHECK(session->run_task(task_id));
	CHECK(wait_for_semaphore(client->request_started));
	CHECK(String(state->get_task(task_id).get("status", String())) == "in_progress");

	session->cancel_current_operation();

	Dictionary checkpoint = session->get_workflow_checkpoint_for_test();
	CHECK(String(checkpoint.get("status", String())) == "user_terminated");
	CHECK(String(checkpoint.get("operation", String())) == "run_task");
	CHECK(String(state->get_task(task_id).get("status", String())) == "ready");
	CHECK(session->can_continue_workflow());

	client->release_request.post();
	wait_for_next_agent(session, "script_agent");

	CHECK(String(state->get_task(task_id).get("status", String())) == "ready");
	CHECK_FALSE(session->is_workflow_active());

	const String workflow_id = session->get_workflow_id();
	memdelete(session);

	AIAgentNextSession *restored_session = memnew(AIAgentNextSession);
	restored_session->set_workflow_project_scope_for_test(project_scope);
	CHECK(restored_session->get_workflow_id() == workflow_id);
	CHECK(restored_session->can_continue_workflow());
	CHECK(String(restored_session->get_project_state()->get_task(task_id).get("status", String())) == "ready");

	restored_session->delete_workflow(workflow_id);
	memdelete(restored_session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] continue workflow resumes sub-agent messages from persisted run state") {
	const String project_scope = "test_next_session_resume_agent_messages";
	cleanup_next_workflows(project_scope);

	Ref<AINextWorkflowStore> store;
	store.instantiate();
	store->set_project_scope(project_scope);

	AINextWorkflowSnapshot snapshot;
	snapshot.id = "workflow_resume_messages";
	snapshot.title = "Resume messages";
	snapshot.created_at = 100;
	snapshot.updated_at = 200;
	snapshot.project_state.instantiate();
	const String milestone_id = snapshot.project_state->create_milestone("Core Movement", "Build movement.");
	const String task_id = snapshot.project_state->add_task(milestone_id, "Create player script", "script_agent", Array());
	snapshot.project_state->set_task_status(task_id, AI_NEXT_TASK_IN_PROGRESS);

	snapshot.checkpoint.status = "user_terminated";
	snapshot.checkpoint.operation = "run_task";
	snapshot.checkpoint.agent_id = "script_agent";
	snapshot.checkpoint.agent_run_id = "agent_run_resume";
	snapshot.checkpoint.milestone_id = milestone_id;
	snapshot.checkpoint.task_id = task_id;
	snapshot.checkpoint.single_task_run = true;
	snapshot.checkpoint.selected_task_id = task_id;

	AINextAgentRunState run_state;
	run_state.run_id = "agent_run_resume";
	run_state.workflow_id = snapshot.id;
	run_state.agent_id = "script_agent";
	run_state.operation = "run_task";
	run_state.milestone_id = milestone_id;
	run_state.task_id = task_id;
	run_state.status = "user_terminated";
	run_state.messages.push_back(_make_next_session_message(AI_AGENT_ROLE_USER, "Run the original task."));
	run_state.messages.push_back(_make_next_session_message(AI_AGENT_ROLE_ASSISTANT, "I inspected existing files."));
	run_state.messages.push_back(_make_next_session_message(AI_AGENT_ROLE_TOOL, "Prior tool result."));
	snapshot.agent_runs.push_back(run_state);
	CHECK(store->save_workflow(snapshot) == OK);

	AIAgentNextSession *session = memnew(AIAgentNextSession);
	session->set_workflow_project_scope_for_test(project_scope);

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Created player_controller.gd";
	client->push_response(final_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	CHECK(session->continue_workflow());
	wait_for_next_agent(session, "script_agent");

	bool saw_persisted_agent_context = false;
	for (int i = 0; i < client->last_messages.size(); i++) {
		Dictionary message = client->last_messages[i];
		const String content = String(message.get("content", String()));
		if (content.contains("I inspected existing files.") || content.contains("Prior tool result.")) {
			saw_persisted_agent_context = true;
		}
	}
	CHECK(saw_persisted_agent_context);
	CHECK(String(session->get_project_state()->get_task(task_id).get("status", String())) == "completed");

	session->delete_workflow(snapshot.id);
	memdelete(session);
	cleanup_next_workflows(project_scope);
}

TEST_CASE("[Editor][AI][NEXT] generated plans are self-reviewed before human approval") {
	AIAgentNextSession *session = make_isolated_next_session();

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse initial_tool_response;
	AIToolCall replace_call;
	replace_call.id = "call_replace_plan";
	replace_call.tool_name = "ai_next.manage_project";
	replace_call.arguments["action"] = "replace_plan";

	Dictionary milestone;
	milestone["title"] = "Core Movement";
	milestone["description"] = "Build movement.";
	Array tasks;
	Dictionary task;
	task["id"] = "task_script";
	task["title"] = "Create player script";
	task["assigned_agent_id"] = "script_agent";
	task["description"] = "Create player movement.";
	tasks.push_back(task);
	milestone["tasks"] = tasks;
	Array milestones;
	milestones.push_back(milestone);
	replace_call.arguments["milestones"] = milestones;
	initial_tool_response.tool_calls.push_back(replace_call);
	client->push_response(initial_tool_response);

	AIAgentRuntimeResponse initial_final_response;
	initial_final_response.content = "Initial plan generated.";
	client->push_response(initial_final_response);

	AIAgentRuntimeResponse refine_tool_response;
	AIToolCall update_call;
	update_call.id = "call_update_task";
	update_call.tool_name = "ai_next.manage_project";
	update_call.arguments["action"] = "update_task";
	update_call.arguments["task_id"] = "task_script";
	Dictionary task_patch;
	task_patch["description"] = "Goal: create the player movement script.\nContext: use the project tree and current Godot conventions before editing.\nSteps: inspect existing scripts, create or update the controller, expose speed/jump tuning, and avoid scene assembly.\nAcceptance criteria: script compiles, movement parameters are exported, and output_paths includes res://scripts/player_controller.gd.\nExpected output_paths: res://scripts/player_controller.gd.";
	Array output_paths;
	output_paths.push_back("res://scripts/player_controller.gd");
	task_patch["output_paths"] = output_paths;
	update_call.arguments["task"] = task_patch;
	refine_tool_response.tool_calls.push_back(update_call);
	client->push_response(refine_tool_response);

	AIAgentRuntimeResponse refine_final_response;
	refine_final_response.content = "Plan self-review complete.";
	client->push_response(refine_final_response);

	session->get_agent_for_test("planning_agent")->set_runtime_client(client);
	session->submit_brief("Build a 2D movement prototype.");
	session->generate_plan();
	wait_for_next_agent(session, "planning_agent");
	wait_for_next_agent(session, "planning_agent");

	Dictionary refined_task = session->get_project_state()->get_task("task_script");
	CHECK(client->request_count == 4);
	CHECK(session->get_project_state()->get_session_state() == AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	CHECK(String(refined_task.get("description", String())).contains("Acceptance criteria"));
	CHECK(refined_task.has("output_paths"));

	bool saw_refinement_prompt = false;
	for (int i = 0; i < client->last_messages.size(); i++) {
		Dictionary message = client->last_messages[i];
		if (String(message.get("content", String())).contains("Review the generated NEXT plan")) {
			saw_refinement_prompt = true;
		}
	}
	CHECK(saw_refinement_prompt);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] planning can collect requirement form answers before writing the plan") {
	AIAgentNextSession *session = make_isolated_next_session();

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();

	Dictionary question;
	question["id"] = "visual_style";
	question["label"] = "Visual style";
	question["type"] = "single_choice";
	Array options;
	options.push_back("Pixel art");
	options.push_back("Flat UI");
	question["options"] = options;
	Array questions;
	questions.push_back(question);

	AIAgentRuntimeResponse form_response;
	AIToolCall form_call;
	form_call.id = "call_requirements";
	form_call.tool_name = AIRequirementFormTool::TOOL_NAME;
	form_call.arguments["title"] = "Confirm platformer requirements";
	form_call.arguments["purpose"] = "Clarify style and rules before planning.";
	form_call.arguments["questions"] = questions;
	form_response.tool_calls.push_back(form_call);
	client->push_response(form_response);

	AIAgentRuntimeResponse replace_response;
	AIToolCall replace_call;
	replace_call.id = "call_replace_plan";
	replace_call.tool_name = "ai_next.manage_project";
	replace_call.arguments["action"] = "replace_plan";
	Dictionary milestone;
	milestone["title"] = "Core Loop";
	milestone["description"] = "Build the confirmed pixel-art platformer loop.";
	Array tasks;
	Dictionary task;
	task["id"] = "task_player";
	task["title"] = "Create player movement";
	task["assigned_agent_id"] = "script_agent";
	task["description"] = "Implement player movement using the confirmed Pixel art requirement.\nAcceptance criteria: movement works and output_paths lists res://scripts/player_controller.gd.";
	Array output_paths;
	output_paths.push_back("res://scripts/player_controller.gd");
	task["output_paths"] = output_paths;
	tasks.push_back(task);
	milestone["tasks"] = tasks;
	Array milestones;
	milestones.push_back(milestone);
	replace_call.arguments["milestones"] = milestones;
	replace_response.tool_calls.push_back(replace_call);
	client->push_response(replace_response);

	AIAgentRuntimeResponse initial_final_response;
	initial_final_response.content = "Plan generated from confirmed requirements.";
	client->push_response(initial_final_response);

	AIAgentRuntimeResponse refine_final_response;
	refine_final_response.content = "Plan self-review complete.";
	client->push_response(refine_final_response);

	session->get_agent_for_test("planning_agent")->set_runtime_client(client);
	session->submit_brief("Make a platformer.");
	session->generate_plan();
	wait_for_next_agent(session, "planning_agent");

	Dictionary pending_form = session->get_pending_requirement_form();
	REQUIRE_FALSE(pending_form.is_empty());
	CHECK(String(pending_form.get("tool_name", "")) == AIRequirementFormTool::TOOL_NAME);
	CHECK(session->get_project_state()->get_session_state() == AI_NEXT_SESSION_PLANNING);

	Dictionary answers;
	answers["visual_style"] = "Pixel art";
	CHECK(session->submit_pending_requirement_form(answers));
	wait_for_next_agent(session, "planning_agent");
	wait_for_next_agent(session, "planning_agent");

	CHECK(client->request_count == 4);
	CHECK(session->get_project_state()->get_session_state() == AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	CHECK(session->get_pending_requirement_form().is_empty());

	bool saw_requirement_tool_context = false;
	REQUIRE(client->messages_history.size() >= 2);
	Array resumed_messages = client->messages_history[1];
	for (int i = 0; i < resumed_messages.size(); i++) {
		Dictionary message = resumed_messages[i];
		if (String(message.get("role", "")) == "tool" && String(message.get("content", "")).contains("Pixel art")) {
			saw_requirement_tool_context = true;
		}
	}
	CHECK(saw_requirement_tool_context);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] agent operations run runtime work off the main thread") {
	{
		AIAgentNextSession *session = make_isolated_next_session();
		Ref<BlockingNextRuntimeClient> client;
		client.instantiate();
		session->get_agent_for_test("planning_agent")->set_runtime_client(client);

		session->submit_brief("Build a 2D movement prototype.");
		session->generate_plan();

		CHECK(wait_for_semaphore(client->request_started));
		CHECK_FALSE(client->request_was_on_main_thread);
		CHECK(session->get_project_state()->get_session_state() == AI_NEXT_SESSION_PLANNING);

		client->release_request.post();
		wait_for_next_agent(session, "planning_agent");
		memdelete(session);
	}

	{
		AIAgentNextSession *session = make_isolated_next_session();
		Ref<AINextProjectState> state = session->get_project_state();
		const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
		const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());

		Ref<BlockingNextRuntimeClient> client;
		client.instantiate();
		session->get_agent_for_test("script_agent")->set_runtime_client(client);

		session->run_active_milestone();

		CHECK(wait_for_semaphore(client->request_started));
		CHECK_FALSE(client->request_was_on_main_thread);
		CHECK(String(state->get_task(task_id).get("status", "")) == "in_progress");

		client->release_request.post();
		wait_for_next_agent(session, "script_agent");
		memdelete(session);
	}

	{
		AIAgentNextSession *session = make_isolated_next_session();
		Ref<AINextProjectState> state = session->get_project_state();
		const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
		const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());
		state->mark_task_completed(task_id, "Created player_controller.gd", Array());

		Ref<BlockingNextRuntimeClient> client;
		client.instantiate();
		session->get_agent_for_test("review_agent")->set_runtime_client(client);

		session->review_active_milestone();

		CHECK(wait_for_semaphore(client->request_started));
		CHECK_FALSE(client->request_was_on_main_thread);
		CHECK(state->get_session_state() == AI_NEXT_SESSION_FEEDBACK_PLANNING);

		client->release_request.post();
		wait_for_next_agent(session, "review_agent");
		memdelete(session);
	}

	{
		AIAgentNextSession *session = make_isolated_next_session();
		Ref<AINextProjectState> state = session->get_project_state();
		const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
		const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());
		state->mark_task_completed(task_id, "Created player_controller.gd", Array());

		Ref<BlockingNextRuntimeClient> client;
		client.instantiate();
		session->get_agent_for_test("planning_agent")->set_runtime_client(client);

		session->generate_feedback_tasks("Jump is too floaty.");

		CHECK(wait_for_semaphore(client->request_started));
		CHECK_FALSE(client->request_was_on_main_thread);
		CHECK(state->get_session_state() == AI_NEXT_SESSION_FEEDBACK_PLANNING);

		client->release_request.post();
		wait_for_next_agent(session, "planning_agent");
		memdelete(session);
	}
}

TEST_CASE("[Editor][AI][NEXT] session generates structured plan through NEXT tool") {
	AIAgentNextSession *session = make_isolated_next_session();

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

	AIAgentRuntimeResponse refine_final_response;
	refine_final_response.content = "Plan self-review complete.";
	client->push_response(refine_final_response);

	session->get_agent_for_test("planning_agent")->set_runtime_client(client);
	session->submit_brief("Build a 2D movement prototype.");
	session->generate_plan();
	wait_for_next_agent(session, "planning_agent");
	wait_for_next_agent(session, "planning_agent");

	CHECK(session->get_project_state()->get_brief() == "Build a 2D movement prototype.");
	CHECK(session->get_project_state()->get_milestone_count() == 1);
	CHECK(session->get_project_state()->get_session_state() == AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	CHECK(client->request_count == 3);
	CHECK(client->last_tool_schemas.size() >= 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session saves planning tool writes before final provider response") {
	AIAgentNextSession *session = make_isolated_next_session();

	Ref<BlockingSecondTurnNextRuntimeClient> client;
	client.instantiate();

	AIAgentRuntimeResponse tool_response;
	AIToolCall call;
	call.id = "call_replace_plan";
	call.tool_name = "ai_next.manage_project";
	call.arguments["action"] = "replace_plan";

	Dictionary first_milestone;
	first_milestone["title"] = "Core Movement";
	first_milestone["description"] = "Build movement.";
	first_milestone["tasks"] = Array();

	Dictionary second_milestone;
	second_milestone["title"] = "Combat";
	second_milestone["description"] = "Build combat.";
	second_milestone["tasks"] = Array();

	Array milestones;
	milestones.push_back(first_milestone);
	milestones.push_back(second_milestone);
	call.arguments["milestones"] = milestones;
	tool_response.tool_calls.push_back(call);
	client->push_response(tool_response);

	AIAgentRuntimeResponse final_response;
	final_response.content = "Plan generated.";
	client->push_response(final_response);

	AIAgentRuntimeResponse refine_final_response;
	refine_final_response.content = "Plan self-review complete.";
	client->push_response(refine_final_response);

	session->get_agent_for_test("planning_agent")->set_runtime_client(client);
	session->submit_brief("Build a 2D movement prototype.");
	session->generate_plan();

	CHECK(wait_for_semaphore(client->second_request_started));
	flush_message_queue();

	AINextWorkflowSnapshot snapshot;
	CHECK(session->get_workflow_store_for_test()->load_workflow(session->get_workflow_id(), snapshot));
	CHECK(snapshot.project_state.is_valid());
	if (snapshot.project_state.is_valid()) {
		CHECK(snapshot.project_state->get_milestone_count() == 2);
	}

	client->release_second_request.post();
	wait_for_next_agent(session, "planning_agent");
	wait_for_next_agent(session, "planning_agent");
	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session selects milestones and advances after locking") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String first_milestone = state->create_milestone("Core Movement", "Build movement.");
	const String first_task = state->add_task(first_milestone, "Create player script", "script_agent", Array());
	const String second_milestone = state->create_milestone("Combat", "Build combat.");
	const String second_task = state->add_task(second_milestone, "Create attack scene", "scene_agent", Array());
	CHECK_FALSE(second_task.is_empty());
	state->mark_task_completed(first_task, "Created player_controller.gd", Array());

	CHECK(state->get_active_milestone_id() == first_milestone);
	CHECK(session->select_milestone(second_milestone));
	CHECK(state->get_active_milestone_id() == second_milestone);
	CHECK_FALSE(session->select_milestone("missing_milestone"));
	CHECK(state->get_active_milestone_id() == second_milestone);

	CHECK(session->select_milestone(first_milestone));
	session->accept_and_lock_active_milestone();

	CHECK(String(state->get_milestone(first_milestone).get("status", String())) == "locked");
	CHECK(state->get_active_milestone_id() == second_milestone);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session user plan edits persist and record events") {
	AIAgentNextSession *session = make_isolated_next_session();

	const String first_milestone = session->create_user_milestone("Core Movement", "Build movement.");
	const String second_milestone = session->create_user_milestone("Combat", "Build combat.");
	REQUIRE_FALSE(first_milestone.is_empty());
	REQUIRE_FALSE(second_milestone.is_empty());
	CHECK(session->edit_user_milestone(first_milestone, "Movement Foundation", "Build responsive movement."));

	const String script_task = session->create_user_task(first_milestone, "Create movement script", "script_agent", Array(), "Implement velocity movement.");
	Array dependencies;
	dependencies.push_back(script_task);
	const String scene_task = session->create_user_task(first_milestone, "Assemble movement scene", "scene_agent", dependencies, "Wire the controller into a scene.");
	REQUIRE_FALSE(script_task.is_empty());
	REQUIRE_FALSE(scene_task.is_empty());

	CHECK(session->move_user_task(scene_task, second_milestone, 0));
	CHECK(session->move_user_milestone(second_milestone, 0));
	CHECK(session->set_user_task_dependencies(scene_task, Array()));

	AINextWorkflowSnapshot snapshot;
	CHECK(session->get_workflow_store_for_test()->load_workflow(session->get_workflow_id(), snapshot));
	REQUIRE(snapshot.project_state.is_valid());
	CHECK(snapshot.project_state->get_milestone_count() == 2);
	CHECK(snapshot.project_state->get_task_milestone_id(scene_task) == second_milestone);
	CHECK(String(snapshot.project_state->get_milestone(first_milestone).get("title", String())) == "Movement Foundation");

	Array events = session->get_event_log()->get_events();
	bool saw_plan_edit = false;
	for (int i = 0; i < events.size(); i++) {
		Dictionary event = events[i];
		if (String(event.get("event_type", String())) == "plan_edited") {
			saw_plan_edit = true;
			break;
		}
	}
	CHECK(saw_plan_edit);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session blocks user plan edits while workflow is active") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create movement script", "script_agent", Array());

	Ref<BlockingNextRuntimeClient> client;
	client.instantiate();
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	CHECK(session->run_task(task_id));
	CHECK(wait_for_semaphore(client->request_started));
	CHECK(session->is_workflow_active());

	CHECK(session->create_user_milestone("Combat", "Build combat.").is_empty());
	CHECK_FALSE(session->edit_user_milestone(milestone_id, "Edited", "Edited."));
	CHECK(session->create_user_task(milestone_id, "New task", "script_agent", Array(), "New task.").is_empty());
	CHECK_FALSE(session->delete_user_task(task_id));

	client->release_request.post();
	wait_for_next_agent(session, "script_agent");
	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session blocks user edits on locked milestones") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create movement script", "script_agent", Array());
	CHECK(state->mark_task_completed(task_id, "Done.", Array()));

	session->accept_and_lock_active_milestone();
	CHECK(String(state->get_milestone(milestone_id).get("status", String())) == "locked");

	CHECK(session->can_edit_plan());
	CHECK_FALSE(session->edit_user_milestone(milestone_id, "Edited", "Edited."));
	CHECK(session->create_user_task(milestone_id, "New task", "script_agent", Array(), "New task.").is_empty());
	CHECK_FALSE(session->delete_user_task(task_id));

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session runs ready milestone tasks serially") {
	AIAgentNextSession *session = make_isolated_next_session();
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
	wait_for_next_agent(session, "script_agent");

	Dictionary task = state->get_task(task_id);
	CHECK(String(task["status"]) == "completed");
	CHECK(String(task["result_summary"]) == "Created player_controller.gd");
	CHECK(state->get_session_state() == AI_NEXT_SESSION_WAITING_PLAYTEST);
	CHECK(client->request_count == 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] later milestone tasks receive prior task result context") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String script_task = state->add_task(milestone_id, "Create player script", "script_agent", Array(), "Create the movement script.");
	Array dependencies;
	dependencies.push_back(script_task);
	const String scene_task = state->add_task(milestone_id, "Assemble player scene", "scene_agent", dependencies, "Use the script from the previous task.");

	Ref<NextScriptedRuntimeClient> script_client;
	script_client.instantiate();
	AIAgentRuntimeResponse script_response;
	script_response.content = "Created player_controller.gd with exported speed and jump_force.";
	script_client->push_response(script_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(script_client);

	Ref<NextScriptedRuntimeClient> scene_client;
	scene_client.instantiate();
	AIAgentRuntimeResponse scene_response;
	scene_response.content = "Created player.tscn and attached player_controller.gd.";
	scene_client->push_response(scene_response);
	session->get_agent_for_test("scene_agent")->set_runtime_client(scene_client);

	session->run_active_milestone();
	wait_for_next_agent(session, "script_agent");
	wait_for_next_agent(session, "scene_agent");

	CHECK(String(state->get_task(script_task).get("status", String())) == "completed");
	CHECK(String(state->get_task(scene_task).get("status", String())) == "completed");
	bool saw_prior_task_context = false;
	for (int i = 0; i < scene_client->last_messages.size(); i++) {
		Dictionary message = scene_client->last_messages[i];
		const String content = String(message.get("content", String()));
		if (content.contains("Created player_controller.gd with exported speed and jump_force")) {
			saw_prior_task_context = true;
		}
	}
	CHECK(saw_prior_task_context);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] agent runs receive main agent fixed context") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array(), "Create the movement script.");

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse response;
	response.content = "Created player_controller.gd.";
	client->push_response(response);
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	CHECK(session->run_task(task_id));
	wait_for_next_agent(session, "script_agent");

	bool saw_editor_context = false;
	bool saw_project_tree_context = false;
	bool saw_best_practices_context = false;
	for (int i = 0; i < client->last_messages.size(); i++) {
		Dictionary message = client->last_messages[i];
		const String content = String(message.get("content", String()));
		if (content.contains("## Editor Context")) {
			saw_editor_context = true;
		}
		if (content.contains("## Project Tree")) {
			saw_project_tree_context = true;
		}
		if (content.contains("## Agent Best Practices")) {
			saw_best_practices_context = true;
		}
	}
	CHECK(saw_editor_context);
	CHECK(saw_project_tree_context);
	CHECK(saw_best_practices_context);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] later milestones receive previous milestone result context") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String first_milestone = state->create_milestone("Core Movement", "Build movement.");
	const String script_task = state->add_task(first_milestone, "Create player script", "script_agent", Array(), "Create the movement script.");
	Array output_paths;
	output_paths.push_back("res://scripts/player_controller.gd");
	CHECK(state->mark_task_completed(script_task, "Created player_controller.gd with exported speed and jump_force.", output_paths));

	const String second_milestone = state->create_milestone("Player Scene", "Assemble playable scene.");
	const String scene_task = state->add_task(second_milestone, "Assemble player scene", "scene_agent", Array(), "Reuse the movement script from the earlier milestone.");
	CHECK(session->select_milestone(second_milestone));

	Ref<NextScriptedRuntimeClient> scene_client;
	scene_client.instantiate();
	AIAgentRuntimeResponse scene_response;
	scene_response.content = "Created player.tscn and attached player_controller.gd.";
	scene_client->push_response(scene_response);
	session->get_agent_for_test("scene_agent")->set_runtime_client(scene_client);

	CHECK(session->run_task(scene_task));
	wait_for_next_agent(session, "scene_agent");

	bool saw_prior_milestone_context = false;
	for (int i = 0; i < scene_client->last_messages.size(); i++) {
		Dictionary message = scene_client->last_messages[i];
		const String content = String(message.get("content", String()));
		if (content.contains("Created player_controller.gd with exported speed and jump_force") &&
				content.contains("res://scripts/player_controller.gd")) {
			saw_prior_milestone_context = true;
		}
	}
	CHECK(saw_prior_milestone_context);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session runs one task without running the full milestone") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String first_task = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	const String second_task = state->add_task(milestone_id, "Assemble player scene", "scene_agent", Array());

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Created player_controller.gd";
	client->push_response(final_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	CHECK(session->run_task(first_task));
	CHECK(session->is_workflow_active());
	wait_for_next_agent(session, "script_agent");

	CHECK_FALSE(session->is_workflow_active());
	CHECK(String(state->get_task(first_task).get("status", String())) == "completed");
	CHECK(String(state->get_task(second_task).get("status", String())) == "ready");
	CHECK(client->request_count == 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] task attachments are sent to the assigned agent when running the task") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Visual Review", "Inspect visual references.");
	const String task_id = state->add_task(milestone_id, "Match the reference sprite", "scene_agent", Array(), "Use the attached reference.");
	REQUIRE_FALSE(task_id.is_empty());

	Dictionary patch;
	patch["attachments"] = _make_next_image_attachments("res://art/reference.png");
	String error;
	CHECK(state->update_task(task_id, patch, error));

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Matched the reference.";
	client->push_response(final_response);
	session->get_agent_for_test("scene_agent")->set_runtime_client(client);

	CHECK(session->run_task(task_id));
	wait_for_next_agent(session, "scene_agent");

	CHECK(_messages_have_attachment_path(client->last_messages, "res://art/reference.png"));
	Dictionary stored_task = state->get_task(task_id);
	REQUIRE(stored_task.has("attachments"));
	CHECK(Array(stored_task["attachments"]).size() == 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] task session continues with previous task run messages") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse first_response;
	first_response.content = "Created player_controller.gd";
	client->push_response(first_response);
	AIAgentRuntimeResponse refine_response;
	refine_response.content = "Raised the default speed.";
	client->push_response(refine_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	CHECK(session->run_task(task_id));
	wait_for_next_agent(session, "script_agent");
	CHECK(String(state->get_task(task_id).get("status", String())) == "completed");

	Array first_session_messages = session->get_task_session_messages(task_id);
	REQUIRE(first_session_messages.size() >= 2);
	CHECK(String(Dictionary(first_session_messages[first_session_messages.size() - 1]).get("content", String())) == "Created player_controller.gd");
	CHECK(session->can_continue_task_session(task_id));

	CHECK(session->send_task_session_message(task_id, "Player is too slow; increase the speed parameter."));
	wait_for_next_agent(session, "script_agent");

	CHECK(client->request_count == 2);
	bool saw_previous_result = false;
	bool saw_user_refinement = false;
	for (int i = 0; i < client->last_messages.size(); i++) {
		Dictionary message = client->last_messages[i];
		const String content = String(message.get("content", String()));
		if (content.contains("Created player_controller.gd")) {
			saw_previous_result = true;
		}
		if (content.contains("Player is too slow")) {
			saw_user_refinement = true;
		}
	}
	CHECK(saw_previous_result);
	CHECK(saw_user_refinement);

	Array refined_session_messages = session->get_task_session_messages(task_id);
	REQUIRE(refined_session_messages.size() >= first_session_messages.size() + 2);
	CHECK(String(Dictionary(refined_session_messages[refined_session_messages.size() - 2]).get("content", String())).contains("Player is too slow"));
	CHECK(String(Dictionary(refined_session_messages[refined_session_messages.size() - 1]).get("content", String())) == "Raised the default speed.");
	CHECK(String(state->get_task(task_id).get("result_summary", String())) == "Raised the default speed.");

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] task session attachments are sent only with the follow-up message") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse first_response;
	first_response.content = "Created player_controller.gd";
	client->push_response(first_response);
	AIAgentRuntimeResponse refine_response;
	refine_response.content = "Adjusted the sprite offsets.";
	client->push_response(refine_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	CHECK(session->run_task(task_id));
	wait_for_next_agent(session, "script_agent");

	CHECK(session->send_task_session_message(task_id, "Use this screenshot as the alignment reference.", _make_next_image_attachments("res://screens/alignment.png")));
	wait_for_next_agent(session, "script_agent");

	CHECK(_messages_have_attachment_path(client->last_messages, "res://screens/alignment.png"));

	Array refined_session_messages = session->get_task_session_messages(task_id);
	REQUIRE(refined_session_messages.size() >= 2);
	CHECK(_messages_have_attachment_path(refined_session_messages, "res://screens/alignment.png"));

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] task session does not start tasks that are not ready") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String prerequisite_task = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	Array dependencies;
	dependencies.push_back(prerequisite_task);
	const String blocked_task = state->add_task(milestone_id, "Assemble player scene", "scene_agent", dependencies);
	const String initial_status = String(state->get_task(blocked_task).get("status", String()));

	Ref<NextScriptedRuntimeClient> scene_client;
	scene_client.instantiate();
	AIAgentRuntimeResponse scene_response;
	scene_response.content = "Created player.tscn";
	scene_client->push_response(scene_response);
	session->get_agent_for_test("scene_agent")->set_runtime_client(scene_client);

	CHECK_FALSE(session->can_continue_task_session(blocked_task));
	const bool sent = session->send_task_session_message(blocked_task, "Build this scene now.");
	if (sent) {
		wait_for_next_agent(session, "scene_agent");
	}

	CHECK_FALSE(sent);
	CHECK(scene_client->request_count == 0);
	CHECK(String(state->get_task(blocked_task).get("status", String())) == initial_status);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] feedback attachments are sent to the planning agent") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());
	state->mark_task_completed(task_id, "Created player_controller.gd", Array());

	Ref<NextScriptedRuntimeClient> client;
	client.instantiate();
	AIAgentRuntimeResponse final_response;
	final_response.content = "Feedback tasks generated.";
	client->push_response(final_response);
	session->get_agent_for_test("planning_agent")->set_runtime_client(client);

	session->generate_feedback_tasks("Jump landing looks wrong in this screenshot.", _make_next_image_attachments("res://screens/jump_feedback.png"));
	wait_for_next_agent(session, "planning_agent");

	CHECK(_messages_have_attachment_path(client->last_messages, "res://screens/jump_feedback.png"));
	Dictionary checkpoint = session->get_workflow_checkpoint_for_test();
	REQUIRE(checkpoint.has("feedback_attachments"));
	CHECK(Array(checkpoint["feedback_attachments"]).size() == 1);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session exposes runtime progress messages") {
	AIAgentNextSession *session = make_isolated_next_session();
	Ref<AINextProjectState> state = session->get_project_state();
	const String milestone_id = state->create_milestone("Core Movement", "Build movement.");
	const String task_id = state->add_task(milestone_id, "Create player script", "script_agent", Array());

	Ref<StreamingProgressNextRuntimeClient> client;
	client.instantiate();
	client->wait_for_release = true;
	AIAgentRuntimeResponse final_response;
	final_response.content = "Created player_controller.gd";
	client->push_response(final_response);
	session->get_agent_for_test("script_agent")->set_runtime_client(client);

	CHECK(session->run_task(task_id));
	CHECK(wait_for_semaphore(client->progress_sent));
	flush_message_queue();

	Array progress_messages = session->get_runtime_messages();
	REQUIRE_FALSE(progress_messages.is_empty());
	Dictionary first_message = progress_messages[0];
	CHECK(String(first_message.get("agent_id", String())) == "script_agent");
	CHECK(String(first_message.get("content", String())).contains("Working on NEXT task"));

	client->release_request.post();
	wait_for_next_agent(session, "script_agent");

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session batches ready tasks up to the NEXT scheduling limit") {
	AIAgentNextSession *session = make_isolated_next_session();
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
	wait_for_next_agent(session, "script_agent");
	wait_for_next_agent(session, "scene_agent");
	wait_for_next_agent(session, "shader_agent");

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
	AIAgentNextSession *session = make_isolated_next_session();
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
	wait_for_next_agent(session, "script_agent");
	wait_for_next_agent(session, "scene_agent");

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
	AIAgentNextSession *session = make_isolated_next_session();
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
	wait_for_next_agent(session, "planning_agent");

	CHECK(state->get_task_count(milestone_id) == 2);
	CHECK(state->get_session_state() == AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL);
	CHECK(client->request_count == 2);

	memdelete(session);
}

TEST_CASE("[Editor][AI][NEXT] session records review agent findings") {
	AIAgentNextSession *session = make_isolated_next_session();
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
	wait_for_next_agent(session, "review_agent");

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
	AIAgentNextSession *session = make_isolated_next_session();
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
