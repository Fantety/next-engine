/**************************************************************************/
/*  ai_startup_recovery.cpp                                               */
/**************************************************************************/

#include "ai_startup_recovery.h"

#include "core/object/class_db.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"

void AIStartupRecovery::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_session_store", "store"), &AIStartupRecovery::set_session_store);
	ClassDB::bind_method(D_METHOD("get_session_store"), &AIStartupRecovery::get_session_store);
	ClassDB::bind_method(D_METHOD("set_event_store", "store"), &AIStartupRecovery::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AIStartupRecovery::get_event_store);
	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &AIStartupRecovery::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &AIStartupRecovery::get_projector);
	ClassDB::bind_method(D_METHOD("recover"), &AIStartupRecovery::recover);
}

String AIStartupRecovery::_step_key(const Dictionary &p_data) {
	const String assistant_message_id = String(p_data.get("assistant_message_id", p_data.get("assistantMessageID", String()))).strip_edges();
	if (!assistant_message_id.is_empty()) {
		return assistant_message_id;
	}
	return String(p_data.get("request_id", p_data.get("requestID", String()))).strip_edges();
}

String AIStartupRecovery::_tool_key(const Dictionary &p_data) {
	const String assistant_message_id = String(p_data.get("assistant_message_id", p_data.get("assistantMessageID", String()))).strip_edges();
	const String call_id = _tool_call_id(p_data);
	return assistant_message_id + "|" + call_id;
}

String AIStartupRecovery::_tool_call_id(const Dictionary &p_data) {
	return String(p_data.get("call_id", p_data.get("callID", p_data.get("id", String())))).strip_edges();
}

Dictionary AIStartupRecovery::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Dictionary AIStartupRecovery::_make_error_result(const AIError &p_error) {
	Dictionary result;
	result["success"] = false;
	result["error"] = p_error.to_dictionary();
	return result;
}

