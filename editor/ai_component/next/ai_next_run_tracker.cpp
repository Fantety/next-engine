/**************************************************************************/
/*  ai_next_run_tracker.cpp                                               */
/**************************************************************************/

#include "ai_next_run_tracker.h"

#include "core/os/time.h"

const Vector<AINextAgentRunState> &AINextRunTracker::get_runs() const {
	return agent_runs;
}

void AINextRunTracker::set_runs(const Vector<AINextAgentRunState> &p_agent_runs) {
	agent_runs = p_agent_runs;
}

void AINextRunTracker::clear() {
	agent_runs.clear();
}

AINextAgentRunState *AINextRunTracker::find_run(const String &p_run_id) {
	if (p_run_id.is_empty()) {
		return nullptr;
	}
	for (int i = 0; i < agent_runs.size(); i++) {
		if (agent_runs[i].run_id == p_run_id) {
			return &agent_runs.write[i];
		}
	}
	return nullptr;
}

const AINextAgentRunState *AINextRunTracker::find_run(const String &p_run_id) const {
	if (p_run_id.is_empty()) {
		return nullptr;
	}
	for (int i = 0; i < agent_runs.size(); i++) {
		if (agent_runs[i].run_id == p_run_id) {
			return &agent_runs[i];
		}
	}
	return nullptr;
}

AINextAgentRunState *AINextRunTracker::find_latest_task_run(const String &p_task_id) {
	if (p_task_id.is_empty()) {
		return nullptr;
	}
	for (int i = agent_runs.size() - 1; i >= 0; i--) {
		if (agent_runs[i].task_id == p_task_id) {
			return &agent_runs.write[i];
		}
	}
	return nullptr;
}

const AINextAgentRunState *AINextRunTracker::find_latest_task_run(const String &p_task_id) const {
	if (p_task_id.is_empty()) {
		return nullptr;
	}
	for (int i = agent_runs.size() - 1; i >= 0; i--) {
		if (agent_runs[i].task_id == p_task_id) {
			return &agent_runs[i];
		}
	}
	return nullptr;
}

Vector<AIAgentMessage> AINextRunTracker::get_or_create_run_messages(const String &p_agent_run_id, const Vector<AIAgentMessage> &p_default_messages) const {
	const AINextAgentRunState *run_state = find_run(p_agent_run_id);
	if (run_state && !run_state->messages.is_empty()) {
		return run_state->messages;
	}
	return p_default_messages;
}

void AINextRunTracker::upsert_run(const AINextAgentRunState &p_run_state) {
	for (int i = 0; i < agent_runs.size(); i++) {
		if (agent_runs[i].run_id == p_run_state.run_id) {
			agent_runs.write[i] = p_run_state;
			return;
		}
	}
	agent_runs.push_back(p_run_state);
}

void AINextRunTracker::mark_run_started(const String &p_run_id, const String &p_workflow_id, const String &p_agent_id, const String &p_operation, const String &p_milestone_id, const String &p_task_id, const Vector<AIAgentMessage> &p_messages) {
	AINextAgentRunState run_state;
	AINextAgentRunState *existing = find_run(p_run_id);
	if (existing) {
		run_state = *existing;
	}

	const uint64_t now = Time::get_singleton()->get_unix_time_from_system();
	run_state.run_id = p_run_id;
	run_state.workflow_id = p_workflow_id;
	run_state.agent_id = p_agent_id;
	run_state.operation = p_operation;
	run_state.milestone_id = p_milestone_id;
	run_state.task_id = p_task_id;
	run_state.status = "running";
	run_state.messages = p_messages;
	run_state.runtime_base_message_count = p_messages.size();
	if (run_state.created_at == 0) {
		run_state.created_at = now;
	}
	run_state.updated_at = now;
	upsert_run(run_state);
}

void AINextRunTracker::store_progress_message(const String &p_run_id, int p_index, const Dictionary &p_message) {
	if (p_index < 0) {
		return;
	}

	AINextAgentRunState *run_state = find_run(p_run_id);
	if (!run_state) {
		return;
	}

	AIAgentMessage message = AIAgentMessage::from_dict(p_message);
	while (run_state->messages.size() <= p_index) {
		run_state->messages.push_back(AIAgentMessage());
	}
	run_state->messages.write[p_index] = message;
	run_state->runtime_base_message_count = run_state->messages.size();
	run_state->updated_at = Time::get_singleton()->get_unix_time_from_system();
}

void AINextRunTracker::store_result(const String &p_run_id, const AIAgentRuntimeResult &p_result) {
	AINextAgentRunState *run_state = find_run(p_run_id);
	if (!run_state) {
		return;
	}

	run_state->status = p_result.success ? String("completed") : String("failed");
	run_state->messages = p_result.messages;
	run_state->runtime_base_message_count = p_result.messages.size();
	run_state->updated_at = Time::get_singleton()->get_unix_time_from_system();
}

bool AINextRunTracker::mark_run_failed(const String &p_run_id) {
	AINextAgentRunState *run_state = find_run(p_run_id);
	if (!run_state) {
		return false;
	}
	run_state->status = "failed";
	run_state->updated_at = Time::get_singleton()->get_unix_time_from_system();
	return true;
}

bool AINextRunTracker::mark_run_user_terminated(const String &p_run_id) {
	AINextAgentRunState *run_state = find_run(p_run_id);
	if (!run_state) {
		return false;
	}
	run_state->status = "user_terminated";
	run_state->updated_at = Time::get_singleton()->get_unix_time_from_system();
	return true;
}
