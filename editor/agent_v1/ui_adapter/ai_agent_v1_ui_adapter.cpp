/**************************************************************************/
/*  ai_agent_v1_ui_adapter.cpp                                            */
/**************************************************************************/

#include "ai_agent_v1_ui_adapter.h"

#include "editor/agent_v1/domain/events/ai_event_types.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

void AIAgentV1UIAdapter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_session_service", "service"), &AIAgentV1UIAdapter::set_session_service);
	ClassDB::bind_method(D_METHOD("get_session_service"), &AIAgentV1UIAdapter::get_session_service);
	ClassDB::bind_method(D_METHOD("create_session", "options"), &AIAgentV1UIAdapter::create_session, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("list_sessions"), &AIAgentV1UIAdapter::list_sessions);
	ClassDB::bind_method(D_METHOD("set_active_session", "session_id"), &AIAgentV1UIAdapter::set_active_session);
	ClassDB::bind_method(D_METHOD("get_active_session_id"), &AIAgentV1UIAdapter::get_active_session_id);
	ClassDB::bind_method(D_METHOD("get_active_session"), &AIAgentV1UIAdapter::get_active_session);
	ClassDB::bind_method(D_METHOD("get_messages", "session_id"), &AIAgentV1UIAdapter::get_messages, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("send_message", "text", "model_id", "agent_id", "attachments", "resume"), &AIAgentV1UIAdapter::send_message, DEFVAL(String()), DEFVAL(String()), DEFVAL(Array()), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("cancel_active_run", "reason"), &AIAgentV1UIAdapter::cancel_active_run, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_run_state", "session_id"), &AIAgentV1UIAdapter::get_run_state, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_pending_permissions"), &AIAgentV1UIAdapter::get_pending_permissions);
	ClassDB::bind_method(D_METHOD("refresh_pending_permissions"), &AIAgentV1UIAdapter::refresh_pending_permissions);
	ClassDB::bind_method(D_METHOD("reply_permission", "request_id", "allowed", "reason", "options"), &AIAgentV1UIAdapter::reply_permission, DEFVAL(String()), DEFVAL(Dictionary()));

	ADD_SIGNAL(MethodInfo("sessions_changed", PropertyInfo(Variant::ARRAY, "sessions")));
	ADD_SIGNAL(MethodInfo("active_session_changed", PropertyInfo(Variant::DICTIONARY, "session")));
	ADD_SIGNAL(MethodInfo("messages_changed", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::ARRAY, "messages")));
	ADD_SIGNAL(MethodInfo("run_state_changed", PropertyInfo(Variant::DICTIONARY, "state")));
	ADD_SIGNAL(MethodInfo("permission_requested", PropertyInfo(Variant::DICTIONARY, "request")));
	ADD_SIGNAL(MethodInfo("permission_resolved", PropertyInfo(Variant::DICTIONARY, "reply")));
	ADD_SIGNAL(MethodInfo("error_reported", PropertyInfo(Variant::DICTIONARY, "error")));
}

AIAgentV1UIAdapter::AIAgentV1UIAdapter() {
	_ensure_defaults();
}

void AIAgentV1UIAdapter::_ensure_defaults() {
	if (session_service.is_null()) {
		session_service.instantiate();
	}
	_wire_service_signals();
}

