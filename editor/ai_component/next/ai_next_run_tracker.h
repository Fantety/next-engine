/**************************************************************************/
/*  ai_next_run_tracker.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/next/ai_next_workflow_snapshot.h"

class AINextRunTracker {
	Vector<AINextAgentRunState> agent_runs;

public:
	const Vector<AINextAgentRunState> &get_runs() const;
	void set_runs(const Vector<AINextAgentRunState> &p_agent_runs);
	void clear();

	AINextAgentRunState *find_run(const String &p_run_id);
	const AINextAgentRunState *find_run(const String &p_run_id) const;
	AINextAgentRunState *find_latest_task_run(const String &p_task_id);
	const AINextAgentRunState *find_latest_task_run(const String &p_task_id) const;
	Vector<AIAgentMessage> get_or_create_run_messages(const String &p_agent_run_id, const Vector<AIAgentMessage> &p_default_messages) const;

	void upsert_run(const AINextAgentRunState &p_run_state);
	void mark_run_started(const String &p_run_id, const String &p_workflow_id, const String &p_agent_id, const String &p_operation, const String &p_milestone_id, const String &p_task_id, const Vector<AIAgentMessage> &p_messages);
	void store_progress_message(const String &p_run_id, int p_index, const Dictionary &p_message);
	void store_result(const String &p_run_id, const AIAgentRuntimeResult &p_result);
	bool mark_run_failed(const String &p_run_id);
	bool mark_run_user_terminated(const String &p_run_id);
};
