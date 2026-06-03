/**************************************************************************/
/*  test_ai_next_run_tracker.cpp                                          */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/next/ai_next_run_tracker.h"

TEST_FORCE_LINK(test_ai_next_run_tracker);

namespace TestAINextRunTracker {

static AIAgentMessage _make_message(AIAgentRole p_role, const String &p_content) {
	AIAgentMessage message;
	message.role = p_role;
	message.content = p_content;
	return message;
}

TEST_CASE("[Editor][AI][NEXT] run tracker stores run lifecycle messages") {
	AINextRunTracker tracker;

	Vector<AIAgentMessage> initial_messages;
	initial_messages.push_back(_make_message(AI_AGENT_ROLE_USER, "Start task."));
	tracker.mark_run_started("run_1", "workflow_1", "script_agent", "run_task", "milestone_1", "task_1", initial_messages);

	const AINextAgentRunState *started = tracker.find_run("run_1");
	REQUIRE(started != nullptr);
	CHECK(started->workflow_id == "workflow_1");
	CHECK(started->agent_id == "script_agent");
	CHECK(started->operation == "run_task");
	CHECK(started->milestone_id == "milestone_1");
	CHECK(started->task_id == "task_1");
	CHECK(started->status == "running");
	CHECK(started->messages.size() == 1);

	Dictionary assistant_progress;
	assistant_progress["role"] = "assistant";
	assistant_progress["content"] = "Working.";
	tracker.store_progress_message("run_1", 1, assistant_progress);

	const AINextAgentRunState *with_progress = tracker.find_run("run_1");
	REQUIRE(with_progress != nullptr);
	REQUIRE(with_progress->messages.size() == 2);
	CHECK(with_progress->messages[1].role == AI_AGENT_ROLE_ASSISTANT);
	CHECK(with_progress->messages[1].content == "Working.");
	CHECK(with_progress->runtime_base_message_count == 2);

	AIAgentRuntimeResult result;
	result.success = true;
	result.messages = with_progress->messages;
	result.messages.push_back(_make_message(AI_AGENT_ROLE_ASSISTANT, "Done."));
	tracker.store_result("run_1", result);

	const AINextAgentRunState *completed = tracker.find_run("run_1");
	REQUIRE(completed != nullptr);
	CHECK(completed->status == "completed");
	CHECK(completed->messages.size() == 3);
	CHECK(completed->runtime_base_message_count == 3);
}

TEST_CASE("[Editor][AI][NEXT] run tracker finds latest run for a task") {
	AINextRunTracker tracker;
	Vector<AIAgentMessage> messages;
	messages.push_back(_make_message(AI_AGENT_ROLE_USER, "Run task."));

	tracker.mark_run_started("run_1", "workflow_1", "script_agent", "run_task", "milestone_1", "task_1", messages);
	tracker.mark_run_started("run_2", "workflow_1", "script_agent", "run_task", "milestone_1", "task_1", messages);

	const AINextAgentRunState *latest = tracker.find_latest_task_run("task_1");
	REQUIRE(latest != nullptr);
	CHECK(latest->run_id == "run_2");
	CHECK(tracker.get_or_create_run_messages("run_2", Vector<AIAgentMessage>()).size() == 1);
}

} // namespace TestAINextRunTracker