bool AIStartupRecovery::_append_failed_step(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const String &p_session_id, const Dictionary &p_started, const String &p_reason, Dictionary &r_report, AIError &r_error) {
	Dictionary data = p_started.duplicate(true);
	const String reason = p_reason.strip_edges().is_empty() ? String("Process restarted before activity settled.") : p_reason;
	data["error"] = AIError::make(AI_ERROR_INTERRUPTED, reason).to_dictionary();
	data["recovery_reason"] = reason;

	const String key = _step_key(p_started);
	AIEventRow row;
	String event_error;
	const String idempotency_key = "activity.step_failed:" + p_session_id + ":" + key;
	if (!p_event_store->append_idempotent(p_session_id, AIDomainEventTypes::step_failed(), data, false, idempotency_key, row, event_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
		return false;
	}
	if (p_projector.is_valid()) {
		p_projector->project(row);
	}

	Array interrupted = r_report.get("interrupted_sessions", Array());
	if (!interrupted.has(p_session_id)) {
		interrupted.push_back(p_session_id);
	}
	r_report["interrupted_sessions"] = interrupted;
	r_error = AIError::none();
	return true;
}

bool AIStartupRecovery::_append_failed_tool(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const String &p_session_id, const Dictionary &p_called, const String &p_reason, Dictionary &r_report, AIError &r_error) {
	const String reason = p_reason.strip_edges().is_empty() ? String("Tool execution interrupted by process restart.") : p_reason;
	Dictionary data;
	data["assistant_message_id"] = p_called.get("assistant_message_id", p_called.get("assistantMessageID", String()));
	data["call_id"] = _tool_call_id(p_called);
	data["tool"] = p_called.get("tool", p_called.get("name", String()));
	data["name"] = data["tool"];
	data["input"] = p_called.get("input", Variant());
	data["provider"] = p_called.get("provider", p_called.get("provider_metadata", Dictionary()));
	if (p_called.has("registration_identity")) {
		data["registration_identity"] = p_called["registration_identity"];
	}
	data["content"] = reason;
	data["result"] = reason;
	data["error"] = AIError::make(AI_ERROR_INTERRUPTED, reason).to_dictionary();
	data["recovery_reason"] = reason;

	AIEventRow row;
	String event_error;
	const String idempotency_key = "activity.tool_failed:" + p_session_id + ":" + _tool_key(p_called);
	if (!p_event_store->append_idempotent(p_session_id, AIDomainEventTypes::tool_failed(), data, false, idempotency_key, row, event_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
		return false;
	}
	if (p_projector.is_valid()) {
		p_projector->project(row);
	}

	Array failed_tools = r_report.get("failed_tool_calls", Array());
	const String call_id = _tool_call_id(p_called);
	if (!call_id.is_empty() && !failed_tools.has(call_id)) {
		failed_tools.push_back(call_id);
	}
	r_report["failed_tool_calls"] = failed_tools;

	Array interrupted = r_report.get("interrupted_sessions", Array());
	if (!interrupted.has(p_session_id)) {
		interrupted.push_back(p_session_id);
	}
	r_report["interrupted_sessions"] = interrupted;
	r_error = AIError::none();
	return true;
}

void AIStartupRecovery::set_session_store(const Ref<AISessionStore> &p_store) {
	session_store = p_store;
}

Ref<AISessionStore> AIStartupRecovery::get_session_store() const {
	return session_store;
}

void AIStartupRecovery::set_event_store(const Ref<AIEventStore> &p_store) {
	event_store = p_store;
}

Ref<AIEventStore> AIStartupRecovery::get_event_store() const {
	return event_store;
}

void AIStartupRecovery::set_projector(const Ref<AISessionProjector> &p_projector) {
	projector = p_projector;
}

Ref<AISessionProjector> AIStartupRecovery::get_projector() const {
	return projector;
}

bool AIStartupRecovery::cleanup_open_activity_struct(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const String &p_session_id, const String &p_reason, bool p_fail_steps, bool p_fail_tools, bool p_retain_permission_pending_tools, Dictionary &r_report, AIError &r_error) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session id is required for recovery.");
		return false;
	}
	if (p_event_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "StartupRecovery has no EventStore.");
		return false;
	}

	HashMap<String, Dictionary> open_steps;
	HashMap<String, Dictionary> open_tools;
	HashSet<String> pending_permission_tools;
	const Vector<AIEventRow> rows = p_event_store->list(session_id, 0, false);
	for (int i = 0; i < rows.size(); i++) {
		const AIEventRow &row = rows[i];
		if (row.type == AIDomainEventTypes::step_started()) {
			const String key = _step_key(row.data);
			if (!key.is_empty()) {
				open_steps[key] = row.data.duplicate(true);
			}
		} else if (row.type == AIDomainEventTypes::step_ended() || row.type == AIDomainEventTypes::step_failed()) {
			const String key = _step_key(row.data);
			if (!key.is_empty()) {
				open_steps.erase(key);
			}
		} else if (row.type == AIDomainEventTypes::tool_called()) {
			const String key = _tool_key(row.data);
			if (!key.ends_with("|")) {
				open_tools[key] = row.data.duplicate(true);
			}
		} else if (row.type == AIDomainEventTypes::tool_success() || row.type == AIDomainEventTypes::tool_failed()) {
			const String key = _tool_key(row.data);
			if (!key.ends_with("|")) {
				open_tools.erase(key);
			}
		} else if (row.type == AIDomainEventTypes::permission_asked()) {
			const Dictionary source = _dictionary_from_variant(row.data.get("source", Dictionary()));
			const String key = _tool_key(source);
			if (!key.ends_with("|") && String(row.data.get("status", "pending")) == "pending") {
				pending_permission_tools.insert(key);
			}
		} else if (row.type == AIDomainEventTypes::permission_replied()) {
			const Dictionary source = _dictionary_from_variant(row.data.get("source", Dictionary()));
			const String key = _tool_key(source);
			if (!key.ends_with("|")) {
				pending_permission_tools.erase(key);
			}
		}
	}

	if (p_fail_steps) {
		for (const KeyValue<String, Dictionary> &kv : open_steps) {
			if (!_append_failed_step(p_event_store, p_projector, session_id, kv.value, p_reason, r_report, r_error)) {
				return false;
			}
		}
	}

	if (p_fail_tools) {
		for (const KeyValue<String, Dictionary> &kv : open_tools) {
			if (p_retain_permission_pending_tools && pending_permission_tools.has(kv.key)) {
				Array notes = r_report.get("notes", Array());
				notes.push_back("Retained permission-pending tool call: " + kv.key);
				r_report["notes"] = notes;
				continue;
			}
			if (!_append_failed_tool(p_event_store, p_projector, session_id, kv.value, p_reason, r_report, r_error)) {
				return false;
			}
		}
	}

	r_error = AIError::none();
	return true;
}

bool AIStartupRecovery::recover_struct(Dictionary &r_report, AIError &r_error) {
	if (session_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "StartupRecovery has no SessionStore.");
		return false;
	}
	if (event_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "StartupRecovery has no EventStore.");
		return false;
	}

	r_report = Dictionary();
	r_report["interrupted_sessions"] = Array();
	r_report["failed_tool_calls"] = Array();
	r_report["notes"] = Array();

	const Array sessions = session_store->list_sessions();
	for (int i = 0; i < sessions.size(); i++) {
		if (sessions[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary session = sessions[i];
		const String session_id = String(session.get("id", session.get("session_id", session.get("sessionID", String())))).strip_edges();
		if (session_id.is_empty()) {
			continue;
		}
		if (!cleanup_open_activity_struct(event_store, projector, session_id, "Process restarted before activity settled.", true, true, true, r_report, r_error)) {
			return false;
		}
	}

	r_report["success"] = true;
	r_error = AIError::none();
	return true;
}

Dictionary AIStartupRecovery::recover() {
	Dictionary report;
	AIError error;
	if (!recover_struct(report, error)) {
		return _make_error_result(error);
	}
	return report;
}