void AIAgentV1UIAdapter::_wire_service_signals() {
	if (session_service.is_null()) {
		return;
	}

	Ref<AIPermissionService> permission_service = session_service->get_permission_service();
	if (permission_service.is_valid()) {
		const Callable asked = callable_mp(this, &AIAgentV1UIAdapter::_permission_asked);
		if (!permission_service->is_connected(SNAME("permission_asked"), asked)) {
			permission_service->connect(SNAME("permission_asked"), asked);
		}
		const Callable replied = callable_mp(this, &AIAgentV1UIAdapter::_permission_replied);
		if (!permission_service->is_connected(SNAME("permission_replied"), replied)) {
			permission_service->connect(SNAME("permission_replied"), replied);
		}
	}

	Ref<AIEventStore> event_store = session_service->get_event_store();
	if (event_store.is_valid()) {
		const Callable event_appended = callable_mp(this, &AIAgentV1UIAdapter::_event_appended);
		if (!event_store->is_connected(SNAME("event_appended"), event_appended)) {
			event_store->connect(SNAME("event_appended"), event_appended);
		}
	}

	Ref<AISessionExecution> execution = session_service->get_execution();
	if (execution.is_valid()) {
		const Callable drain_requested = callable_mp(this, &AIAgentV1UIAdapter::_drain_requested);
		if (!execution->is_connected(SNAME("drain_requested"), drain_requested)) {
			execution->connect(SNAME("drain_requested"), drain_requested);
		}
		const Callable drain_settled = callable_mp(this, &AIAgentV1UIAdapter::_drain_settled);
		if (!execution->is_connected(SNAME("drain_settled"), drain_settled)) {
			execution->connect(SNAME("drain_settled"), drain_settled);
		}
		const Callable interrupt_requested = callable_mp(this, &AIAgentV1UIAdapter::_interrupt_requested);
		if (!execution->is_connected(SNAME("interrupt_requested"), interrupt_requested)) {
			execution->connect(SNAME("interrupt_requested"), interrupt_requested);
		}
	}
}

Dictionary AIAgentV1UIAdapter::_make_error_result(const String &p_message, const Dictionary &p_details) {
	Dictionary error;
	error["message"] = p_message;
	error["details"] = p_details.duplicate(true);

	Dictionary result;
	result["success"] = false;
	result["error"] = error;
	return result;
}

String AIAgentV1UIAdapter::_message_role(const AISessionMessage &p_message) {
	if (p_message.type == AI_SESSION_MESSAGE_COMPACTION) {
		return "context";
	}
	return ai_session_message_type_to_string(p_message.type);
}

Array AIAgentV1UIAdapter::_files_to_ui_attachments(const Vector<AIFileAttachment> &p_files) {
	Array attachments;
	for (int i = 0; i < p_files.size(); i++) {
		attachments.push_back(p_files[i].to_dictionary());
	}
	return attachments;
}

Array AIAgentV1UIAdapter::_references_to_ui_array(const Vector<AIPromptReference> &p_references) {
	Array references;
	for (int i = 0; i < p_references.size(); i++) {
		references.push_back(p_references[i].to_dictionary());
	}
	return references;
}

Dictionary AIAgentV1UIAdapter::_input_to_ui_message(const AISessionInput &p_input) {
	Dictionary metadata;
	metadata["seq"] = p_input.admitted_seq;
	metadata["agent_v1_type"] = "user";
	metadata["input_status"] = p_input.is_promoted() ? String("promoted") : String("admitted");
	metadata["delivery"] = ai_session_input_delivery_to_string(p_input.delivery);
	if (!p_input.prompt.files.is_empty()) {
		metadata["attachments"] = _files_to_ui_attachments(p_input.prompt.files);
	}
	if (!p_input.prompt.references.is_empty()) {
		metadata["references"] = _references_to_ui_array(p_input.prompt.references);
	}

	Dictionary message;
	message["id"] = p_input.id;
	message["session_id"] = p_input.session_id;
	message["role"] = "user";
	message["content"] = p_input.prompt.text;
	message["metadata"] = metadata;
	message["created_at"] = p_input.time_created;
	return message;
}

Dictionary AIAgentV1UIAdapter::_base_message_metadata(const AISessionMessage &p_message) {
	Dictionary metadata = p_message.metadata.duplicate(true);
	metadata["seq"] = p_message.seq;
	metadata["agent_v1_type"] = ai_session_message_type_to_string(p_message.type);
	if (!p_message.files.is_empty()) {
		metadata["attachments"] = _files_to_ui_attachments(p_message.files);
	}
	if (!p_message.references.is_empty()) {
		metadata["references"] = _references_to_ui_array(p_message.references);
	}
	return metadata;
}

