/**************************************************************************/
/*  ai_session_service.cpp                                                */
/**************************************************************************/

#include "ai_session_service.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AISessionService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_session_store", "session_store"), &AISessionService::set_session_store);
	ClassDB::bind_method(D_METHOD("get_session_store"), &AISessionService::get_session_store);
	ClassDB::bind_method(D_METHOD("set_input_store", "input_store"), &AISessionService::set_input_store);
	ClassDB::bind_method(D_METHOD("get_input_store"), &AISessionService::get_input_store);
	ClassDB::bind_method(D_METHOD("set_event_store", "event_store"), &AISessionService::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AISessionService::get_event_store);
	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &AISessionService::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &AISessionService::get_projector);
	ClassDB::bind_method(D_METHOD("set_execution", "execution"), &AISessionService::set_execution);
	ClassDB::bind_method(D_METHOD("get_execution"), &AISessionService::get_execution);
	ClassDB::bind_method(D_METHOD("set_prompt_promoter", "prompt_promoter"), &AISessionService::set_prompt_promoter);
	ClassDB::bind_method(D_METHOD("get_prompt_promoter"), &AISessionService::get_prompt_promoter);
	ClassDB::bind_method(D_METHOD("set_empty_runner", "empty_runner"), &AISessionService::set_empty_runner);
	ClassDB::bind_method(D_METHOD("get_empty_runner"), &AISessionService::get_empty_runner);
	ClassDB::bind_method(D_METHOD("create", "input"), &AISessionService::create);
	ClassDB::bind_method(D_METHOD("prompt", "input"), &AISessionService::prompt);
	ClassDB::bind_method(D_METHOD("interrupt", "input"), &AISessionService::interrupt);
	ClassDB::bind_method(D_METHOD("promote_eligible", "session_id", "mode"), &AISessionService::promote_eligible, DEFVAL("new-activity"));
}

AISessionService::AISessionService() {
	_ensure_defaults();
	_wire_dependencies();
}

Array AISessionService::_parts_from_input(const Dictionary &p_input) {
	if (p_input.get("parts", Variant()).get_type() == Variant::ARRAY) {
		return Array(p_input["parts"]).duplicate(true);
	}

	Array parts;
	if (p_input.has("text")) {
		Dictionary text_part;
		text_part["type"] = "text";
		text_part["text"] = p_input.get("text", String());
		parts.push_back(text_part);
	}
	return parts;
}

