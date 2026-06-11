/**************************************************************************/
/*  ai_permission_service.cpp                                              */
/**************************************************************************/

#include "ai_permission_service.h"

#include "editor/agent_v1/core/base/ai_id.h"
#include "editor/agent_v1/domain/events/ai_event_types.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

Dictionary AIPermissionDecision::to_dictionary() const {
	Dictionary result;
	result["allowed"] = allowed;
	result["pending"] = pending;
	result["denied"] = denied;
	result["request_id"] = request_id;
	result["session_id"] = session_id;
	result["action"] = action;
	result["resource"] = resource;
	result["effect"] = effect;
	result["reply"] = reply;
	result["reason"] = reason;
	result["source"] = source;
	result["error"] = error.to_dictionary();
	result["success"] = allowed && !error.is_error();
	if (pending) {
		result["status"] = "pending";
	} else if (denied) {
		result["status"] = "rejected";
	} else if (allowed) {
		result["status"] = "allowed";
	} else {
		result["status"] = "unknown";
	}
	return result;
}

Dictionary AIPermissionService::PendingRequest::to_dictionary() const {
	Dictionary result;
	result["request_id"] = request_id;
	result["session_id"] = session_id;
	result["action"] = action;
	result["resource"] = resource;
	result["effect"] = effect;
	result["status"] = status;
	result["reason"] = reason;
	result["source"] = source;
	return result;
}

void AIPermissionService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_event_store", "event_store"), &AIPermissionService::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AIPermissionService::get_event_store);
	ClassDB::bind_method(D_METHOD("set_rules", "rules"), &AIPermissionService::set_rules);
	ClassDB::bind_method(D_METHOD("get_rules"), &AIPermissionService::get_rules);
	ClassDB::bind_method(D_METHOD("assert_permission", "input"), &AIPermissionService::assert_permission);
	ClassDB::bind_method(D_METHOD("reply", "input"), &AIPermissionService::reply);
	ClassDB::bind_method(D_METHOD("get_pending_requests"), &AIPermissionService::get_pending_requests);
	ClassDB::bind_method(D_METHOD("get_decisions_for_source", "session_id", "source"), &AIPermissionService::get_decisions_for_source);
	ClassDB::bind_method(D_METHOD("clear"), &AIPermissionService::clear);

	ADD_SIGNAL(MethodInfo("permission_asked", PropertyInfo(Variant::DICTIONARY, "request")));
	ADD_SIGNAL(MethodInfo("permission_replied", PropertyInfo(Variant::DICTIONARY, "reply")));
}

String AIPermissionService::_normalize_effect(const String &p_effect) {
	const String effect = p_effect.strip_edges().to_lower();
	if (effect == "allow" || effect == "ask" || effect == "deny") {
		return effect;
	}
	return "ask";
}

String AIPermissionService::_normalize_reply(const String &p_reply) {
	const String reply = p_reply.strip_edges().to_lower();
	if (reply == "once" || reply == "always" || reply == "reject") {
		return reply;
	}
	return "reject";
}

String AIPermissionService::_request_key(const String &p_session_id, const String &p_action, const String &p_resource, const Dictionary &p_source) {
	const String assistant_message_id = p_source.get("assistant_message_id", p_source.get("assistantMessageID", String()));
	const String call_id = p_source.get("call_id", p_source.get("callID", String()));
	return p_session_id + "|" + assistant_message_id + "|" + call_id + "|" + p_action + "|" + p_resource;
}

String AIPermissionService::_approval_key(const String &p_action, const String &p_resource) {
	return p_action + "|" + p_resource;
}

bool AIPermissionService::_source_matches(const Dictionary &p_filter, const Dictionary &p_source) {
	const String filter_assistant = String(p_filter.get("assistant_message_id", p_filter.get("assistantMessageID", String()))).strip_edges();
	if (!filter_assistant.is_empty() && filter_assistant != String(p_source.get("assistant_message_id", p_source.get("assistantMessageID", String()))).strip_edges()) {
		return false;
	}

	const String filter_call = String(p_filter.get("call_id", p_filter.get("callID", String()))).strip_edges();
	if (!filter_call.is_empty() && filter_call != String(p_source.get("call_id", p_source.get("callID", String()))).strip_edges()) {
		return false;
	}

	const String filter_tool = String(p_filter.get("tool", p_filter.get("name", String()))).strip_edges();
	if (!filter_tool.is_empty() && filter_tool != String(p_source.get("tool", p_source.get("name", String()))).strip_edges()) {
		return false;
	}

	return true;
}

