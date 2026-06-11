/**************************************************************************/
/*  ai_session_runner.cpp                                                 */
/**************************************************************************/

#include "ai_session_runner.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

class AISessionRunnerEventRecorder : public RefCounted {
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;
	Ref<AIV1ToolMaterialization> tool_materialization;
	String session_id;
	String agent_id;
	String assistant_message_id;
	String text_content_id;
	String reasoning_content_id;
	String text;
	String reasoning;
	AIError error;
	bool needs_continuation = false;
	bool waiting_permission = false;

	bool _append_event(const String &p_type, const Dictionary &p_data, bool p_live_only) {
		if (event_store.is_null()) {
			error = AIError::make(AI_ERROR_UNAVAILABLE, "Session runner recorder has no EventStore.");
			return false;
		}

		AIEventRow row;
		String event_error;
		if (!event_store->append(session_id, p_type, p_data, p_live_only, row, event_error)) {
			error = AIError::make(AI_ERROR_INTERNAL, event_error);
			return false;
		}
		if (!row.live_only && projector.is_valid()) {
			projector->project(row);
		}
		return true;
	}

public:
	void setup(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const Ref<AIV1ToolMaterialization> &p_tool_materialization, const String &p_session_id, const String &p_agent_id, const String &p_assistant_message_id) {
		event_store = p_event_store;
		projector = p_projector;
		tool_materialization = p_tool_materialization;
		session_id = p_session_id;
		agent_id = p_agent_id;
		assistant_message_id = p_assistant_message_id;
		text_content_id = AIId::make("content");
		reasoning_content_id = AIId::make("reasoning");
	}

	String get_text() const {
		return text;
	}

	String get_reasoning() const {
		return reasoning;
	}

	String get_text_content_id() const {
		return text_content_id;
	}

	String get_reasoning_content_id() const {
		return reasoning_content_id;
	}

	AIError get_error() const {
		return error;
	}

	bool has_local_tool_settlement() const {
		return needs_continuation;
	}

	bool is_waiting_permission() const {
		return waiting_permission;
	}

	bool handle_event(const Dictionary &p_event) {
		const String type = String(p_event.get("type", String()));
		if (type == "text-delta") {
			const String delta = p_event.get("text", String());
			text += delta;

			Dictionary data;
			data["assistant_message_id"] = assistant_message_id;
			data["content_id"] = text_content_id;
			data["text"] = delta;
			data["delta"] = delta;
			return !_append_event(AIDomainEventTypes::text_delta(), data, true);
		}
		if (type == "reasoning-delta") {
			const String delta = p_event.get("text", String());
			reasoning += delta;

			Dictionary data;
			data["assistant_message_id"] = assistant_message_id;
			data["content_id"] = reasoning_content_id;
			data["text"] = delta;
			data["delta"] = delta;
			data["provider_metadata"] = p_event.get("provider_metadata", Dictionary());
			return !_append_event(AIDomainEventTypes::reasoning_delta(), data, true);
		}
		if (type == "tool-call") {
			const String call_id = p_event.get("id", AIId::make("call"));
			const String tool_name = p_event.get("name", String());
			Dictionary data;
			data["assistant_message_id"] = assistant_message_id;
			data["call_id"] = call_id;
			data["tool"] = tool_name;
			data["name"] = tool_name;
			data["input"] = p_event.get("input", Variant());
			data["provider"] = p_event.get("provider_metadata", Dictionary());
			data["provider_executed"] = bool(p_event.get("provider_executed", false));
			if (tool_materialization.is_valid()) {
				const Dictionary identity = tool_materialization->get_tool_identity(tool_name);
				if (!identity.is_empty()) {
					data["registration_identity"] = identity;
				}
			}
			if (!_append_event(AIDomainEventTypes::tool_called(), data, false)) {
				return true;
			}

			if (tool_materialization.is_null() || bool(p_event.get("provider_executed", false))) {
				return false;
			}

			Dictionary call;
			call["id"] = call_id;
			call["name"] = tool_name;
			call["input"] = p_event.get("input", Variant());
			call["provider_executed"] = false;

			Dictionary settle_input;
			settle_input["session_id"] = session_id;
			settle_input["agent"] = agent_id;
			settle_input["assistant_message_id"] = assistant_message_id;
			if (data.has("registration_identity")) {
				settle_input["registration_identity"] = data["registration_identity"];
			}
			settle_input["call"] = call;

			AIV1ToolSettlement settlement;
			AIError settlement_error;
			if (!tool_materialization->settle_struct(settle_input, settlement, settlement_error)) {
				error = settlement_error.is_error() ? settlement_error : AIError::make(AI_ERROR_INTERNAL, "Tool settlement failed.");
				return true;
			}
			if (settlement.pending) {
				waiting_permission = true;
				return false;
			}
			needs_continuation = needs_continuation || settlement.needs_continuation;
			return false;
		}
		if (type == "provider-error") {
			error = AIError::make(AI_ERROR_PROVIDER, "Provider stream error.", p_event.get("error", Dictionary()));
			return true;
		}
		return false;
	}
};

