/**************************************************************************/
/*  ai_session_types.cpp                                                  */
/**************************************************************************/

#include "ai_session_types.h"

#include "core/variant/variant.h"

static Dictionary _ai_session_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return p_value;
	}
	return Dictionary();
}

static Array _ai_session_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return p_value;
	}
	return Array();
}

String ai_session_input_status_to_string(AISessionInputStatus p_status) {
	switch (p_status) {
		case AI_SESSION_INPUT_STATUS_ADMITTED:
			return "admitted";
		case AI_SESSION_INPUT_STATUS_PROMOTED:
			return "promoted";
		case AI_SESSION_INPUT_STATUS_CANCELED:
			return "canceled";
	}
	return "admitted";
}

AISessionInputStatus ai_session_input_status_from_string(const String &p_status) {
	const String status = p_status.strip_edges().to_lower();
	if (status == "promoted") {
		return AI_SESSION_INPUT_STATUS_PROMOTED;
	}
	if (status == "canceled") {
		return AI_SESSION_INPUT_STATUS_CANCELED;
	}
	return AI_SESSION_INPUT_STATUS_ADMITTED;
}

Dictionary AISessionRow::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["agent_id"] = agent_id;
	result["location"] = location.to_dictionary();
	result["directory"] = location.directory;
	result["workspace_id"] = location.workspace_id;
	result["title"] = title;
	result["metadata"] = metadata;
	result["created_at"] = created_at;
	result["updated_at"] = updated_at;
	return result;
}

AISessionRow AISessionRow::from_dictionary(const Dictionary &p_dict) {
	AISessionRow result;
	result.id = p_dict.get("id", String());
	result.agent_id = p_dict.get("agent_id", p_dict.get("agentID", String()));
	if (p_dict.get("location", Variant()).get_type() == Variant::DICTIONARY) {
		result.location = AILocationRef::from_dictionary(p_dict["location"]);
	} else {
		result.location.directory = p_dict.get("directory", String());
		result.location.workspace_id = p_dict.get("workspace_id", p_dict.get("workspaceID", String()));
	}
	result.title = p_dict.get("title", String());
	result.metadata = _ai_session_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	result.created_at = uint64_t(p_dict.get("created_at", p_dict.get("createdAt", 0)));
	result.updated_at = uint64_t(p_dict.get("updated_at", p_dict.get("updatedAt", 0)));
	return result;
}

bool AISessionInputRecord::is_admitted() const {
	return status == AI_SESSION_INPUT_STATUS_ADMITTED;
}

bool AISessionInputRecord::is_promoted() const {
	return status == AI_SESSION_INPUT_STATUS_PROMOTED;
}

Dictionary AISessionInputRecord::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["session_id"] = session_id;
	result["message_id"] = message_id;
	result["role"] = role;
	result["parts"] = parts;
	result["prompt"] = prompt.to_dictionary();
	result["delivery"] = ai_session_input_delivery_to_string(delivery);
	result["status"] = ai_session_input_status_to_string(status);
	result["resume"] = resume;
	result["created_at"] = created_at;
	if (promoted_at > 0) {
		result["promoted_at"] = promoted_at;
	}
	result["idempotency_key"] = idempotency_key;
	result["cancel_reason"] = cancel_reason;
	result["admitted_seq"] = admitted_seq;
	result["promoted_seq"] = promoted_seq;
	result["metadata"] = metadata;
	return result;
}

AISessionInputRecord AISessionInputRecord::from_dictionary(const Dictionary &p_dict) {
	AISessionInputRecord result;
	result.id = p_dict.get("id", String());
	result.session_id = p_dict.get("session_id", p_dict.get("sessionID", String()));
	result.message_id = p_dict.get("message_id", p_dict.get("messageID", String()));
	result.role = p_dict.get("role", "user");
	result.parts = _ai_session_array_from_variant(p_dict.get("parts", Array())).duplicate(true);
	result.prompt = AIPrompt::from_dictionary(_ai_session_dictionary_from_variant(p_dict.get("prompt", Dictionary())));
	result.delivery = ai_session_input_delivery_from_string(p_dict.get("delivery", "steer"));
	result.status = ai_session_input_status_from_string(p_dict.get("status", "admitted"));
	result.resume = bool(p_dict.get("resume", true));
	result.created_at = uint64_t(p_dict.get("created_at", p_dict.get("createdAt", 0)));
	result.promoted_at = uint64_t(p_dict.get("promoted_at", p_dict.get("promotedAt", 0)));
	result.idempotency_key = p_dict.get("idempotency_key", p_dict.get("idempotencyKey", String()));
	result.cancel_reason = p_dict.get("cancel_reason", p_dict.get("cancelReason", String()));
	result.admitted_seq = int64_t(p_dict.get("admitted_seq", p_dict.get("admittedSeq", 0)));
	result.promoted_seq = int64_t(p_dict.get("promoted_seq", p_dict.get("promotedSeq", 0)));
	result.metadata = _ai_session_dictionary_from_variant(p_dict.get("metadata", Dictionary())).duplicate(true);
	return result;
}

Dictionary AISessionInputAdmission::to_dictionary() const {
	Dictionary result;
	result["input"] = input.to_dictionary();
	result["created"] = created;
	result["retry"] = retry;
	result["synthesized"] = synthesized;
	return result;
}

Dictionary AISessionPromptResult::to_dictionary() const {
	Dictionary result;
	result["session"] = session.to_dictionary();
	result["prompt"] = prompt.to_dictionary();
	result["wake_scheduled"] = wake_scheduled;
	result["input_created"] = input_created;
	result["retry"] = retry;
	result["synthesized"] = synthesized;
	return result;
}

Dictionary AISessionExecutionState::to_dictionary() const {
	Dictionary result;
	result["session_id"] = session_id;
	result["active"] = active;
	result["wake_pending"] = wake_pending;
	result["active_run_id"] = active_run_id;
	result["interrupted"] = interrupted;
	result["interrupt_reason"] = interrupt_reason;
	result["wake_seq"] = wake_seq;
	result["drain_start_count"] = drain_start_count;
	return result;
}

AISessionExecutionState AISessionExecutionState::from_dictionary(const Dictionary &p_dict) {
	AISessionExecutionState result;
	result.session_id = p_dict.get("session_id", p_dict.get("sessionID", String()));
	result.active = bool(p_dict.get("active", false));
	result.wake_pending = bool(p_dict.get("wake_pending", p_dict.get("wakePending", false)));
	result.active_run_id = p_dict.get("active_run_id", p_dict.get("activeRunID", String()));
	result.interrupted = bool(p_dict.get("interrupted", false));
	result.interrupt_reason = p_dict.get("interrupt_reason", p_dict.get("interruptReason", String()));
	result.wake_seq = int64_t(p_dict.get("wake_seq", p_dict.get("wakeSeq", 0)));
	result.drain_start_count = int64_t(p_dict.get("drain_start_count", p_dict.get("drainStartCount", 0)));
	return result;
}
