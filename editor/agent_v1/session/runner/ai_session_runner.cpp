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
	String session_id;
	String assistant_message_id;
	String text_content_id;
	String reasoning_content_id;
	String text;
	String reasoning;
	AIError error;

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
	void setup(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const String &p_session_id, const String &p_assistant_message_id) {
		event_store = p_event_store;
		projector = p_projector;
		session_id = p_session_id;
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
			Dictionary data;
			data["assistant_message_id"] = assistant_message_id;
			data["call_id"] = p_event.get("id", AIId::make("call"));
			data["tool"] = p_event.get("name", String());
			data["name"] = p_event.get("name", String());
			data["input"] = p_event.get("input", Variant());
			data["provider"] = p_event.get("provider_metadata", Dictionary());
			return !_append_event(AIDomainEventTypes::tool_called(), data, false);
		}
		if (type == "provider-error") {
			error = AIError::make(AI_ERROR_PROVIDER, "Provider stream error.", p_event.get("error", Dictionary()));
			return true;
		}
		return false;
	}
};

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
	ClassDB::bind_method(D_METHOD("run", "session_id", "force"), &AISessionRunner::run, DEFVAL(false));
}

AISessionRunner::AISessionRunner() {
	context_epoch_store.instantiate();
	config_service.instantiate();
	runtime_registry.instantiate();
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

bool AISessionRunner::_build_request(const String &p_session_id, const String &p_agent_id, int64_t p_wake_seq, AIModelRequest &r_request, AIError &r_error) {
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

	const String provider = config_service->get_default_provider();
	const Dictionary provider_config = config_service->get_provider_config(provider);
	const String model = String(provider_config.get("model", config_service->get_default_model())).strip_edges();
	const Array system = config_service->get_system_prompt(p_agent_id);

	AIContextEpoch epoch;
	if (!_ensure_context_epoch(p_session_id, p_agent_id, system, provider, model, epoch, r_error)) {
		return false;
	}

	r_request.request_id = AIId::make("request");
	r_request.provider = provider.is_empty() ? String("fake") : provider;
	r_request.model = model.is_empty() ? String("fake-model") : model;
	r_request.system = _system_parts_from_array(system);
	r_request.provider_options = provider_config.duplicate(true);
	r_request.metadata["session_id"] = p_session_id;
	r_request.metadata["wake_seq"] = p_wake_seq;
	r_request.metadata["context_epoch"] = epoch.to_dictionary();

	const Vector<AISessionMessage> runner_messages = AISessionHistory::entries_for_runner(projector->get_messages_struct(p_session_id), epoch.baseline_seq);
	for (int i = 0; i < runner_messages.size(); i++) {
		r_request.messages.push_back(_message_to_model(runner_messages[i]));
	}
	return true;
}

bool AISessionRunner::_run_provider_turn(const String &p_session_id, const AIModelRequest &p_request, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) {
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
	recorder->setup(event_store, projector, p_session_id, assistant_message_id);

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
}

Ref<AIEventStore> AISessionRunner::get_event_store() const {
	return event_store;
}

void AISessionRunner::set_projector(const Ref<AISessionProjector> &p_projector) {
	projector = p_projector;
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

bool AISessionRunner::drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) {
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

	if (projector.is_valid()) {
		projector->mark_pending_tools_interrupted(session_id);
	}

	if (!prompt_promoter->promote_eligible_struct(session_id, "new-activity", r_promoted, r_error)) {
		return false;
	}
	if (r_promoted.is_empty()) {
		r_error = AIError::none();
		return true;
	}

	AIModelRequest request;
	if (!_build_request(session_id, "main", p_wake_seq, request, r_error)) {
		return false;
	}
	return _run_provider_turn(session_id, request, p_cancel_token, r_error);
}

Dictionary AISessionRunner::run(const String &p_session_id, bool p_force) {
	(void)p_force;
	Ref<AICancelToken> cancel_token;
	cancel_token.instantiate();

	Vector<AISessionInputRecord> promoted;
	AIError error;
	if (!drain_struct(p_session_id, cancel_token, 0, promoted, error)) {
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
