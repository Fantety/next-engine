/**************************************************************************/
/*  ai_next_types.h                                                       */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

enum AINextSessionState {
	AI_NEXT_SESSION_IDLE,
	AI_NEXT_SESSION_BRIEFING,
	AI_NEXT_SESSION_PLANNING,
	AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL,
	AI_NEXT_SESSION_EXECUTING,
	AI_NEXT_SESSION_WAITING_PLAYTEST,
	AI_NEXT_SESSION_FEEDBACK_PLANNING,
	AI_NEXT_SESSION_READY_TO_LOCK,
	AI_NEXT_SESSION_FAILED,
};

enum AINextTaskStatus {
	AI_NEXT_TASK_PENDING,
	AI_NEXT_TASK_BLOCKED,
	AI_NEXT_TASK_READY,
	AI_NEXT_TASK_IN_PROGRESS,
	AI_NEXT_TASK_COMPLETED,
	AI_NEXT_TASK_FAILED,
	AI_NEXT_TASK_SKIPPED,
};

enum AINextMilestoneStatus {
	AI_NEXT_MILESTONE_DRAFT,
	AI_NEXT_MILESTONE_READY,
	AI_NEXT_MILESTONE_EXECUTING,
	AI_NEXT_MILESTONE_WAITING_PLAYTEST,
	AI_NEXT_MILESTONE_READY_TO_LOCK,
	AI_NEXT_MILESTONE_LOCKED,
	AI_NEXT_MILESTONE_FAILED,
};

struct AINextTask {
	String id;
	String title;
	String description;
	String assigned_agent_id;
	Vector<String> depends_on;
	Vector<String> asset_refs;
	Vector<String> output_paths;
	AINextTaskStatus status = AI_NEXT_TASK_PENDING;
	String run_id;
	String result_summary;
	String error;
	uint64_t created_at = 0;
	uint64_t updated_at = 0;
};

struct AINextMilestone {
	String id;
	String title;
	String description;
	Vector<AINextTask> tasks;
	AINextMilestoneStatus status = AI_NEXT_MILESTONE_DRAFT;
	int feedback_iteration = 0;
	uint64_t created_at = 0;
	uint64_t updated_at = 0;
};

struct AINextAssetRecord {
	String id;
	String path;
	String source;
	bool protected_from_agent_edits = false;
	String parent_asset_id;
	String baseline_milestone_id;
};

String ai_next_session_state_to_string(AINextSessionState p_state);
AINextSessionState ai_next_session_state_from_string(const String &p_state);
String ai_next_task_status_to_string(AINextTaskStatus p_status);
AINextTaskStatus ai_next_task_status_from_string(const String &p_status);
String ai_next_milestone_status_to_string(AINextMilestoneStatus p_status);
AINextMilestoneStatus ai_next_milestone_status_from_string(const String &p_status);

Array ai_next_string_vector_to_array(const Vector<String> &p_values);
Vector<String> ai_next_array_to_string_vector(const Array &p_values);

Dictionary ai_next_task_to_dict(const AINextTask &p_task);
AINextTask ai_next_task_from_dict(const Dictionary &p_dict);
Dictionary ai_next_milestone_to_dict(const AINextMilestone &p_milestone);
AINextMilestone ai_next_milestone_from_dict(const Dictionary &p_dict);
Dictionary ai_next_asset_record_to_dict(const AINextAssetRecord &p_asset);
AINextAssetRecord ai_next_asset_record_from_dict(const Dictionary &p_dict);
