/**************************************************************************/
/*  ai_next_workflow_snapshot.h                                           */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/agent/ai_agent_message.h"
#include "editor/ai_component/next/ai_next_project_state.h"

struct AINextWorkflowCheckpoint {
	String status = "idle";
	String operation = "none";
	String workflow_run_id;
	String agent_run_id;
	String agent_id;
	String milestone_id;
	String task_id;
	bool single_task_run = false;
	String feedback_text;
	int feedback_previous_task_count = 0;
	String selected_task_id;
	Array active_task_batch;
	int active_task_batch_index = 0;

	bool is_resumable() const;
	Dictionary to_dict() const;
	void load_from_dict(const Dictionary &p_dict);
};

struct AINextAgentRunState {
	String run_id;
	String workflow_id;
	String agent_id;
	String operation;
	String milestone_id;
	String task_id;
	String status = "idle";
	Vector<AIAgentMessage> messages;
	int runtime_base_message_count = 0;
	uint64_t created_at = 0;
	uint64_t updated_at = 0;

	Dictionary to_dict() const;
	void load_from_dict(const Dictionary &p_dict);
};

struct AINextWorkflowMetadata {
	String id;
	String title;
	uint64_t created_at = 0;
	uint64_t updated_at = 0;
	String session_state;
	String active_milestone_id;
	int milestone_count = 0;
	int task_count = 0;
	bool has_resumable_checkpoint = false;

	Dictionary to_dict() const;
};

struct AINextWorkflowSnapshot {
	static const int SCHEMA_VERSION = 1;

	int schema_version = SCHEMA_VERSION;
	String id;
	String title = "New NEXT Workflow";
	uint64_t created_at = 0;
	uint64_t updated_at = 0;
	Ref<AINextProjectState> project_state;
	Array event_log;
	AINextWorkflowCheckpoint checkpoint;
	Vector<AINextAgentRunState> agent_runs;

	Dictionary to_dict() const;
	bool load_from_dict(const Dictionary &p_dict);
	AINextWorkflowMetadata to_metadata() const;
};