static String _ai_session_runner_tool_key(const Dictionary &p_data) {
	const String assistant_message_id = String(p_data.get("assistant_message_id", p_data.get("assistantMessageID", String()))).strip_edges();
	const String call_id = String(p_data.get("call_id", p_data.get("callID", p_data.get("id", String())))).strip_edges();
	return assistant_message_id + "|" + call_id;
}

void AISessionRunner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_prompt_promoter", "prompt_promoter"), &AISessionRunner::set_prompt_promoter);
	ClassDB::bind_method(D_METHOD("get_prompt_promoter"), &AISessionRunner::get_prompt_promoter);
	ClassDB::bind_method(D_METHOD("set_event_store", "event_store"), &AISessionRunner::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AISessionRunner::get_event_store);
	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &AISessionRunner::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &AISessionRunner::get_projector);
	ClassDB::bind_method(D_METHOD("set_context_epoch_store", "store"), &AISessionRunner::set_context_epoch_store);
	ClassDB::bind_method(D_METHOD("get_context_epoch_store"), &AISessionRunner::get_context_epoch_store);
	ClassDB::bind_method(D_METHOD("set_config_service", "config_service"), &AISessionRunner::set_config_service);
	ClassDB::bind_method(D_METHOD("get_config_service"), &AISessionRunner::get_config_service);
	ClassDB::bind_method(D_METHOD("set_runtime_registry", "registry"), &AISessionRunner::set_runtime_registry);
	ClassDB::bind_method(D_METHOD("get_runtime_registry"), &AISessionRunner::get_runtime_registry);
	ClassDB::bind_method(D_METHOD("set_tool_registry", "registry"), &AISessionRunner::set_tool_registry);
	ClassDB::bind_method(D_METHOD("get_tool_registry"), &AISessionRunner::get_tool_registry);
	ClassDB::bind_method(D_METHOD("set_session_store", "store"), &AISessionRunner::set_session_store);
	ClassDB::bind_method(D_METHOD("get_session_store"), &AISessionRunner::get_session_store);
	ClassDB::bind_method(D_METHOD("run", "session_id", "force"), &AISessionRunner::run, DEFVAL(false));
}

AISessionRunner::AISessionRunner() {
	context_epoch_store.instantiate();
	config_service.instantiate();
	runtime_registry.instantiate();
	tool_registry.instantiate();
	tool_registry->register_builtin_tools();
}