bool AIPermissionService::_is_resource_match(const String &p_pattern, const String &p_resource) {
	const String pattern = p_pattern.strip_edges();
	if (pattern.is_empty() || pattern == "*") {
		return true;
	}
	if (pattern.ends_with("*")) {
		return p_resource.begins_with(pattern.substr(0, pattern.length() - 1));
	}
	return pattern == p_resource;
}

bool AIPermissionService::_rule_matches(const Dictionary &p_rule, const String &p_action, const String &p_resource) {
	const String action = String(p_rule.get("action", "*")).strip_edges().to_lower();
	if (!_is_resource_match(action, p_action)) {
		return false;
	}
	return _is_resource_match(String(p_rule.get("resource", "*")), p_resource);
}

String AIPermissionService::_default_effect_for_action(const String &p_action) const {
	const String action = p_action.strip_edges().to_lower();
	if (action == "file.read") {
		return "allow";
	}
	if (action == "file.write" || action == "shell.run") {
		return "ask";
	}
	return "ask";
}

String AIPermissionService::_evaluate_effect_locked(const String &p_action, const String &p_resource, String &r_reason, const String &p_default_effect) const {
	bool matched = false;
	String effect;
	String reason;
	for (int i = 0; i < rules.size(); i++) {
		if (rules[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary rule = rules[i];
		if (!_rule_matches(rule, p_action, p_resource)) {
			continue;
		}
		matched = true;
		reason = rule.get("reason", String());
		effect = _normalize_effect(rule.get("effect", "ask"));
	}

	if (matched) {
		r_reason = reason;
		return effect;
	}

	r_reason = String();
	if (!p_default_effect.strip_edges().is_empty()) {
		return _normalize_effect(p_default_effect);
	}
	return _default_effect_for_action(p_action);
}

void AIPermissionService::_record_decision_locked(const String &p_request_key, const AIPermissionDecision &p_decision) {
	if (p_request_key.strip_edges().is_empty()) {
		return;
	}
	decision_audits[p_request_key] = p_decision.to_dictionary();
}

void AIPermissionService::_record_pending_request_locked(const String &p_request_key, const PendingRequest &p_request) {
	AIPermissionDecision decision;
	decision.session_id = p_request.session_id;
	decision.action = p_request.action;
	decision.resource = p_request.resource;
	decision.effect = p_request.effect;
	decision.request_id = p_request.request_id;
	decision.reason = p_request.reason;
	decision.source = p_request.source.duplicate(true);
	if (p_request.status == "once" || p_request.status == "always") {
		decision.allowed = true;
		decision.reply = p_request.status;
	} else if (p_request.status == "reject") {
		decision.denied = true;
		decision.reply = "reject";
	} else {
		decision.pending = true;
	}
	_record_decision_locked(p_request_key, decision);
}

bool AIPermissionService::_append_permission_event(const String &p_session_id, const String &p_type, const Dictionary &p_data, AIError &r_error) {
	if (event_store.is_null()) {
		return true;
	}

	AIEventRow row;
	String event_error;
	const String aggregate_id = p_session_id.strip_edges().is_empty() ? String("permission") : p_session_id.strip_edges();
	const String request_id = p_data.get("request_id", AIId::make("perm"));
	const String idempotency_key = p_type + ":" + aggregate_id + ":" + request_id + ":" + String(p_data.get("reply", p_data.get("status", String())));
	if (!event_store->append_idempotent(aggregate_id, p_type, p_data, false, idempotency_key, row, event_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
		return false;
	}
	return true;
}

void AIPermissionService::set_event_store(const Ref<AIEventStore> &p_event_store) {
	event_store = p_event_store;
}

Ref<AIEventStore> AIPermissionService::get_event_store() const {
	return event_store;
}

void AIPermissionService::set_rules(const Array &p_rules) {
	MutexLock lock(mutex);
	rules = p_rules.duplicate(true);
}

Array AIPermissionService::get_rules() const {
	MutexLock lock(mutex);
	return rules.duplicate(true);
}

bool AIPermissionService::assert_permission_struct(const Dictionary &p_input, AIPermissionDecision &r_decision, AIError &r_error) {
	const String session_id = String(p_input.get("session_id", p_input.get("sessionID", String()))).strip_edges();
	const String action = String(p_input.get("action", String())).strip_edges().to_lower();
	const String resource = String(p_input.get("resource", "*")).strip_edges();
	Dictionary source;
	if (p_input.get("source", Variant()).get_type() == Variant::DICTIONARY) {
		source = Dictionary(p_input["source"]).duplicate(true);
	}

	r_decision = AIPermissionDecision();
	r_decision.session_id = session_id;
	r_decision.action = action;
	r_decision.resource = resource;
	r_decision.source = source;

	if (action.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Permission action is required.");
		r_decision.denied = true;
		r_decision.error = r_error;
		return false;
	}

	const String request_key = _request_key(session_id, action, resource, source);
	PendingRequest new_request;
	bool should_emit_asked = false;
	Dictionary asked_data;

	{
		MutexLock lock(mutex);
		const String approval_key = _approval_key(action, resource);
		if (saved_approvals.has(approval_key) && saved_approvals[approval_key]) {
			r_decision.allowed = true;
			r_decision.effect = "allow";
			r_decision.reply = "always";
			_record_decision_locked(request_key, r_decision);
			return true;
		}

		String reason;
		const String default_effect = String(p_input.get("default_effect", p_input.get("defaultEffect", String()))).strip_edges();
		const String effect = _normalize_effect(p_input.get("effect", _evaluate_effect_locked(action, resource, reason, default_effect)));
		if (reason.is_empty()) {
			reason = p_input.get("reason", String());
		}
		r_decision.effect = effect;
		r_decision.reason = reason;

		if (effect == "allow") {
			r_decision.allowed = true;
			_record_decision_locked(request_key, r_decision);
			return true;
		}
		if (effect == "deny") {
			r_error = AIError::make(AI_ERROR_PERMISSION, reason.is_empty() ? String("Permission denied.") : reason);
			r_decision.denied = true;
			r_decision.error = r_error;
			_record_decision_locked(request_key, r_decision);
			return false;
		}

		if (pending_requests.has(request_key)) {
			PendingRequest &existing = pending_requests[request_key];
			r_decision.request_id = existing.request_id;
			r_decision.reason = existing.reason;
			r_decision.effect = existing.effect;
			if (existing.status == "once" || existing.status == "always") {
				r_decision.allowed = true;
				r_decision.reply = existing.status;
				_record_decision_locked(request_key, r_decision);
				return true;
			}
			if (existing.status == "reject") {
				r_error = AIError::make(AI_ERROR_PERMISSION, existing.reason.is_empty() ? String("Permission rejected.") : existing.reason);
				r_decision.denied = true;
				r_decision.reply = "reject";
				r_decision.error = r_error;
				_record_decision_locked(request_key, r_decision);
				return false;
			}

			r_error = AIError::make(AI_ERROR_PERMISSION, "Permission pending.", existing.to_dictionary());
			r_decision.pending = true;
			r_decision.error = r_error;
			_record_decision_locked(request_key, r_decision);
			return false;
		}

		new_request.request_id = AIId::make("perm");
		new_request.session_id = session_id;
		new_request.action = action;
		new_request.resource = resource;
		new_request.effect = "ask";
		new_request.status = "pending";
		new_request.reason = reason;
		new_request.source = source.duplicate(true);
		pending_requests[request_key] = new_request;

		r_decision.request_id = new_request.request_id;
		r_decision.pending = true;
		r_decision.reason = reason;
		r_error = AIError::make(AI_ERROR_PERMISSION, "Permission pending.", new_request.to_dictionary());
		r_decision.error = r_error;
		_record_pending_request_locked(request_key, new_request);

		asked_data = new_request.to_dictionary();
		asked_data["request_key"] = request_key;
		should_emit_asked = true;
	}

	if (should_emit_asked) {
		AIError event_error;
		if (!_append_permission_event(session_id, AIDomainEventTypes::permission_asked(), asked_data, event_error)) {
			r_error = event_error;
			r_decision.error = event_error;
			return false;
		}
		call_deferred("emit_signal", SNAME("permission_asked"), asked_data);
	}
	return false;
}

bool AIPermissionService::reply_struct(const Dictionary &p_input, AIPermissionDecision &r_decision, AIError &r_error) {
	const String request_id = String(p_input.get("request_id", p_input.get("requestID", String()))).strip_edges();
	const String reply = _normalize_reply(p_input.get("reply", p_input.get("decision", String())));
	const String reason = p_input.get("reason", String());
	if (request_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Permission request_id is required.");
		r_decision.error = r_error;
		r_decision.denied = true;
		return false;
	}

	PendingRequest request;
	Dictionary reply_data;
	bool found = false;
	String matched_request_key;
	{
		MutexLock lock(mutex);
		for (KeyValue<String, PendingRequest> &kv : pending_requests) {
			if (kv.value.request_id != request_id) {
				continue;
			}

			kv.value.status = reply;
			if (!reason.is_empty()) {
				kv.value.reason = reason;
			}
			request = kv.value;
			found = true;
			matched_request_key = kv.key;
			if (reply == "always") {
				saved_approvals[_approval_key(request.action, request.resource)] = true;
				for (KeyValue<String, PendingRequest> &pending_kv : pending_requests) {
					if (pending_kv.value.action == request.action && pending_kv.value.resource == request.resource && pending_kv.value.status == "pending") {
						pending_kv.value.status = "always";
						if (!reason.is_empty()) {
							pending_kv.value.reason = reason;
						}
						_record_pending_request_locked(pending_kv.key, pending_kv.value);
					}
				}
			} else if (reply == "reject") {
				for (KeyValue<String, PendingRequest> &pending_kv : pending_requests) {
					if (pending_kv.value.session_id == request.session_id && pending_kv.value.status == "pending") {
						pending_kv.value.status = "reject";
						if (!reason.is_empty()) {
							pending_kv.value.reason = reason;
						}
						_record_pending_request_locked(pending_kv.key, pending_kv.value);
					}
				}
			}
			_record_pending_request_locked(matched_request_key, kv.value);
			break;
		}
	}

	if (!found) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Permission request not found.");
		r_decision.error = r_error;
		r_decision.denied = true;
		return false;
	}

	reply_data = request.to_dictionary();
	reply_data["reply"] = reply;
	if (!reason.is_empty()) {
		reply_data["reason"] = reason;
	}

	AIError event_error;
	if (!_append_permission_event(request.session_id, AIDomainEventTypes::permission_replied(), reply_data, event_error)) {
		r_error = event_error;
		r_decision.error = event_error;
		return false;
	}
	call_deferred("emit_signal", SNAME("permission_replied"), reply_data);

	r_decision.session_id = request.session_id;
	r_decision.action = request.action;
	r_decision.resource = request.resource;
	r_decision.request_id = request.request_id;
	r_decision.source = request.source;
	r_decision.reply = reply;
	r_decision.reason = request.reason;
	r_decision.allowed = reply == "once" || reply == "always";
	r_decision.denied = reply == "reject";
	if (r_decision.denied) {
		r_error = AIError::make(AI_ERROR_PERMISSION, request.reason.is_empty() ? String("Permission rejected.") : request.reason, reply_data);
		r_decision.error = r_error;
		return false;
	}
	return true;
}

Dictionary AIPermissionService::assert_permission(const Dictionary &p_input) {
	AIPermissionDecision decision;
	AIError error;
	assert_permission_struct(p_input, decision, error);
	return decision.to_dictionary();
}

Dictionary AIPermissionService::reply(const Dictionary &p_input) {
	AIPermissionDecision decision;
	AIError error;
	reply_struct(p_input, decision, error);
	return decision.to_dictionary();
}

Array AIPermissionService::get_pending_requests() const {
	MutexLock lock(mutex);
	Array result;
	for (const KeyValue<String, PendingRequest> &kv : pending_requests) {
		result.push_back(kv.value.to_dictionary());
	}
	return result;
}

Array AIPermissionService::get_decisions_for_source_struct(const String &p_session_id, const Dictionary &p_source) const {
	MutexLock lock(mutex);
	Array result;
	const String session_id = p_session_id.strip_edges();
	for (const KeyValue<String, Dictionary> &kv : decision_audits) {
		const Dictionary decision = kv.value;
		if (!session_id.is_empty() && session_id != String(decision.get("session_id", decision.get("sessionID", String()))).strip_edges()) {
			continue;
		}
		if (decision.get("source", Variant()).get_type() == Variant::DICTIONARY && _source_matches(p_source, decision["source"])) {
			result.push_back(decision.duplicate(true));
		}
	}
	return result;
}

Array AIPermissionService::get_decisions_for_source(const String &p_session_id, const Dictionary &p_source) const {
	return get_decisions_for_source_struct(p_session_id, p_source);
}

void AIPermissionService::clear() {
	MutexLock lock(mutex);
	pending_requests.clear();
	saved_approvals.clear();
	decision_audits.clear();
}
