/**************************************************************************/
/*  ai_session_service.cpp                                                */
/**************************************************************************/

#include "ai_session_service.h"

#include "editor/agent_v1/session/recovery/ai_startup_recovery.h"
#include "editor/agent_v1/tools/ai_builtin_tools_v1.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AISessionService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_project_scope", "project_id", "directory", "storage_root"), &AISessionService::set_project_scope, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_project_scope_id"), &AISessionService::get_project_scope_id);
	ClassDB::bind_method(D_METHOD("get_project_scope_directory"), &AISessionService::get_project_scope_directory);
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
	ClassDB::bind_method(D_METHOD("set_session_runner", "session_runner"), &AISessionService::set_session_runner);
	ClassDB::bind_method(D_METHOD("get_session_runner"), &AISessionService::get_session_runner);
	ClassDB::bind_method(D_METHOD("set_compaction_service", "service"), &AISessionService::set_compaction_service);
	ClassDB::bind_method(D_METHOD("get_compaction_service"), &AISessionService::get_compaction_service);
	ClassDB::bind_method(D_METHOD("set_context_epoch_store", "store"), &AISessionService::set_context_epoch_store);
	ClassDB::bind_method(D_METHOD("get_context_epoch_store"), &AISessionService::get_context_epoch_store);
	ClassDB::bind_method(D_METHOD("set_context_source_registry", "registry"), &AISessionService::set_context_source_registry);
	ClassDB::bind_method(D_METHOD("get_context_source_registry"), &AISessionService::get_context_source_registry);
	ClassDB::bind_method(D_METHOD("set_context_epoch_service", "service"), &AISessionService::set_context_epoch_service);
	ClassDB::bind_method(D_METHOD("get_context_epoch_service"), &AISessionService::get_context_epoch_service);
	ClassDB::bind_method(D_METHOD("set_config_service", "config_service"), &AISessionService::set_config_service);
	ClassDB::bind_method(D_METHOD("get_config_service"), &AISessionService::get_config_service);
	ClassDB::bind_method(D_METHOD("set_runtime_registry", "registry"), &AISessionService::set_runtime_registry);
	ClassDB::bind_method(D_METHOD("get_runtime_registry"), &AISessionService::get_runtime_registry);
	ClassDB::bind_method(D_METHOD("set_permission_service", "permission_service"), &AISessionService::set_permission_service);
	ClassDB::bind_method(D_METHOD("get_permission_service"), &AISessionService::get_permission_service);
	ClassDB::bind_method(D_METHOD("set_tool_registry", "tool_registry"), &AISessionService::set_tool_registry);
	ClassDB::bind_method(D_METHOD("get_tool_registry"), &AISessionService::get_tool_registry);
	ClassDB::bind_method(D_METHOD("set_todo_store", "store"), &AISessionService::set_todo_store);
	ClassDB::bind_method(D_METHOD("get_todo_store"), &AISessionService::get_todo_store);
	ClassDB::bind_method(D_METHOD("set_todo_service", "service"), &AISessionService::set_todo_service);
	ClassDB::bind_method(D_METHOD("get_todo_service"), &AISessionService::get_todo_service);
	ClassDB::bind_method(D_METHOD("set_attachment_blob_store", "blob_store"), &AISessionService::set_attachment_blob_store);
	ClassDB::bind_method(D_METHOD("get_attachment_blob_store"), &AISessionService::get_attachment_blob_store);
	ClassDB::bind_method(D_METHOD("set_attachment_resolver", "resolver"), &AISessionService::set_attachment_resolver);
	ClassDB::bind_method(D_METHOD("get_attachment_resolver"), &AISessionService::get_attachment_resolver);
	ClassDB::bind_method(D_METHOD("set_model_part_builder", "builder"), &AISessionService::set_model_part_builder);
	ClassDB::bind_method(D_METHOD("get_model_part_builder"), &AISessionService::get_model_part_builder);
	ClassDB::bind_method(D_METHOD("set_skill_service", "service"), &AISessionService::set_skill_service);
	ClassDB::bind_method(D_METHOD("get_skill_service"), &AISessionService::get_skill_service);
	ClassDB::bind_method(D_METHOD("set_agent_service", "service"), &AISessionService::set_agent_service);
	ClassDB::bind_method(D_METHOD("get_agent_service"), &AISessionService::get_agent_service);
	ClassDB::bind_method(D_METHOD("create", "input"), &AISessionService::create);
	ClassDB::bind_method(D_METHOD("prompt", "input"), &AISessionService::prompt);
	ClassDB::bind_method(D_METHOD("reply_permission", "input"), &AISessionService::reply_permission);
	ClassDB::bind_method(D_METHOD("interrupt", "input"), &AISessionService::interrupt);
	ClassDB::bind_method(D_METHOD("promote_eligible", "session_id", "mode"), &AISessionService::promote_eligible, DEFVAL("new-activity"));
	ClassDB::bind_method(D_METHOD("update_todos", "session_id", "todos"), &AISessionService::update_todos);
	ClassDB::bind_method(D_METHOD("get_todos", "session_id"), &AISessionService::get_todos);
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
	if (p_parts.is_empty() && p_input.has("text")) {
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
			continue;
		} else if (type == "agent") {
			prompt.agents.push_back(AIAgentReference::from_dictionary(part));
		} else if (type == "reference") {
			prompt.references.push_back(AIPromptReference::from_dictionary(part));
		}
	}
	return prompt;
}