String AISessionRunner::_message_text(const AISessionMessage &p_message) {
	if (!p_message.text.is_empty()) {
		return p_message.text;
	}
	String text;
	for (int i = 0; i < p_message.content.size(); i++) {
		const AIAssistantContent &content = p_message.content[i];
		if (content.type == "text" || content.type == "reasoning") {
			text += text.is_empty() ? content.text : "\n" + content.text;
		} else if (content.type == "tool") {
			String tool_text;
			const String tool_name = content.name.is_empty() ? String("tool") : content.name;
			if (content.tool_state.status == AI_TOOL_STATUS_SUCCESS) {
				const Variant output = content.tool_state.output.get_type() == Variant::NIL ? content.tool_state.result : content.tool_state.output;
				tool_text = "Tool " + tool_name + " result:\n" + Variant(output).stringify();
			} else if (content.tool_state.status == AI_TOOL_STATUS_FAILED) {
				tool_text = "Tool " + tool_name + " failed:\n" + content.tool_state.error.message;
			}
			if (!tool_text.is_empty()) {
				text += text.is_empty() ? tool_text : "\n" + tool_text;
			}
		}
	}
	return text;
}

AIModelMessage AISessionRunner::_message_to_model(const AISessionMessage &p_message) {
	String role = "user";
	if (p_message.type == AI_SESSION_MESSAGE_ASSISTANT) {
		role = "assistant";
	} else if (p_message.type == AI_SESSION_MESSAGE_SYSTEM || p_message.type == AI_SESSION_MESSAGE_COMPACTION) {
		role = "system";
	}

	String text = _message_text(p_message);
	if (p_message.type == AI_SESSION_MESSAGE_COMPACTION && !text.is_empty()) {
		text = "Compaction summary:\n" + text;
	}
	return AIModelMessage::text_message(role, text, p_message.id);
}

String AISessionRunner::_system_baseline_from_array(const Array &p_system) {
	String baseline;
	for (int i = 0; i < p_system.size(); i++) {
		const String item = String(p_system[i]).strip_edges();
		if (!item.is_empty()) {
			baseline += baseline.is_empty() ? item : "\n\n" + item;
		}
	}
	return baseline;
}

Vector<AIModelPart> AISessionRunner::_system_parts_from_array(const Array &p_system) {
	Vector<AIModelPart> result;
	for (int i = 0; i < p_system.size(); i++) {
		const String item = String(p_system[i]).strip_edges();
		if (!item.is_empty()) {
			result.push_back(AIModelPart::text_part(item));
		}
	}
	return result;
}

Dictionary AISessionRunner::_make_error_result(const AIError &p_error) {
	Dictionary result;
	result["success"] = false;
	result["error"] = p_error.to_dictionary();
	return result;
}

bool AISessionRunner::_append_event(const String &p_session_id, const String &p_type, const Dictionary &p_data, bool p_live_only, AIEventRow &r_row, AIError &r_error) {
	if (event_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no EventStore.");
		return false;
	}
	String event_error;
	if (!event_store->append(p_session_id, p_type, p_data, p_live_only, r_row, event_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
		return false;
	}
	if (!r_row.live_only && projector.is_valid()) {
		projector->project(r_row);
	}
	return true;
}

bool AISessionRunner::_resolve_session(const String &p_session_id, AISessionRow &r_session, AIError &r_error) const {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session id is required to run.");
		return false;
	}
	if (session_store.is_valid()) {
		if (!session_store->get_session_struct(session_id, r_session)) {
			Dictionary details;
			details["session_id"] = session_id;
			r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Session not found.", details);
			return false;
		}
		return true;
	}

	r_session.id = session_id;
	r_session.agent_id = "main";
	r_error = AIError::none();
	return true;
}

bool AISessionRunner::_ensure_context_epoch(const String &p_session_id, const String &p_agent_id, const Array &p_system, const String &p_provider, const String &p_model, AIContextEpoch &r_epoch, AIError &r_error) {
	Dictionary snapshot;
	snapshot["provider"] = p_provider;
	snapshot["model"] = p_model;
	snapshot["tools"] = Array();
	snapshot["sources"] = Array();

	AIContextEpoch epoch;
	epoch.session_id = p_session_id;
	epoch.agent_id = p_agent_id;
	epoch.baseline = _system_baseline_from_array(p_system);
	epoch.snapshot = snapshot;
	epoch.revision = 1;
	if (context_epoch_store.is_valid()) {
		AIContextEpoch previous;
		if (context_epoch_store->get_epoch_struct(p_session_id, previous)) {
			epoch.revision = previous.revision + 1;
		}
	}

	Dictionary data = epoch.to_dictionary();
	data["epoch"] = epoch.to_dictionary();
	data["baseline"] = epoch.baseline;
	data["snapshot"] = snapshot;

	AIEventRow row;
	if (!_append_event(p_session_id, AIDomainEventTypes::context_updated(), data, false, row, r_error)) {
		return false;
	}

	epoch.baseline_seq = row.seq;
	if (context_epoch_store.is_valid()) {
		context_epoch_store->set_epoch_struct(epoch);
	}
	r_epoch = epoch;
	return true;
}

