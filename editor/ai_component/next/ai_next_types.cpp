/**************************************************************************/
/*  ai_next_types.cpp                                                     */
/**************************************************************************/

#include "ai_next_types.h"

#include "core/variant/variant.h"

String ai_next_session_state_to_string(AINextSessionState p_state) {
	switch (p_state) {
		case AI_NEXT_SESSION_BRIEFING:
			return "briefing";
		case AI_NEXT_SESSION_PLANNING:
			return "planning";
		case AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL:
			return "waiting_human_approval";
		case AI_NEXT_SESSION_EXECUTING:
			return "executing";
		case AI_NEXT_SESSION_WAITING_PLAYTEST:
			return "waiting_playtest";
		case AI_NEXT_SESSION_FEEDBACK_PLANNING:
			return "feedback_planning";
		case AI_NEXT_SESSION_READY_TO_LOCK:
			return "ready_to_lock";
		case AI_NEXT_SESSION_FAILED:
			return "failed";
		case AI_NEXT_SESSION_IDLE:
		default:
			return "idle";
	}
}

AINextSessionState ai_next_session_state_from_string(const String &p_state) {
	const String state = p_state.strip_edges().to_lower();
	if (state == "briefing") {
		return AI_NEXT_SESSION_BRIEFING;
	}
	if (state == "planning") {
		return AI_NEXT_SESSION_PLANNING;
	}
	if (state == "waiting_human_approval") {
		return AI_NEXT_SESSION_WAITING_HUMAN_APPROVAL;
	}
	if (state == "executing") {
		return AI_NEXT_SESSION_EXECUTING;
	}
	if (state == "waiting_playtest") {
		return AI_NEXT_SESSION_WAITING_PLAYTEST;
	}
	if (state == "feedback_planning") {
		return AI_NEXT_SESSION_FEEDBACK_PLANNING;
	}
	if (state == "ready_to_lock") {
		return AI_NEXT_SESSION_READY_TO_LOCK;
	}
	if (state == "failed") {
		return AI_NEXT_SESSION_FAILED;
	}
	return AI_NEXT_SESSION_IDLE;
}

String ai_next_task_status_to_string(AINextTaskStatus p_status) {
	switch (p_status) {
		case AI_NEXT_TASK_BLOCKED:
			return "blocked";
		case AI_NEXT_TASK_READY:
			return "ready";
		case AI_NEXT_TASK_IN_PROGRESS:
			return "in_progress";
		case AI_NEXT_TASK_COMPLETED:
			return "completed";
		case AI_NEXT_TASK_FAILED:
			return "failed";
		case AI_NEXT_TASK_SKIPPED:
			return "skipped";
		case AI_NEXT_TASK_PENDING:
		default:
			return "pending";
	}
}

AINextTaskStatus ai_next_task_status_from_string(const String &p_status) {
	const String status = p_status.strip_edges().to_lower();
	if (status == "blocked") {
		return AI_NEXT_TASK_BLOCKED;
	}
	if (status == "ready") {
		return AI_NEXT_TASK_READY;
	}
	if (status == "in_progress") {
		return AI_NEXT_TASK_IN_PROGRESS;
	}
	if (status == "completed") {
		return AI_NEXT_TASK_COMPLETED;
	}
	if (status == "failed") {
		return AI_NEXT_TASK_FAILED;
	}
	if (status == "skipped") {
		return AI_NEXT_TASK_SKIPPED;
	}
	return AI_NEXT_TASK_PENDING;
}

String ai_next_milestone_status_to_string(AINextMilestoneStatus p_status) {
	switch (p_status) {
		case AI_NEXT_MILESTONE_READY:
			return "ready";
		case AI_NEXT_MILESTONE_EXECUTING:
			return "executing";
		case AI_NEXT_MILESTONE_WAITING_PLAYTEST:
			return "waiting_playtest";
		case AI_NEXT_MILESTONE_READY_TO_LOCK:
			return "ready_to_lock";
		case AI_NEXT_MILESTONE_LOCKED:
			return "locked";
		case AI_NEXT_MILESTONE_FAILED:
			return "failed";
		case AI_NEXT_MILESTONE_DRAFT:
		default:
			return "draft";
	}
}

AINextMilestoneStatus ai_next_milestone_status_from_string(const String &p_status) {
	const String status = p_status.strip_edges().to_lower();
	if (status == "ready") {
		return AI_NEXT_MILESTONE_READY;
	}
	if (status == "executing") {
		return AI_NEXT_MILESTONE_EXECUTING;
	}
	if (status == "waiting_playtest") {
		return AI_NEXT_MILESTONE_WAITING_PLAYTEST;
	}
	if (status == "ready_to_lock") {
		return AI_NEXT_MILESTONE_READY_TO_LOCK;
	}
	if (status == "locked") {
		return AI_NEXT_MILESTONE_LOCKED;
	}
	if (status == "failed") {
		return AI_NEXT_MILESTONE_FAILED;
	}
	return AI_NEXT_MILESTONE_DRAFT;
}

Array ai_next_string_vector_to_array(const Vector<String> &p_values) {
	Array result;
	for (const String &value : p_values) {
		result.push_back(value);
	}
	return result;
}

Vector<String> ai_next_array_to_string_vector(const Array &p_values) {
	Vector<String> result;
	for (int i = 0; i < p_values.size(); i++) {
		result.push_back(String(p_values[i]));
	}
	return result;
}

