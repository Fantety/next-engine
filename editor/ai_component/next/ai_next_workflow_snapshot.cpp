/**************************************************************************/
/*  ai_next_workflow_snapshot.cpp                                         */
/**************************************************************************/

#include "ai_next_workflow_snapshot.h"

#include "core/variant/variant.h"
#include "editor/ai_component/storage/ai_conversation_serializer.h"

bool AINextWorkflowCheckpoint::is_resumable() const {
	if (operation.is_empty() || operation == "none") {
		return false;
	}
	return status == "running" || status == "user_terminated";
}

Dictionary AINextWorkflowCheckpoint::to_dict() const {
	Dictionary dict;
	dict["status"] = status;
	dict["operation"] = operation;
	dict["workflow_run_id"] = workflow_run_id;
	dict["agent_run_id"] = agent_run_id;
	dict["agent_id"] = agent_id;
	dict["milestone_id"] = milestone_id;
	dict["task_id"] = task_id;
	dict["single_task_run"] = single_task_run;
	dict["feedback_text"] = feedback_text;
	dict["feedback_previous_task_count"] = feedback_previous_task_count;
	dict["selected_task_id"] = selected_task_id;
	dict["active_task_batch"] = active_task_batch.duplicate(true);
	dict["active_task_batch_index"] = active_task_batch_index;
	return dict;
}

void AINextWorkflowCheckpoint::load_from_dict(const Dictionary &p_dict) {
	status = String(p_dict.get("status", "idle"));
	operation = String(p_dict.get("operation", "none"));
	workflow_run_id = String(p_dict.get("workflow_run_id", String()));
	agent_run_id = String(p_dict.get("agent_run_id", String()));
	agent_id = String(p_dict.get("agent_id", String()));
	milestone_id = String(p_dict.get("milestone_id", String()));
	task_id = String(p_dict.get("task_id", String()));
	single_task_run = bool(p_dict.get("single_task_run", false));
	feedback_text = String(p_dict.get("feedback_text", String()));
	feedback_previous_task_count = (int)p_dict.get("feedback_previous_task_count", 0);
	selected_task_id = String(p_dict.get("selected_task_id", String()));
	active_task_batch.clear();
	if (p_dict.has("active_task_batch") && Variant(p_dict["active_task_batch"]).get_type() == Variant::ARRAY) {
		active_task_batch = Array(p_dict["active_task_batch"]).duplicate(true);
	}
	active_task_batch_index = (int)p_dict.get("active_task_batch_index", 0);
}

Dictionary AINextAgentRunState::to_dict() const {
	Dictionary dict;
	dict["run_id"] = run_id;
	dict["workflow_id"] = workflow_id;
	dict["agent_id"] = agent_id;
	dict["operation"] = operation;
	dict["milestone_id"] = milestone_id;
	dict["task_id"] = task_id;
	dict["status"] = status;
	dict["messages"] = AIConversationSerializer::messages_to_array(messages);
	dict["runtime_base_message_count"] = runtime_base_message_count;
	dict["created_at"] = created_at;
	dict["updated_at"] = updated_at;
	return dict;
}

void AINextAgentRunState::load_from_dict(const Dictionary &p_dict) {
	run_id = String(p_dict.get("run_id", String()));
	workflow_id = String(p_dict.get("workflow_id", String()));
	agent_id = String(p_dict.get("agent_id", String()));
	operation = String(p_dict.get("operation", String()));
	milestone_id = String(p_dict.get("milestone_id", String()));
	task_id = String(p_dict.get("task_id", String()));
	status = String(p_dict.get("status", "idle"));
	messages.clear();
	if (p_dict.has("messages") && Variant(p_dict["messages"]).get_type() == Variant::ARRAY) {
		messages = AIConversationSerializer::messages_from_array(p_dict["messages"]);
	}
	runtime_base_message_count = (int)p_dict.get("runtime_base_message_count", 0);
	created_at = (uint64_t)(int64_t)p_dict.get("created_at", 0);
	updated_at = (uint64_t)(int64_t)p_dict.get("updated_at", 0);
}

Dictionary AINextWorkflowMetadata::to_dict() const {
	Dictionary dict;
	dict["id"] = id;
	dict["title"] = title;
	dict["created_at"] = created_at;
	dict["updated_at"] = updated_at;
	dict["session_state"] = session_state;
	dict["active_milestone_id"] = active_milestone_id;
	dict["milestone_count"] = milestone_count;
	dict["task_count"] = task_count;
	dict["has_resumable_checkpoint"] = has_resumable_checkpoint;
	return dict;
}

Dictionary AINextWorkflowSnapshot::to_dict() const {
	Dictionary root;
	root["schema_version"] = schema_version;
	root["id"] = id;
	root["title"] = title;
	root["created_at"] = created_at;
	root["updated_at"] = updated_at;
	root["project_state"] = project_state.is_valid() ? project_state->to_dict() : Dictionary();
	root["event_log"] = event_log.duplicate(true);
	root["checkpoint"] = checkpoint.to_dict();

	Array runs;
	for (int i = 0; i < agent_runs.size(); i++) {
		runs.push_back(agent_runs[i].to_dict());
	}
	root["agent_runs"] = runs;
	return root;
}

bool AINextWorkflowSnapshot::load_from_dict(const Dictionary &p_dict) {
	schema_version = (int)p_dict.get("schema_version", SCHEMA_VERSION);
	id = String(p_dict.get("id", String()));
	title = String(p_dict.get("title", "New NEXT Workflow"));
	created_at = (uint64_t)(int64_t)p_dict.get("created_at", 0);
	updated_at = (uint64_t)(int64_t)p_dict.get("updated_at", 0);

	project_state.instantiate();
	if (p_dict.has("project_state") && Variant(p_dict["project_state"]).get_type() == Variant::DICTIONARY) {
		project_state->load_from_dict(p_dict["project_state"]);
	}

	event_log.clear();
	if (p_dict.has("event_log") && Variant(p_dict["event_log"]).get_type() == Variant::ARRAY) {
		event_log = Array(p_dict["event_log"]).duplicate(true);
	}

	checkpoint = AINextWorkflowCheckpoint();
	if (p_dict.has("checkpoint") && Variant(p_dict["checkpoint"]).get_type() == Variant::DICTIONARY) {
		checkpoint.load_from_dict(p_dict["checkpoint"]);
	}

	agent_runs.clear();
	if (p_dict.has("agent_runs") && Variant(p_dict["agent_runs"]).get_type() == Variant::ARRAY) {
		Array runs = p_dict["agent_runs"];
		for (int i = 0; i < runs.size(); i++) {
			if (Variant(runs[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			AINextAgentRunState run;
			run.load_from_dict(runs[i]);
			agent_runs.push_back(run);
		}
	}

	return !id.is_empty();
}

AINextWorkflowMetadata AINextWorkflowSnapshot::to_metadata() const {
	AINextWorkflowMetadata metadata;
	metadata.id = id;
	metadata.title = title;
	metadata.created_at = created_at;
	metadata.updated_at = updated_at;
	metadata.has_resumable_checkpoint = checkpoint.is_resumable();

	if (project_state.is_valid()) {
		metadata.session_state = project_state->get_session_state_name();
		metadata.active_milestone_id = project_state->get_active_milestone_id();
		metadata.milestone_count = project_state->get_milestone_count();

		Array milestones = project_state->get_milestones_as_array();
		for (int i = 0; i < milestones.size(); i++) {
			if (Variant(milestones[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary milestone = milestones[i];
			Array tasks = milestone.get("tasks", Array());
			metadata.task_count += tasks.size();
		}
	}

	return metadata;
}