bool AISessionRunner::_build_request(const AISessionRow &p_session, const String &p_agent_id, const String &p_root_dir, int64_t p_wake_seq, AIModelRequest &r_request, Ref<AIV1ToolMaterialization> &r_tool_materialization, AIError &r_error) {
	if (config_service.is_null() || runtime_registry.is_null() || projector.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner dependencies are not wired.");
		return false;
	}

	const Dictionary config = config_service->get_config();
	if (!bool(config.get("success", true))) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "ConfigService failed to provide config.", config.get("error", Dictionary()));
		return false;
	}

	if (!runtime_registry->configure_from_config_struct(config, r_error)) {
		return false;
	}

	if (tool_registry.is_valid()) {
		const Dictionary permissions = config.get("permissions", Dictionary());
		const Array rules = permissions.get("rules", Array());
		if (tool_registry->get_permission_service().is_valid()) {
			tool_registry->get_permission_service()->set_rules(rules);
		}
		r_tool_materialization = tool_registry->materialize_struct(p_root_dir, rules);
		if (r_tool_materialization.is_valid()) {
			const Array definitions = r_tool_materialization->get_definitions();
			for (int i = 0; i < definitions.size(); i++) {
				if (definitions[i].get_type() != Variant::DICTIONARY) {
					continue;
				}
				const Dictionary definition_dict = definitions[i];
				AIModelToolDefinition definition;
				definition.name = definition_dict.get("name", String());
				definition.description = definition_dict.get("description", String());
				definition.input_schema = definition_dict.get("input_schema", Dictionary());
				definition.metadata = definition_dict.get("metadata", Dictionary());
				if (definition.is_valid()) {
					r_request.tools.push_back(definition);
				}
			}
		}
	}

	const String provider = config_service->get_default_provider();
	const Dictionary provider_config = config_service->get_provider_config(provider);
	const String model = String(provider_config.get("model", config_service->get_default_model())).strip_edges();
	const Array system = config_service->get_system_prompt(p_agent_id);

	AIContextEpoch epoch;
	if (!_ensure_context_epoch(p_session.id, p_agent_id, system, provider, model, epoch, r_error)) {
		return false;
	}

	r_request.request_id = AIId::make("request");
	r_request.provider = provider.is_empty() ? String("fake") : provider;
	r_request.model = model.is_empty() ? String("fake-model") : model;
	r_request.system = _system_parts_from_array(system);
	r_request.provider_options = provider_config.duplicate(true);
	r_request.metadata["session_id"] = p_session.id;
	r_request.metadata["wake_seq"] = p_wake_seq;
	r_request.metadata["context_epoch"] = epoch.to_dictionary();

	const Vector<AISessionMessage> runner_messages = AISessionHistory::entries_for_runner(projector->get_messages_struct(p_session.id), epoch.baseline_seq);
	for (int i = 0; i < runner_messages.size(); i++) {
		r_request.messages.push_back(_message_to_model(runner_messages[i]));
	}
	return true;
}