AIPrompt AISessionService::_prompt_from_input(const Dictionary &p_input, const Array &p_parts) {
	if (p_input.get("prompt", Variant()).get_type() == Variant::DICTIONARY) {
		return AIPrompt::from_dictionary(p_input["prompt"]);
	}

	AIPrompt prompt;
	if (p_input.has("text")) {
		prompt.text = p_input.get("text", String());
	}

	for (int i = 0; i < p_parts.size(); i++) {
		const Variant part_value = p_parts[i];
		if (part_value.get_type() == Variant::STRING) {
			const String text = String(part_value);
			prompt.text += prompt.text.is_empty() ? text : "\n" + text;
			continue;
		}
		if (part_value.get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary part = part_value;
		const String type = String(part.get("type", "text")).strip_edges().to_lower();
		if (type == "text" || type == "input_text") {
			const String text = part.get("text", part.get("content", String()));
			prompt.text += prompt.text.is_empty() ? text : "\n" + text;
		} else if (type == "file" || type == "attachment") {
			prompt.files.push_back(AIFileAttachment::from_dictionary(part));
		} else if (type == "agent") {
			prompt.agents.push_back(AIAgentReference::from_dictionary(part));
		} else if (type == "reference") {
			prompt.references.push_back(AIPromptReference::from_dictionary(part));
		}
	}
	return prompt;
}

bool AISessionService::_has_location_input(const Dictionary &p_input) {
	if (p_input.get("location", Variant()).get_type() == Variant::DICTIONARY) {
		const AILocationRef location = AILocationRef::from_dictionary(p_input["location"]);
		return !location.directory.strip_edges().is_empty();
	}
	return !String(p_input.get("directory", String())).strip_edges().is_empty();
}

Dictionary AISessionService::_make_error_result(const AIError &p_error) {
	Dictionary result;
	result["success"] = false;
	result["error"] = p_error.to_dictionary();
	return result;
}

void AISessionService::_ensure_defaults() {
	if (session_store.is_null()) {
		session_store.instantiate();
	}
	if (input_store.is_null()) {
		input_store.instantiate();
	}
	if (event_store.is_null()) {
		event_store.instantiate();
	}
	if (projector.is_null()) {
		projector.instantiate();
	}
	if (execution.is_null()) {
		execution.instantiate();
	}
	if (prompt_promoter.is_null()) {
		prompt_promoter.instantiate();
	}
	if (empty_runner.is_null()) {
		empty_runner.instantiate();
	}
}

void AISessionService::_wire_dependencies() {
	if (input_store.is_valid()) {
		input_store->set_event_store(event_store);
		input_store->set_projector(projector);
	}
	if (prompt_promoter.is_valid()) {
		prompt_promoter->set_input_store(input_store);
	}
	if (empty_runner.is_valid()) {
		empty_runner->set_prompt_promoter(prompt_promoter);
	}
	if (execution.is_valid()) {
		execution->set_runner(empty_runner);
	}
}

bool AISessionService::_resolve_session_for_prompt(const Dictionary &p_input, AISessionRow &r_session, bool &r_created, AIError &r_error) {
	_ensure_defaults();
	const String session_id = String(p_input.get("session_id", p_input.get("sessionID", String()))).strip_edges();
	if (!session_id.is_empty()) {
		if (!session_store->get_session_struct(session_id, r_session)) {
			Dictionary details;
			details["session_id"] = session_id;
			r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Session not found.", details);
			return false;
		}
		r_created = false;
		return true;
	}

	if (!_has_location_input(p_input)) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Location is required when prompting without an existing session.");
		return false;
	}

	String error;
	if (!session_store->create_or_reuse(p_input, r_session, r_created, error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, error);
		return false;
	}
	return true;
}