Dictionary ai_next_task_to_dict(const AINextTask &p_task) {
	Dictionary dict;
	dict["id"] = p_task.id;
	dict["title"] = p_task.title;
	dict["description"] = p_task.description;
	dict["assigned_agent_id"] = p_task.assigned_agent_id;
	dict["depends_on"] = ai_next_string_vector_to_array(p_task.depends_on);
	dict["asset_refs"] = ai_next_string_vector_to_array(p_task.asset_refs);
	dict["output_paths"] = ai_next_string_vector_to_array(p_task.output_paths);
	dict["attachments"] = p_task.attachments.duplicate(true);
	dict["status"] = ai_next_task_status_to_string(p_task.status);
	dict["run_id"] = p_task.run_id;
	dict["result_summary"] = p_task.result_summary;
	dict["error"] = p_task.error;
	dict["created_at"] = p_task.created_at;
	dict["updated_at"] = p_task.updated_at;
	return dict;
}

AINextTask ai_next_task_from_dict(const Dictionary &p_dict) {
	AINextTask task;
	task.id = String(p_dict.get("id", String()));
	task.title = String(p_dict.get("title", String()));
	task.description = String(p_dict.get("description", String()));
	task.assigned_agent_id = String(p_dict.get("assigned_agent_id", String()));
	if (p_dict.has("depends_on") && Variant(p_dict["depends_on"]).get_type() == Variant::ARRAY) {
		task.depends_on = ai_next_array_to_string_vector(p_dict["depends_on"]);
	}
	if (p_dict.has("asset_refs") && Variant(p_dict["asset_refs"]).get_type() == Variant::ARRAY) {
		task.asset_refs = ai_next_array_to_string_vector(p_dict["asset_refs"]);
	}
	if (p_dict.has("output_paths") && Variant(p_dict["output_paths"]).get_type() == Variant::ARRAY) {
		task.output_paths = ai_next_array_to_string_vector(p_dict["output_paths"]);
	}
	if (p_dict.has("attachments") && Variant(p_dict["attachments"]).get_type() == Variant::ARRAY) {
		task.attachments = Array(p_dict["attachments"]).duplicate(true);
	}
	task.status = ai_next_task_status_from_string(String(p_dict.get("status", "pending")));
	task.run_id = String(p_dict.get("run_id", String()));
	task.result_summary = String(p_dict.get("result_summary", String()));
	task.error = String(p_dict.get("error", String()));
	task.created_at = (uint64_t)(int64_t)p_dict.get("created_at", 0);
	task.updated_at = (uint64_t)(int64_t)p_dict.get("updated_at", 0);
	return task;
}

Dictionary ai_next_milestone_to_dict(const AINextMilestone &p_milestone) {
	Dictionary dict;
	dict["id"] = p_milestone.id;
	dict["title"] = p_milestone.title;
	dict["description"] = p_milestone.description;
	dict["status"] = ai_next_milestone_status_to_string(p_milestone.status);
	dict["feedback_iteration"] = p_milestone.feedback_iteration;
	dict["created_at"] = p_milestone.created_at;
	dict["updated_at"] = p_milestone.updated_at;

	Array tasks;
	for (const AINextTask &task : p_milestone.tasks) {
		tasks.push_back(ai_next_task_to_dict(task));
	}
	dict["tasks"] = tasks;
	return dict;
}

AINextMilestone ai_next_milestone_from_dict(const Dictionary &p_dict) {
	AINextMilestone milestone;
	milestone.id = String(p_dict.get("id", String()));
	milestone.title = String(p_dict.get("title", String()));
	milestone.description = String(p_dict.get("description", String()));
	milestone.status = ai_next_milestone_status_from_string(String(p_dict.get("status", "draft")));
	milestone.feedback_iteration = (int)p_dict.get("feedback_iteration", 0);
	milestone.created_at = (uint64_t)(int64_t)p_dict.get("created_at", 0);
	milestone.updated_at = (uint64_t)(int64_t)p_dict.get("updated_at", 0);

	if (p_dict.has("tasks") && Variant(p_dict["tasks"]).get_type() == Variant::ARRAY) {
		Array tasks = p_dict["tasks"];
		for (int i = 0; i < tasks.size(); i++) {
			if (Variant(tasks[i]).get_type() == Variant::DICTIONARY) {
				milestone.tasks.push_back(ai_next_task_from_dict(tasks[i]));
			}
		}
	}
	return milestone;
}

Dictionary ai_next_asset_record_to_dict(const AINextAssetRecord &p_asset) {
	Dictionary dict;
	dict["id"] = p_asset.id;
	dict["path"] = p_asset.path;
	dict["source"] = p_asset.source;
	dict["protected_from_agent_edits"] = p_asset.protected_from_agent_edits;
	dict["parent_asset_id"] = p_asset.parent_asset_id;
	dict["baseline_milestone_id"] = p_asset.baseline_milestone_id;
	return dict;
}

AINextAssetRecord ai_next_asset_record_from_dict(const Dictionary &p_dict) {
	AINextAssetRecord asset;
	asset.id = String(p_dict.get("id", String()));
	asset.path = String(p_dict.get("path", String()));
	asset.source = String(p_dict.get("source", String()));
	asset.protected_from_agent_edits = bool(p_dict.get("protected_from_agent_edits", false));
	asset.parent_asset_id = String(p_dict.get("parent_asset_id", String()));
	asset.baseline_milestone_id = String(p_dict.get("baseline_milestone_id", String()));
	return asset;
}