bool AISessionRunner::_settle_open_tool_calls(const AISessionRow &p_session, const String &p_agent_id, const String &p_root_dir, const Array &p_permission_rules, const Ref<AICancelToken> &p_cancel_token, bool &r_needs_continuation, bool &r_waiting_permission, AIError &r_error) {
	r_needs_continuation = false;
	r_waiting_permission = false;
	if (event_store.is_null() || tool_registry.is_null()) {
		r_error = AIError::none();
		return true;
	}

	HashMap<String, Dictionary> open_calls;
	const Vector<AIEventRow> rows = event_store->list(p_session.id, 0, false);
	for (int i = 0; i < rows.size(); i++) {
		const AIEventRow &row = rows[i];
		if (row.type == AIDomainEventTypes::tool_called()) {
			const String key = _ai_session_runner_tool_key(row.data);
			if (!key.ends_with("|")) {
				open_calls[key] = row.data.duplicate(true);
			}
		} else if (row.type == AIDomainEventTypes::tool_success() || row.type == AIDomainEventTypes::tool_failed()) {
			const String key = _ai_session_runner_tool_key(row.data);
			if (!key.ends_with("|")) {
				open_calls.erase(key);
			}
		}
	}
	if (open_calls.is_empty()) {
		r_error = AIError::none();
		return true;
	}

	if (tool_registry->get_permission_service().is_valid()) {
		tool_registry->get_permission_service()->set_rules(p_permission_rules);
	}
	Ref<AIV1ToolMaterialization> materialization = tool_registry->materialize_struct(p_root_dir, p_permission_rules);
	for (const KeyValue<String, Dictionary> &kv : open_calls) {
		if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
			r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("Session run interrupted."));
			return false;
		}

		const Dictionary data = kv.value;
		Dictionary call;
		call["id"] = data.get("call_id", data.get("callID", String()));
		call["name"] = data.get("tool", data.get("name", String()));
		call["input"] = data.get("input", Variant());
		call["provider_executed"] = bool(data.get("provider_executed", data.get("providerExecuted", false)));
		if (data.get("registration_identity", Variant()).get_type() == Variant::DICTIONARY) {
			call["registration_identity"] = data["registration_identity"];
		}

		Dictionary settle_input;
		settle_input["session_id"] = p_session.id;
		settle_input["agent"] = p_agent_id;
		settle_input["assistant_message_id"] = data.get("assistant_message_id", data.get("assistantMessageID", String()));
		settle_input["root_dir"] = p_root_dir;
		if (call.has("registration_identity")) {
			settle_input["registration_identity"] = call["registration_identity"];
		}
		settle_input["call"] = call;

		AIV1ToolSettlement settlement;
		AIError settlement_error;
		if (materialization.is_null() || !materialization->settle_struct(settle_input, settlement, settlement_error)) {
			r_error = settlement_error.is_error() ? settlement_error : AIError::make(AI_ERROR_INTERNAL, "Open tool settlement failed.");
			return false;
		}
		if (settlement.pending) {
			r_waiting_permission = true;
			continue;
		}
		r_needs_continuation = r_needs_continuation || settlement.needs_continuation;
	}

	r_error = AIError::none();
	return true;
}