String AIAgentV1UIAdapter::_variant_to_display_text(const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) {
		return String();
	}
	if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME || p_value.get_type() == Variant::NODE_PATH) {
		return String(p_value);
	}
	return p_value.stringify();
}

Dictionary AIAgentV1UIAdapter::_tool_content_to_ui_message(const AISessionMessage &p_message, const AIAssistantContent &p_content) {
	const AIToolState &state = p_content.tool_state;
	Dictionary metadata = state.metadata.duplicate(true);
	metadata["seq"] = p_message.seq;
	metadata["agent_v1_type"] = ai_session_message_type_to_string(p_message.type);
	metadata["tool_name"] = p_content.name;
	metadata["call_id"] = p_content.id;
	metadata["status"] = ai_tool_status_to_string(state.status);
	metadata["input"] = state.input;
	metadata["progress"] = state.progress;
	metadata["output"] = state.output;
	metadata["output_paths"] = state.output_paths;
	metadata["result"] = state.result;
	metadata["provider"] = state.provider.duplicate(true);
	metadata["provider_metadata"] = p_content.provider_metadata.duplicate(true);
	metadata["content_metadata"] = p_content.metadata.duplicate(true);

	String content;
	if (state.error.is_error()) {
		content = state.error.message;
		metadata["error"] = state.error.to_dictionary();
	} else if (state.output.get_type() != Variant::NIL) {
		content = _variant_to_display_text(state.output);
	} else if (state.result.get_type() != Variant::NIL) {
		content = _variant_to_display_text(state.result);
	} else if (state.progress.get_type() != Variant::NIL) {
		content = _variant_to_display_text(state.progress);
	}

	Dictionary message;
	message["id"] = p_message.id + ":" + p_content.id;
	message["session_id"] = p_message.session_id;
	message["role"] = "tool";
	message["content"] = content;
	message["metadata"] = metadata;
	message["created_at"] = p_message.time_created;
	return message;
}

void AIAgentV1UIAdapter::_append_ui_messages_from_session_message(const AISessionMessage &p_message, Array &r_messages) {
	PackedStringArray assistant_text;
	Array tool_messages;
	for (int i = 0; i < p_message.content.size(); i++) {
		const AIAssistantContent &content = p_message.content[i];
		if (content.type == "tool") {
			tool_messages.push_back(_tool_content_to_ui_message(p_message, content));
		} else if (!content.text.is_empty()) {
			assistant_text.push_back(content.text);
		}
	}

	String text = p_message.text;
	if (text.is_empty() && !assistant_text.is_empty()) {
		text = String("\n\n").join(assistant_text);
	}

	const bool should_emit_base_message = p_message.type != AI_SESSION_MESSAGE_ASSISTANT || !text.strip_edges().is_empty() || tool_messages.is_empty();
	if (should_emit_base_message) {
		Dictionary message;
		message["id"] = p_message.id;
		message["session_id"] = p_message.session_id;
		message["role"] = _message_role(p_message);
		message["content"] = text;
		message["metadata"] = _base_message_metadata(p_message);
		message["created_at"] = p_message.time_created;
		r_messages.push_back(message);
	}

	for (int i = 0; i < tool_messages.size(); i++) {
		r_messages.push_back(tool_messages[i]);
	}
}

Dictionary AIAgentV1UIAdapter::_permission_request_to_view(const Dictionary &p_request) {
	Dictionary request = p_request.duplicate(true);
	if (!request.has("source") || request["source"].get_type() != Variant::DICTIONARY) {
		request["source"] = Dictionary();
	}
	Dictionary source = request["source"];
	if (!source.has("tool_name") && source.has("tool")) {
		source["tool_name"] = source["tool"];
	}
	request["source"] = source;
	return request;
}

String AIAgentV1UIAdapter::_resolve_session_id(const String &p_session_id) const {
	const String session_id = p_session_id.strip_edges();
	return session_id.is_empty() ? active_session_id : session_id;
}