bool AISessionService::_append_admitted_event(AISessionInputRecord &r_input, AIError &r_error) {
	if (event_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionService has no EventStore.");
		return false;
	}
	if (r_input.admitted_seq > 0 || r_input.is_promoted()) {
		return true;
	}

	Dictionary data;
	data["id"] = r_input.id;
	data["session_id"] = r_input.session_id;
	data["message_id"] = r_input.message_id;
	data["prompt"] = r_input.prompt.to_dictionary();
	data["parts"] = r_input.parts;
	data["delivery"] = ai_session_input_delivery_to_string(r_input.delivery);
	data["resume"] = r_input.resume;
	data["idempotency_key"] = r_input.idempotency_key;
	data["time_created"] = r_input.created_at;

	AIEventRow row;
	String event_error;
	if (!event_store->append(r_input.session_id, AIDomainEventTypes::prompt_admitted(), data, false, row, event_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
		return false;
	}

	AISessionInputRecord updated;
	if (!input_store->set_admitted_seq(r_input.session_id, r_input.id, row.seq, updated, r_error)) {
		return false;
	}
	r_input = updated;

	if (projector.is_valid()) {
		projector->project(row);
	}
	return true;
}

void AISessionService::set_session_store(const Ref<AISessionStore> &p_session_store) {
	session_store = p_session_store;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AISessionStore> AISessionService::get_session_store() const {
	return session_store;
}

void AISessionService::set_input_store(const Ref<AISessionInputStore> &p_input_store) {
	input_store = p_input_store;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AISessionInputStore> AISessionService::get_input_store() const {
	return input_store;
}

void AISessionService::set_event_store(const Ref<AIEventStore> &p_event_store) {
	event_store = p_event_store;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIEventStore> AISessionService::get_event_store() const {
	return event_store;
}

void AISessionService::set_projector(const Ref<AISessionProjector> &p_projector) {
	projector = p_projector;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AISessionProjector> AISessionService::get_projector() const {
	return projector;
}

void AISessionService::set_execution(const Ref<AISessionExecution> &p_execution) {
	execution = p_execution;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AISessionExecution> AISessionService::get_execution() const {
	return execution;
}

void AISessionService::set_prompt_promoter(const Ref<AIPromptPromoter> &p_prompt_promoter) {
	prompt_promoter = p_prompt_promoter;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIPromptPromoter> AISessionService::get_prompt_promoter() const {
	return prompt_promoter;
}

void AISessionService::set_empty_runner(const Ref<AIEmptySessionRunner> &p_empty_runner) {
	empty_runner = p_empty_runner;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIEmptySessionRunner> AISessionService::get_empty_runner() const {
	return empty_runner;
}

Dictionary AISessionService::create(const Dictionary &p_input) {
	_ensure_defaults();
	AISessionRow session;
	bool created = false;
	String error;
	if (!session_store->create_or_reuse(p_input, session, created, error)) {
		return _make_error_result(AIError::make(AI_ERROR_INTERNAL, error));
	}

	Dictionary result = session.to_dictionary();
	result["success"] = true;
	result["created"] = created;
	return result;
}

Dictionary AISessionService::prompt(const Dictionary &p_input) {
	_ensure_defaults();
	_wire_dependencies();

	AISessionRow session;
	bool session_created = false;
	AIError error;
	if (!_resolve_session_for_prompt(p_input, session, session_created, error)) {
		return _make_error_result(error);
	}

	const Array parts = _parts_from_input(p_input);
	AISessionInputRecord request;
	request.id = p_input.get("prompt_id", p_input.get("promptID", String()));
	request.session_id = session.id;
	request.message_id = p_input.get("message_id", p_input.get("messageID", String()));
	request.parts = parts;
	request.prompt = _prompt_from_input(p_input, parts);
	request.delivery = ai_session_input_delivery_from_string(p_input.get("delivery", "steer"));
	request.resume = bool(p_input.get("resume", true));
	request.idempotency_key = p_input.get("idempotency_key", p_input.get("idempotencyKey", String()));
	if (p_input.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
		request.metadata = Dictionary(p_input["metadata"]).duplicate(true);
	}

	AISessionInputAdmission admission;
	if (!input_store->admit(request, admission, error)) {
		return _make_error_result(error);
	}

	AISessionInputRecord prompt_input = admission.input;
	if (!_append_admitted_event(prompt_input, error)) {
		return _make_error_result(error);
	}

	bool wake_scheduled = false;
	if (prompt_input.resume && execution.is_valid()) {
		AISessionExecutionState state;
		execution->wake_struct(session.id, prompt_input.admitted_seq, state);
		wake_scheduled = true;
	}

	AISessionPromptResult prompt_result;
	prompt_result.session = session;
	prompt_result.prompt = prompt_input;
	prompt_result.wake_scheduled = wake_scheduled;
	prompt_result.input_created = admission.created;
	prompt_result.retry = admission.retry;
	prompt_result.synthesized = admission.synthesized;

	Dictionary result = prompt_result.to_dictionary();
	result["success"] = true;
	result["session_created"] = session_created;
	return result;
}

Dictionary AISessionService::interrupt(const Dictionary &p_input) {
	_ensure_defaults();

	const String session_id = String(p_input.get("session_id", p_input.get("sessionID", String()))).strip_edges();
	if (session_id.is_empty()) {
		return _make_error_result(AIError::make(AI_ERROR_VALIDATION, "Session id is required to interrupt."));
	}

	const String reason = p_input.get("reason", String());
	if (event_store.is_valid()) {
		Dictionary data;
		data["session_id"] = session_id;
		data["reason"] = reason;
		AIEventRow row;
		String event_error;
		if (!event_store->append(session_id, AIDomainEventTypes::interrupt_requested(), data, false, row, event_error)) {
			return _make_error_result(AIError::make(AI_ERROR_INTERNAL, event_error));
		}
	}

	Dictionary result;
	result["success"] = true;
	result["interrupted"] = execution.is_valid() ? execution->interrupt(session_id, reason) : Dictionary();
	return result;
}

Dictionary AISessionService::promote_eligible(const String &p_session_id, const String &p_mode) {
	_ensure_defaults();
	_wire_dependencies();
	if (prompt_promoter.is_null()) {
		return _make_error_result(AIError::make(AI_ERROR_UNAVAILABLE, "SessionService has no PromptPromoter."));
	}
	return prompt_promoter->promote_eligible(p_session_id, p_mode);
}