bool AISessionRunner::_run_provider_turn(const String &p_session_id, const String &p_agent_id, const AIModelRequest &p_request, const Ref<AIV1ToolMaterialization> &p_tool_materialization, const Ref<AICancelToken> &p_cancel_token, bool &r_needs_continuation, bool &r_waiting_permission, AIError &r_error) {
	r_needs_continuation = false;
	r_waiting_permission = false;
	Ref<AILLMRuntime> runtime;
	if (!runtime_registry->get_runtime_struct(p_request.provider, runtime)) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "No LLM runtime registered for provider: " + p_request.provider);
		return false;
	}

	const String assistant_message_id = AIId::make("assistant");
	Dictionary started;
	started["assistant_message_id"] = assistant_message_id;
	started["request_id"] = p_request.request_id;
	started["provider"] = p_request.provider;
	started["model"] = p_request.model;
	started["request"] = p_request.to_dictionary();

	AIEventRow start_row;
	if (!_append_event(p_session_id, AIDomainEventTypes::step_started(), started, false, start_row, r_error)) {
		return false;
	}

	Ref<AISessionRunnerEventRecorder> recorder;
	recorder.instantiate();
	recorder->setup(event_store, projector, p_tool_materialization, p_session_id, p_agent_id, assistant_message_id);

	Ref<AICallableStreamSink> sink;
	sink.instantiate();
	sink->set_callback(callable_mp(recorder.ptr(), &AISessionRunnerEventRecorder::handle_event));

	AIError runtime_error;
	const bool ok = runtime->stream_struct(p_request, sink, p_cancel_token, runtime_error);
	const AIError recorder_error = recorder->get_error();
	if (!ok || recorder_error.is_error()) {
		const AIError provider_error = runtime_error.is_error() ? runtime_error : recorder_error;
		r_error = provider_error;
		Dictionary failed;
		failed["assistant_message_id"] = assistant_message_id;
		failed["request_id"] = p_request.request_id;
		failed["provider"] = p_request.provider;
		failed["model"] = p_request.model;
		failed["error"] = provider_error.to_dictionary();
		AIEventRow failed_row;
		AIError append_error;
		if (!_append_event(p_session_id, AIDomainEventTypes::step_failed(), failed, false, failed_row, append_error)) {
			r_error = append_error;
		}
		return false;
	}
	r_needs_continuation = recorder->has_local_tool_settlement();
	r_waiting_permission = recorder->is_waiting_permission();

	if (!recorder->get_reasoning().is_empty()) {
		Dictionary reasoning;
		reasoning["assistant_message_id"] = assistant_message_id;
		reasoning["content_id"] = recorder->get_reasoning_content_id();
		reasoning["text"] = recorder->get_reasoning();
		AIEventRow reasoning_row;
		if (!_append_event(p_session_id, AIDomainEventTypes::reasoning_ended(), reasoning, false, reasoning_row, r_error)) {
			return false;
		}
	}

	if (!recorder->get_text().is_empty()) {
		Dictionary text;
		text["assistant_message_id"] = assistant_message_id;
		text["content_id"] = recorder->get_text_content_id();
		text["text"] = recorder->get_text();
		AIEventRow text_row;
		if (!_append_event(p_session_id, AIDomainEventTypes::text_ended(), text, false, text_row, r_error)) {
			return false;
		}
	}

	Dictionary ended;
	ended["assistant_message_id"] = assistant_message_id;
	ended["request_id"] = p_request.request_id;
	ended["provider"] = p_request.provider;
	ended["model"] = p_request.model;
	AIEventRow end_row;
	if (!_append_event(p_session_id, AIDomainEventTypes::step_ended(), ended, false, end_row, r_error)) {
		return false;
	}
	return true;
}

void AISessionRunner::set_prompt_promoter(const Ref<AIPromptPromoter> &p_prompt_promoter) {
	prompt_promoter = p_prompt_promoter;
}

Ref<AIPromptPromoter> AISessionRunner::get_prompt_promoter() const {
	return prompt_promoter;
}

void AISessionRunner::set_event_store(const Ref<AIEventStore> &p_event_store) {
	event_store = p_event_store;
	if (tool_registry.is_valid()) {
		tool_registry->set_event_store(event_store);
	}
}

Ref<AIEventStore> AISessionRunner::get_event_store() const {
	return event_store;
}

void AISessionRunner::set_projector(const Ref<AISessionProjector> &p_projector) {
	projector = p_projector;
	if (tool_registry.is_valid()) {
		tool_registry->set_projector(projector);
	}
}

Ref<AISessionProjector> AISessionRunner::get_projector() const {
	return projector;
}

void AISessionRunner::set_context_epoch_store(const Ref<AIContextEpochStore> &p_store) {
	context_epoch_store = p_store;
}

Ref<AIContextEpochStore> AISessionRunner::get_context_epoch_store() const {
	return context_epoch_store;
}

void AISessionRunner::set_config_service(const Ref<AIConfigService> &p_config_service) {
	config_service = p_config_service;
}

Ref<AIConfigService> AISessionRunner::get_config_service() const {
	return config_service;
}