Array AIAgentV1UIAdapter::_project_and_get_messages(const String &p_session_id) {
	Array result;
	if (p_session_id.strip_edges().is_empty() || session_service.is_null()) {
		return result;
	}

	Ref<AISessionProjector> projector = session_service->get_projector();
	Ref<AIEventStore> event_store = session_service->get_event_store();
	if (projector.is_valid() && event_store.is_valid()) {
		projector->project_from_store(event_store, p_session_id, projector->get_projected_seq(p_session_id));
		const Vector<AISessionInput> inputs = projector->get_inputs_struct(p_session_id);
		const Vector<AISessionMessage> messages = projector->get_messages_struct(p_session_id);

		HashSet<String> projected_user_message_ids;
		for (int i = 0; i < messages.size(); i++) {
			if (messages[i].type == AI_SESSION_MESSAGE_USER) {
				projected_user_message_ids.insert(messages[i].id);
			}
		}
		for (int i = 0; i < inputs.size(); i++) {
			if (!projected_user_message_ids.has(inputs[i].id)) {
				result.push_back(_input_to_ui_message(inputs[i]));
			}
		}
		for (int i = 0; i < messages.size(); i++) {
			_append_ui_messages_from_session_message(messages[i], result);
		}
	}
	return result;
}

Dictionary AIAgentV1UIAdapter::_build_run_state(const String &p_session_id) const {
	Dictionary result;
	result["session_id"] = p_session_id;
	result["state"] = "idle";
	result["busy"] = false;
	result["can_send"] = !p_session_id.is_empty();
	result["can_cancel"] = false;
	result["status_text"] = String();
	result["active_run_id"] = String();
	result["wake_pending"] = false;
	result["interrupted"] = false;
	result["interrupt_reason"] = String();

	if (p_session_id.is_empty() || session_service.is_null()) {
		result["can_send"] = false;
		return result;
	}

	Ref<AISessionExecution> execution = session_service->get_execution();
	if (execution.is_valid()) {
		const AISessionExecutionState state = execution->get_state_struct(p_session_id);
		result["active_run_id"] = state.active_run_id;
		result["wake_pending"] = state.wake_pending;
		result["interrupted"] = state.interrupted;
		result["interrupt_reason"] = state.interrupt_reason;
		if (state.interrupted) {
			result["state"] = "interrupted";
			result["status_text"] = state.interrupt_reason;
		}
		if (state.wake_pending) {
			result["state"] = "preparing";
		}
		if (state.active) {
			result["state"] = "running";
			result["busy"] = true;
			result["can_send"] = false;
			result["can_cancel"] = true;
		}
	}

	Ref<AIPermissionService> permission_service = session_service->get_permission_service();
	if (permission_service.is_valid()) {
		const Array pending = permission_service->get_pending_requests();
		for (int i = 0; i < pending.size(); i++) {
			if (pending[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary request = pending[i];
			if (String(request.get("session_id", String())) == p_session_id) {
				result["state"] = "waiting_permission";
				result["busy"] = true;
				result["can_send"] = false;
				result["can_cancel"] = true;
				result["status_text"] = "Waiting for permission";
				break;
			}
		}
	}

	return result;
}

void AIAgentV1UIAdapter::_emit_error(const Dictionary &p_error_result) {
	Dictionary error = p_error_result.get("error", Dictionary());
	emit_signal(SNAME("error_reported"), error);
}

void AIAgentV1UIAdapter::_emit_messages_changed(const String &p_session_id) {
	if (p_session_id.is_empty()) {
		return;
	}
	emit_signal(SNAME("messages_changed"), p_session_id, _project_and_get_messages(p_session_id));
}

void AIAgentV1UIAdapter::_emit_run_state_changed(const String &p_session_id) {
	if (p_session_id.is_empty()) {
		return;
	}
	emit_signal(SNAME("run_state_changed"), _build_run_state(p_session_id));
}

void AIAgentV1UIAdapter::_permission_asked(const Dictionary &p_request) {
	const Dictionary request = _permission_request_to_view(p_request);
	const String request_id = request.get("request_id", String());
	if (!request_id.is_empty()) {
		emitted_permission_requests.insert(request_id);
	}
	emit_signal(SNAME("permission_requested"), request);
	_emit_run_state_changed(String(request.get("session_id", String())));
}

void AIAgentV1UIAdapter::_permission_replied(const Dictionary &p_reply) {
	emit_signal(SNAME("permission_resolved"), p_reply.duplicate(true));
	_emit_run_state_changed(String(p_reply.get("session_id", active_session_id)));
}

void AIAgentV1UIAdapter::_event_appended(const Dictionary &p_event) {
	const String session_id = String(p_event.get("aggregate_id", p_event.get("session_id", String())));
	if (session_id.is_empty()) {
		return;
	}
	_emit_messages_changed(session_id);
	_emit_run_state_changed(session_id);
}

void AIAgentV1UIAdapter::_drain_requested(const String &p_session_id, const String &p_run_id, int64_t p_wake_seq) {
	(void)p_run_id;
	(void)p_wake_seq;
	_emit_run_state_changed(p_session_id);
}

void AIAgentV1UIAdapter::_drain_settled(const String &p_session_id, const String &p_run_id, bool p_interrupted) {
	(void)p_run_id;
	(void)p_interrupted;
	_emit_messages_changed(p_session_id);
	_emit_run_state_changed(p_session_id);
}

void AIAgentV1UIAdapter::_interrupt_requested(const String &p_session_id, const String &p_reason) {
	(void)p_reason;
	_emit_run_state_changed(p_session_id);
}

void AIAgentV1UIAdapter::set_session_service(const Ref<AISessionService> &p_service) {
	session_service = p_service;
	_ensure_defaults();
}

Ref<AISessionService> AIAgentV1UIAdapter::get_session_service() const {
	return session_service;
}

Dictionary AIAgentV1UIAdapter::create_session(const Dictionary &p_options) {
	_ensure_defaults();

	Dictionary options = p_options.duplicate(true);
	if (!options.has("directory")) {
		options["directory"] = "res://";
	}

	const Dictionary created = session_service->create(options);
	if (!bool(created.get("success", false))) {
		_emit_error(created);
		return created;
	}

	active_session_id = created.get("id", String());
	emit_signal(SNAME("sessions_changed"), list_sessions());
	emit_signal(SNAME("active_session_changed"), created.duplicate(true));
	_emit_messages_changed(active_session_id);
	_emit_run_state_changed(active_session_id);
	return created.duplicate(true);
}

Array AIAgentV1UIAdapter::list_sessions() {
	_ensure_defaults();
	return session_service->get_session_store()->list_sessions();
}

bool AIAgentV1UIAdapter::set_active_session(const String &p_session_id) {
	_ensure_defaults();
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	Dictionary session = session_service->get_session_store()->get_session(session_id);
	if (!bool(session.get("success", false))) {
		_emit_error(session);
		return false;
	}

	active_session_id = session_id;
	emit_signal(SNAME("active_session_changed"), session.duplicate(true));
	_emit_messages_changed(active_session_id);
	_emit_run_state_changed(active_session_id);
	return true;
}

String AIAgentV1UIAdapter::get_active_session_id() const {
	return active_session_id;
}

Dictionary AIAgentV1UIAdapter::get_active_session() {
	_ensure_defaults();
	if (active_session_id.is_empty()) {
		return Dictionary();
	}
	return session_service->get_session_store()->get_session(active_session_id);
}

Array AIAgentV1UIAdapter::get_messages(const String &p_session_id) {
	_ensure_defaults();
	return _project_and_get_messages(_resolve_session_id(p_session_id));
}

Dictionary AIAgentV1UIAdapter::send_message(const String &p_text, const String &p_model_id, const String &p_agent_id, const Array &p_attachments, bool p_resume) {
	_ensure_defaults();

	const String text = p_text.strip_edges();
	if (text.is_empty()) {
		Dictionary details;
		details["field"] = "text";
		const Dictionary result = _make_error_result("Message text is required.", details);
		_emit_error(result);
		return result;
	}

	if (active_session_id.is_empty()) {
		Dictionary create;
		create["directory"] = "res://";
		if (!p_agent_id.strip_edges().is_empty()) {
			create["agent_id"] = p_agent_id.strip_edges();
		}
		create["title"] = text.substr(0, MIN(48, text.length()));
		const Dictionary created = create_session(create);
		if (!bool(created.get("success", false))) {
			return created;
		}
	}

	Dictionary prompt_metadata;
	prompt_metadata["source"] = "ai_component_ui";
	if (!p_model_id.strip_edges().is_empty()) {
		prompt_metadata["ui_model_id"] = p_model_id.strip_edges();
	}
	if (!p_agent_id.strip_edges().is_empty()) {
		prompt_metadata["ui_agent_id"] = p_agent_id.strip_edges();
	}

	Dictionary prompt;
	prompt["session_id"] = active_session_id;
	prompt["text"] = p_text;
	prompt["resume"] = p_resume;
	prompt["metadata"] = prompt_metadata;
	if (!p_attachments.is_empty()) {
		prompt["attachments"] = p_attachments.duplicate(true);
	}

	const Dictionary result = session_service->prompt(prompt);
	if (!bool(result.get("success", false))) {
		_emit_error(result);
		return result;
	}

	_emit_messages_changed(active_session_id);
	_emit_run_state_changed(active_session_id);
	return result.duplicate(true);
}

Dictionary AIAgentV1UIAdapter::cancel_active_run(const String &p_reason) {
	_ensure_defaults();
	if (active_session_id.is_empty()) {
		const Dictionary result = _make_error_result("No active session to cancel.");
		_emit_error(result);
		return result;
	}

	Dictionary input;
	input["session_id"] = active_session_id;
	input["reason"] = p_reason.strip_edges().is_empty() ? String("User cancelled.") : p_reason;
	const Dictionary result = session_service->interrupt(input);
	if (!bool(result.get("success", false))) {
		_emit_error(result);
	}
	_emit_messages_changed(active_session_id);
	_emit_run_state_changed(active_session_id);
	return result.duplicate(true);
}

Dictionary AIAgentV1UIAdapter::get_run_state(const String &p_session_id) const {
	return _build_run_state(_resolve_session_id(p_session_id));
}

Array AIAgentV1UIAdapter::get_pending_permissions() const {
	Array result;
	if (session_service.is_null() || session_service->get_permission_service().is_null()) {
		return result;
	}

	const Array pending = session_service->get_permission_service()->get_pending_requests();
	for (int i = 0; i < pending.size(); i++) {
		if (pending[i].get_type() == Variant::DICTIONARY) {
			result.push_back(_permission_request_to_view(pending[i]));
		}
	}
	return result;
}

Array AIAgentV1UIAdapter::refresh_pending_permissions() {
	_ensure_defaults();
	const Array pending = get_pending_permissions();
	for (int i = 0; i < pending.size(); i++) {
		if (pending[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary request = pending[i];
		const String request_id = request.get("request_id", String());
		if (!request_id.is_empty() && emitted_permission_requests.has(request_id)) {
			continue;
		}
		if (!request_id.is_empty()) {
			emitted_permission_requests.insert(request_id);
		}
		emit_signal(SNAME("permission_requested"), request);
	}
	_emit_run_state_changed(active_session_id);
	return pending;
}

Dictionary AIAgentV1UIAdapter::reply_permission(const String &p_request_id, bool p_allowed, const String &p_reason, const Dictionary &p_options) {
	_ensure_defaults();
	Dictionary input = p_options.duplicate(true);
	input["request_id"] = p_request_id;
	input["reply"] = p_allowed ? String("once") : String("reject");
	if (!p_reason.strip_edges().is_empty()) {
		input["reason"] = p_reason;
	}

	const Dictionary result = session_service->reply_permission(input);
	emit_signal(SNAME("permission_resolved"), result.duplicate(true));
	_emit_run_state_changed(String(result.get("session_id", active_session_id)));
	if (p_allowed && !bool(result.get("success", false))) {
		_emit_error(result);
	}
	return result.duplicate(true);
}