Array AISessionService::_parts_from_prompt(const AIPrompt &p_prompt) {
	Array parts;
	if (!p_prompt.text.strip_edges().is_empty()) {
		Dictionary text_part;
		text_part["type"] = "text";
		text_part["text"] = p_prompt.text;
		parts.push_back(text_part);
	}
	for (int i = 0; i < p_prompt.files.size(); i++) {
		Dictionary attachment_part;
		attachment_part["type"] = "attachment";
		attachment_part["attachment"] = p_prompt.files[i].to_dictionary();
		parts.push_back(attachment_part);
	}
	for (int i = 0; i < p_prompt.agents.size(); i++) {
		Dictionary agent_part = p_prompt.agents[i].to_dictionary();
		agent_part["type"] = "agent";
		parts.push_back(agent_part);
	}
	for (int i = 0; i < p_prompt.references.size(); i++) {
		Dictionary reference_part = p_prompt.references[i].to_dictionary();
		reference_part["type"] = "reference";
		parts.push_back(reference_part);
	}
	return parts;
}

bool AISessionService::_part_is_attachment(const Dictionary &p_part) {
	const String type = String(p_part.get("type", String())).strip_edges().to_lower();
	return type == "file" || type == "attachment";
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

String AISessionService::_sanitize_project_scope_id(const String &p_project_id) {
	const String project_id = p_project_id.strip_edges();
	if (project_id.is_empty()) {
		return String();
	}
	return project_id.validate_filename();
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
	if (session_runner.is_null()) {
		session_runner.instantiate();
	}
	if (compaction_service.is_null()) {
		compaction_service.instantiate();
	}
	if (context_epoch_store.is_null()) {
		context_epoch_store.instantiate();
	}
	if (context_source_registry.is_null()) {
		context_source_registry.instantiate();
	}
	if (context_epoch_service.is_null()) {
		context_epoch_service.instantiate();
	}
	if (config_service.is_null()) {
		config_service.instantiate();
	}
	if (runtime_registry.is_null()) {
		runtime_registry.instantiate();
	}
	if (permission_service.is_null()) {
		permission_service.instantiate();
	}
	if (tool_registry.is_null()) {
		tool_registry.instantiate();
		tool_registry->register_builtin_tools();
	}
	if (todo_store.is_null()) {
		todo_store.instantiate();
	}
	if (todo_service.is_null()) {
		todo_service.instantiate();
	}
	if (attachment_blob_store.is_null()) {
		attachment_blob_store.instantiate();
	}
	if (attachment_resolver.is_null()) {
		attachment_resolver.instantiate();
	}
	if (model_part_builder.is_null()) {
		model_part_builder.instantiate();
	}
	if (skill_service.is_null()) {
		skill_service.instantiate();
	}
	if (agent_service.is_null()) {
		agent_service.instantiate();
	}
	if (task_tool.is_null()) {
		task_tool.instantiate();
	}
	if (todo_write_tool.is_null()) {
		todo_write_tool.instantiate();
	}
}

void AISessionService::_wire_dependencies() {
	if (attachment_resolver.is_valid()) {
		attachment_resolver->set_blob_store(attachment_blob_store);
	}
	if (model_part_builder.is_valid()) {
		model_part_builder->set_blob_store(attachment_blob_store);
	}
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
	if (session_runner.is_valid()) {
		session_runner->set_prompt_promoter(prompt_promoter);
		session_runner->set_event_store(event_store);
		session_runner->set_projector(projector);
		session_runner->set_context_epoch_store(context_epoch_store);
		session_runner->set_context_source_registry(context_source_registry);
		session_runner->set_context_epoch_service(context_epoch_service);
		session_runner->set_config_service(config_service);
		session_runner->set_runtime_registry(runtime_registry);
		session_runner->set_tool_registry(tool_registry);
		session_runner->set_session_store(session_store);
		session_runner->set_compaction_service(compaction_service);
		session_runner->set_model_part_builder(model_part_builder);
		session_runner->set_skill_service(skill_service);
		session_runner->set_agent_service(agent_service);
	}
	if (compaction_service.is_valid()) {
		compaction_service->set_event_store(event_store);
		compaction_service->set_projector(projector);
		compaction_service->set_context_source_registry(context_source_registry);
		compaction_service->set_context_epoch_service(context_epoch_service);
	}
	if (context_source_registry.is_valid()) {
		context_source_registry->set_config_service(config_service);
	}
	if (context_epoch_service.is_valid()) {
		context_epoch_service->set_epoch_store(context_epoch_store);
		context_epoch_service->set_event_store(event_store);
		context_epoch_service->set_projector(projector);
	}
	if (permission_service.is_valid()) {
		permission_service->set_event_store(event_store);
	}
	if (todo_service.is_valid()) {
		todo_service->set_todo_store(todo_store);
		todo_service->set_event_store(event_store);
		todo_service->set_projector(projector);
	}
	if (tool_registry.is_valid()) {
		tool_registry->set_event_store(event_store);
		tool_registry->set_projector(projector);
		tool_registry->set_permission_service(permission_service);
	}
	if (skill_service.is_valid()) {
		skill_service->set_tool_registry(tool_registry);
	}
	if (agent_service.is_valid()) {
		agent_service->set_config_service(config_service);
		agent_service->set_session_store(session_store);
	}
	if (task_tool.is_valid()) {
		task_tool->setup(this, agent_service.ptr());
	}
	if (todo_write_tool.is_valid()) {
		todo_write_tool->set_todo_service(todo_service);
	}
	if (tool_registry.is_valid() && todo_write_tool.is_valid() && !todo_write_tool_registered) {
		Dictionary todo_metadata;
		todo_metadata["tool_origin"] = "builtin";
		todo_metadata["action"] = "todo.write";
		todo_metadata["session_service_bound"] = true;
		todo_write_tool_registered = tool_registry->register_tool_struct("todowrite", todo_write_tool, "builtin", todo_metadata);
	}
	if (tool_registry.is_valid() && task_tool.is_valid() && !tool_registry->has_tool("task")) {
		Dictionary task_metadata;
		task_metadata["tool_origin"] = "builtin";
		task_metadata["phase"] = "10";
		tool_registry->register_tool_struct("task", task_tool, "builtin", task_metadata);
	}
	if (execution.is_valid()) {
		if (session_runner.is_valid()) {
			execution->set_runner(session_runner);
		} else {
			execution->set_runner(empty_runner);
		}
	}
}

void AISessionService::_apply_project_scope_storage() {
	if (project_scope_id.is_empty() || project_scope_storage_root.is_empty()) {
		return;
	}

	const String project_root = project_scope_storage_root.path_join(project_scope_id);
	const String session_dir = project_root.path_join("sessions");
	const String input_dir = project_root.path_join("session_inputs");
	const String event_dir = project_root.path_join("events");
	const String todo_dir = project_root.path_join("todos");
	const String attachment_dir = project_root.path_join("attachments");

	if (session_store.is_valid() && session_store->get_base_dir() != session_dir) {
		session_store->set_base_dir(session_dir);
	}
	if (input_store.is_valid() && input_store->get_base_dir() != input_dir) {
		input_store->set_base_dir(input_dir);
	}
	if (event_store.is_valid() && event_store->get_base_dir() != event_dir) {
		event_store->set_base_dir(event_dir);
	}
	if (todo_store.is_valid() && todo_store->get_base_dir() != todo_dir) {
		todo_store->set_base_dir(todo_dir);
	}
	if (attachment_blob_store.is_valid() && attachment_blob_store->get_base_dir() != attachment_dir) {
		attachment_blob_store->set_base_dir(attachment_dir);
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

	r_error = AIError::make(AI_ERROR_VALIDATION, "SessionService.prompt requires an existing session_id. Call SessionService.create first.");
	return false;
}

bool AISessionService::_resolve_prompt_attachments(const Dictionary &p_input, const AISessionRow &p_session, const Array &p_parts, AIPrompt &r_prompt, AIError &r_error) {
	if (attachment_resolver.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionService has no AttachmentResolver.");
		return false;
	}

	Vector<AIFileAttachment> resolved_files;
	for (int i = 0; i < r_prompt.files.size(); i++) {
		AIFileAttachment file = r_prompt.files[i];
		const String blob_ref = String(file.metadata.get("blob_id", file.metadata.get("blobID", file.path))).strip_edges();
		if (blob_ref.begins_with("blob_") && attachment_blob_store.is_valid() && attachment_blob_store->has_blob_struct(blob_ref)) {
			resolved_files.push_back(file);
			continue;
		}

		AIFileAttachment resolved;
		if (!attachment_resolver->resolve_struct(p_session.id, p_session.location, file.to_dictionary(), resolved, r_error)) {
			return false;
		}
		resolved_files.push_back(resolved);
	}

	if (p_input.get("attachments", Variant()).get_type() == Variant::ARRAY) {
		if (!attachment_resolver->resolve_many_struct(p_session.id, p_session.location, p_input["attachments"], resolved_files, r_error)) {
			return false;
		}
	}

	for (int i = 0; i < p_parts.size(); i++) {
		if (p_parts[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary part = p_parts[i];
		if (!_part_is_attachment(part)) {
			continue;
		}
		Dictionary attachment = part;
		if (part.get("attachment", Variant()).get_type() == Variant::DICTIONARY) {
			attachment = Dictionary(part["attachment"]).duplicate(true);
		}

		AIFileAttachment resolved;
		if (!attachment_resolver->resolve_struct(p_session.id, p_session.location, attachment, resolved, r_error)) {
			return false;
		}
		resolved_files.push_back(resolved);
	}

	r_prompt.files = resolved_files;
	r_error = AIError::none();
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
	const String idempotency_key = "prompt.admitted:" + r_input.session_id + ":" + (r_input.message_id.is_empty() ? r_input.id : r_input.message_id);
	if (!event_store->append_idempotent(r_input.session_id, AIDomainEventTypes::prompt_admitted(), data, false, idempotency_key, row, event_error)) {
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

bool AISessionService::_append_interrupted_activity_events(const String &p_session_id, const String &p_reason, AIError &r_error) {
	if (event_store.is_null()) {
		r_error = AIError::none();
		return true;
	}

	Dictionary report;
	report["interrupted_sessions"] = Array();
	report["failed_tool_calls"] = Array();
	report["notes"] = Array();
	const String reason = p_reason.strip_edges().is_empty() ? String("Session interrupted.") : p_reason;
	return AIStartupRecovery::cleanup_open_activity_struct(event_store, projector, p_session_id, reason, true, true, false, report, r_error);
}

bool AISessionService::_reject_pending_permissions(const String &p_session_id, const String &p_reason, AIError &r_error) {
	if (permission_service.is_null()) {
		r_error = AIError::none();
		return true;
	}

	const String session_id = p_session_id.strip_edges();
	const String reason = p_reason.strip_edges().is_empty() ? String("Session interrupted.") : p_reason;
	const Array pending = permission_service->get_pending_requests();
	for (int i = 0; i < pending.size(); i++) {
		if (pending[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary request = pending[i];
		if (String(request.get("session_id", String())).strip_edges() != session_id) {
			continue;
		}

		const String request_id = String(request.get("request_id", request.get("requestID", String()))).strip_edges();
		if (request_id.is_empty()) {
			continue;
		}

		Dictionary reply;
		reply["request_id"] = request_id;
		reply["reply"] = "reject";
		reply["reason"] = reason;

		AIPermissionDecision decision;
		AIError reply_error;
		if (!permission_service->reply_struct(reply, decision, reply_error) && !(decision.denied && reply_error.kind == AI_ERROR_PERMISSION)) {
			r_error = reply_error.is_error() ? reply_error : AIError::make(AI_ERROR_INTERNAL, "Failed to reject pending permission request.");
			return false;
		}
	}

	r_error = AIError::none();
	return true;
}

void AISessionService::set_project_scope(const String &p_project_id, const String &p_directory, const String &p_storage_root) {
	_ensure_defaults();
	project_scope_id = _sanitize_project_scope_id(p_project_id);
	project_scope_directory = p_directory.strip_edges();
	const String storage_root = p_storage_root.strip_edges();
	if (!storage_root.is_empty() || project_scope_storage_root.is_empty()) {
		project_scope_storage_root = storage_root;
	}

	_apply_project_scope_storage();
	_wire_dependencies();
}

String AISessionService::get_project_scope_id() const {
	return project_scope_id;
}

String AISessionService::get_project_scope_directory() const {
	return project_scope_directory;
}

void AISessionService::set_session_store(const Ref<AISessionStore> &p_session_store) {
	session_store = p_session_store;
	_ensure_defaults();
	_apply_project_scope_storage();
	_wire_dependencies();
}

Ref<AISessionStore> AISessionService::get_session_store() const {
	return session_store;
}

void AISessionService::set_input_store(const Ref<AISessionInputStore> &p_input_store) {
	input_store = p_input_store;
	_ensure_defaults();
	_apply_project_scope_storage();
	_wire_dependencies();
}

Ref<AISessionInputStore> AISessionService::get_input_store() const {
	return input_store;
}

void AISessionService::set_event_store(const Ref<AIEventStore> &p_event_store) {
	event_store = p_event_store;
	_ensure_defaults();
	_apply_project_scope_storage();
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

void AISessionService::set_session_runner(const Ref<AISessionRunner> &p_session_runner) {
	session_runner = p_session_runner;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AISessionRunner> AISessionService::get_session_runner() const {
	return session_runner;
}

void AISessionService::set_compaction_service(const Ref<AICompactionService> &p_service) {
	compaction_service = p_service;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AICompactionService> AISessionService::get_compaction_service() const {
	return compaction_service;
}

void AISessionService::set_context_epoch_store(const Ref<AIContextEpochStore> &p_store) {
	context_epoch_store = p_store;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIContextEpochStore> AISessionService::get_context_epoch_store() const {
	return context_epoch_store;
}

void AISessionService::set_context_source_registry(const Ref<AIContextSourceRegistry> &p_registry) {
	context_source_registry = p_registry;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIContextSourceRegistry> AISessionService::get_context_source_registry() const {
	return context_source_registry;
}

void AISessionService::set_context_epoch_service(const Ref<AIContextEpochService> &p_service) {
	context_epoch_service = p_service;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIContextEpochService> AISessionService::get_context_epoch_service() const {
	return context_epoch_service;
}

void AISessionService::set_config_service(const Ref<AIConfigService> &p_config_service) {
	config_service = p_config_service;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIConfigService> AISessionService::get_config_service() const {
	return config_service;
}

void AISessionService::set_runtime_registry(const Ref<AILLMRuntimeRegistry> &p_registry) {
	runtime_registry = p_registry;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AILLMRuntimeRegistry> AISessionService::get_runtime_registry() const {
	return runtime_registry;
}

void AISessionService::set_permission_service(const Ref<AIPermissionService> &p_permission_service) {
	permission_service = p_permission_service;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIPermissionService> AISessionService::get_permission_service() const {
	return permission_service;
}

void AISessionService::set_tool_registry(const Ref<AIV1ToolRegistry> &p_tool_registry) {
	const bool registry_changed = tool_registry != p_tool_registry;
	tool_registry = p_tool_registry;
	if (registry_changed) {
		todo_write_tool_registered = false;
	}
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIV1ToolRegistry> AISessionService::get_tool_registry() const {
	return tool_registry;
}

void AISessionService::set_todo_store(const Ref<AITodoStore> &p_store) {
	todo_store = p_store;
	_ensure_defaults();
	_apply_project_scope_storage();
	_wire_dependencies();
}

Ref<AITodoStore> AISessionService::get_todo_store() const {
	return todo_store;
}

void AISessionService::set_todo_service(const Ref<AITodoService> &p_service) {
	todo_service = p_service;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AITodoService> AISessionService::get_todo_service() const {
	return todo_service;
}

void AISessionService::set_attachment_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store) {
	attachment_blob_store = p_blob_store;
	_ensure_defaults();
	_apply_project_scope_storage();
	_wire_dependencies();
}

Ref<AIAttachmentBlobStore> AISessionService::get_attachment_blob_store() const {
	return attachment_blob_store;
}

void AISessionService::set_attachment_resolver(const Ref<AIAttachmentResolver> &p_resolver) {
	attachment_resolver = p_resolver;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIAttachmentResolver> AISessionService::get_attachment_resolver() const {
	return attachment_resolver;
}

void AISessionService::set_model_part_builder(const Ref<AIModelPartBuilder> &p_builder) {
	model_part_builder = p_builder;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIModelPartBuilder> AISessionService::get_model_part_builder() const {
	return model_part_builder;
}

void AISessionService::set_skill_service(const Ref<AIV1SkillService> &p_service) {
	skill_service = p_service;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIV1SkillService> AISessionService::get_skill_service() const {
	return skill_service;
}

void AISessionService::set_agent_service(const Ref<AIAgentService> &p_service) {
	agent_service = p_service;
	_ensure_defaults();
	_wire_dependencies();
}

Ref<AIAgentService> AISessionService::get_agent_service() const {
	return agent_service;
}

Dictionary AISessionService::create(const Dictionary &p_input) {
	_ensure_defaults();
	Dictionary input = p_input.duplicate(true);
	if (!project_scope_id.is_empty() && !input.has("workspace_id") && !input.has("workspaceID")) {
		input["workspace_id"] = project_scope_id;
	}
	if (!project_scope_directory.is_empty() && !input.has("directory") && input.get("location", Variant()).get_type() != Variant::DICTIONARY) {
		input["directory"] = project_scope_directory;
	}

	AISessionRow session;
	bool created = false;
	String error;
	if (!session_store->create_or_reuse(input, session, created, error)) {
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

	const Array input_parts = _parts_from_input(p_input);
	AISessionInputRecord request;
	request.id = p_input.get("prompt_id", p_input.get("promptID", String()));
	request.session_id = session.id;
	request.message_id = p_input.get("message_id", p_input.get("messageID", String()));
	request.prompt = _prompt_from_input(p_input, input_parts);
	if (!_resolve_prompt_attachments(p_input, session, input_parts, request.prompt, error)) {
		return _make_error_result(error);
	}
	request.parts = _parts_from_prompt(request.prompt);
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

Dictionary AISessionService::reply_permission(const Dictionary &p_input) {
	_ensure_defaults();
	_wire_dependencies();
	if (permission_service.is_null()) {
		return _make_error_result(AIError::make(AI_ERROR_UNAVAILABLE, "SessionService has no PermissionService."));
	}

	AIPermissionDecision decision;
	AIError error;
	const bool allowed = permission_service->reply_struct(p_input, decision, error);
	if (!allowed && error.kind != AI_ERROR_PERMISSION) {
		return _make_error_result(error);
	}

	bool wake_scheduled = false;
	if (allowed && execution.is_valid() && !decision.session_id.strip_edges().is_empty()) {
		AISessionExecutionState state;
		execution->wake_struct(decision.session_id, 0, state);
		wake_scheduled = true;
	}

	Dictionary result = decision.to_dictionary();
	result["success"] = allowed;
	result["wake_scheduled"] = wake_scheduled;
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
	AIError permission_error;
	if (!_reject_pending_permissions(session_id, reason, permission_error)) {
		return _make_error_result(permission_error);
	}
	const Dictionary interrupted = execution.is_valid() ? execution->interrupt(session_id, reason) : Dictionary();
	result["interrupted"] = interrupted;

	if (!bool(interrupted.get("interrupted", false))) {
		AIError tool_error;
		if (!_append_interrupted_activity_events(session_id, reason, tool_error)) {
			return _make_error_result(tool_error);
		}
	}
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

Dictionary AISessionService::update_todos(const String &p_session_id, const Array &p_todos) {
	_ensure_defaults();
	_wire_dependencies();
	if (todo_service.is_null()) {
		return _make_error_result(AIError::make(AI_ERROR_UNAVAILABLE, "SessionService has no TodoService."));
	}
	return todo_service->update_todos(p_session_id, p_todos);
}

Array AISessionService::get_todos(const String &p_session_id) {
	_ensure_defaults();
	_wire_dependencies();
	if (todo_service.is_null()) {
		return Array();
	}
	return todo_service->get_todos(p_session_id);
}