void AISessionRunner::set_runtime_registry(const Ref<AILLMRuntimeRegistry> &p_registry) {
	runtime_registry = p_registry;
}

Ref<AILLMRuntimeRegistry> AISessionRunner::get_runtime_registry() const {
	return runtime_registry;
}

void AISessionRunner::set_tool_registry(const Ref<AIV1ToolRegistry> &p_registry) {
	tool_registry = p_registry;
	if (tool_registry.is_valid()) {
		tool_registry->set_event_store(event_store);
		tool_registry->set_projector(projector);
	}
}

Ref<AIV1ToolRegistry> AISessionRunner::get_tool_registry() const {
	return tool_registry;
}

void AISessionRunner::set_session_store(const Ref<AISessionStore> &p_store) {
	session_store = p_store;
}

Ref<AISessionStore> AISessionRunner::get_session_store() const {
	return session_store;
}

bool AISessionRunner::_drain_struct_internal(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, bool p_force, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session id is required to run.");
		return false;
	}
	if (prompt_promoter.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no PromptPromoter.");
		return false;
	}
	if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
		r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("Session run interrupted."));
		return false;
	}

	AISessionRow session;
	if (!_resolve_session(session_id, session, r_error)) {
		return false;
	}
	const String agent_id = session.agent_id.strip_edges().is_empty() ? String("main") : session.agent_id.strip_edges();
	const String root_dir = session.location.directory.strip_edges();

	if (!prompt_promoter->promote_eligible_struct(session_id, "new-activity", r_promoted, r_error)) {
		return false;
	}

	Dictionary config = config_service.is_valid() ? config_service->get_config() : Dictionary();
	if (!bool(config.get("success", true))) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "ConfigService failed to provide config.", config.get("error", Dictionary()));
		return false;
	}
	const Dictionary permissions = config.get("permissions", Dictionary());
	const Array rules = permissions.get("rules", Array());

	bool settled_open_tools = false;
	bool waiting_permission = false;
	if (!_settle_open_tool_calls(session, agent_id, root_dir, rules, p_cancel_token, settled_open_tools, waiting_permission, r_error)) {
		return false;
	}
	if (waiting_permission) {
		r_error = AIError::none();
		return true;
	}
	if (r_promoted.is_empty() && !settled_open_tools && !p_force) {
		r_error = AIError::none();
		return true;
	}

	static const int MAX_PROVIDER_TURNS = 25;
	for (int turn = 0; turn < MAX_PROVIDER_TURNS; turn++) {
		if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
			r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("Session run interrupted."));
			return false;
		}

		AIModelRequest request;
		Ref<AIV1ToolMaterialization> materialization;
		if (!_build_request(session, agent_id, root_dir, p_wake_seq, request, materialization, r_error)) {
			return false;
		}

		bool needs_continuation = false;
		bool provider_waiting_permission = false;
		if (!_run_provider_turn(session_id, agent_id, request, materialization, p_cancel_token, needs_continuation, provider_waiting_permission, r_error)) {
			return false;
		}
		if (provider_waiting_permission) {
			return true;
		}
		if (!needs_continuation) {
			return true;
		}
	}

	r_error = AIError::make(AI_ERROR_CONFLICT, "Session runner reached the provider turn limit.");
	return false;
}

bool AISessionRunner::drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) {
	return _drain_struct_internal(p_session_id, p_cancel_token, p_wake_seq, false, r_promoted, r_error);
}

Dictionary AISessionRunner::run(const String &p_session_id, bool p_force) {
	Ref<AICancelToken> cancel_token;
	cancel_token.instantiate();

	Vector<AISessionInputRecord> promoted;
	AIError error;
	if (!_drain_struct_internal(p_session_id, cancel_token, 0, p_force, promoted, error)) {
		return _make_error_result(error);
	}

	Array items;
	for (int i = 0; i < promoted.size(); i++) {
		items.push_back(promoted[i].to_dictionary());
	}

	Dictionary result;
	result["success"] = true;
	result["promoted"] = items;
	return result;
}
