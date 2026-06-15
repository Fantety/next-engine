/**************************************************************************/
/*  ai_session_runner.cpp                                                 */
/**************************************************************************/

#include "ai_session_runner.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

static int64_t _ai_session_runner_token_count(const Dictionary &p_tokens, const String &p_key, const String &p_alias = String()) {
	if (p_tokens.has(p_key) && p_tokens[p_key].get_type() != Variant::NIL) {
		return MAX(int64_t(0), int64_t(p_tokens[p_key]));
	}
	if (!p_alias.is_empty() && p_tokens.has(p_alias) && p_tokens[p_alias].get_type() != Variant::NIL) {
		return MAX(int64_t(0), int64_t(p_tokens[p_alias]));
	}
	return 0;
}

static Dictionary _ai_session_runner_normalize_token_usage(const Dictionary &p_tokens) {
	const int64_t input_tokens = _ai_session_runner_token_count(p_tokens, "input_tokens", "input");
	const int64_t output_tokens = _ai_session_runner_token_count(p_tokens, "output_tokens", "output");
	const int64_t cache_read_tokens = _ai_session_runner_token_count(p_tokens, "cache_read_tokens", "cache_read");
	const int64_t cache_write_tokens = _ai_session_runner_token_count(p_tokens, "cache_write_tokens", "cache_write");
	const int64_t computed_total_tokens = input_tokens + output_tokens + cache_read_tokens + cache_write_tokens;
	const bool has_total_tokens = p_tokens.has("total_tokens") || p_tokens.has("total");
	const int64_t total_tokens = has_total_tokens ? _ai_session_runner_token_count(p_tokens, "total_tokens", "total") : computed_total_tokens;

	Dictionary usage;
	usage["input_tokens"] = input_tokens;
	usage["output_tokens"] = output_tokens;
	usage["cache_read_tokens"] = cache_read_tokens;
	usage["cache_write_tokens"] = cache_write_tokens;
	usage["total_tokens"] = total_tokens;
	return usage;
}

static Dictionary _ai_session_runner_add_token_usage(const Dictionary &p_existing, const Dictionary &p_delta) {
	const Dictionary existing = _ai_session_runner_normalize_token_usage(p_existing);
	const Dictionary delta = _ai_session_runner_normalize_token_usage(p_delta);

	Dictionary tokens;
	tokens["input_tokens"] = _ai_session_runner_token_count(existing, "input_tokens") + _ai_session_runner_token_count(delta, "input_tokens");
	tokens["output_tokens"] = _ai_session_runner_token_count(existing, "output_tokens") + _ai_session_runner_token_count(delta, "output_tokens");
	tokens["cache_read_tokens"] = _ai_session_runner_token_count(existing, "cache_read_tokens") + _ai_session_runner_token_count(delta, "cache_read_tokens");
	tokens["cache_write_tokens"] = _ai_session_runner_token_count(existing, "cache_write_tokens") + _ai_session_runner_token_count(delta, "cache_write_tokens");
	tokens["total_tokens"] = _ai_session_runner_token_count(existing, "total_tokens") + _ai_session_runner_token_count(delta, "total_tokens");
	return tokens;
}

static bool _ai_session_runner_merge_session_token_usage(const Ref<AISessionStore> &p_session_store, const String &p_session_id, const Dictionary &p_usage, AIError &r_error) {
	if (p_usage.is_empty() || p_session_store.is_null()) {
		r_error = AIError::none();
		return true;
	}

	AISessionRow session;
	if (!p_session_store->get_session_struct(p_session_id, session)) {
		Dictionary details;
		details["session_id"] = p_session_id;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Session not found while updating token usage.", details);
		return false;
	}

	Dictionary metadata = session.metadata.duplicate(true);
	Dictionary existing_tokens;
	if (metadata.get("tokens", Variant()).get_type() == Variant::DICTIONARY) {
		existing_tokens = Dictionary(metadata["tokens"]);
	}
	metadata["tokens"] = _ai_session_runner_add_token_usage(existing_tokens, p_usage);

	AISessionRow updated_session;
	String store_error;
	if (!p_session_store->update_metadata_struct(p_session_id, metadata, updated_session, store_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, store_error);
		return false;
	}

	r_error = AIError::none();
	return true;
}

class AISessionRunnerEventRecorder : public RefCounted {
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;
	Ref<AIV1ToolMaterialization> tool_materialization;
	Ref<AICancelToken> cancel_token;
	String session_id;
	String agent_id;
	String assistant_message_id;
	String text_content_id;
	String reasoning_content_id;
	String text;
	String reasoning;
	Dictionary usage;
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
		if (projector.is_valid()) {
			projector->project(row);
		}
		return true;
	}

public:
	void setup(const Ref<AIEventStore> &p_event_store, const Ref<AISessionProjector> &p_projector, const Ref<AIV1ToolMaterialization> &p_tool_materialization, const Ref<AICancelToken> &p_cancel_token, const String &p_session_id, const String &p_agent_id, const String &p_assistant_message_id) {
		event_store = p_event_store;
		projector = p_projector;
		tool_materialization = p_tool_materialization;
		cancel_token = p_cancel_token;
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

	Dictionary get_usage() const {
		return usage.duplicate(true);
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

			needs_continuation = true;
			return false;
		}
		if (type == "provider-error") {
			error = AIError::make(AI_ERROR_PROVIDER, "Provider stream error.", p_event.get("error", Dictionary()));
			return true;
		}
		if (type == "usage") {
			if (p_event.get("usage", Variant()).get_type() == Variant::DICTIONARY) {
				usage = _ai_session_runner_normalize_token_usage(Dictionary(p_event["usage"]));
			}
			return false;
		}
		return false;
	}
};

static String _ai_session_runner_tool_key(const Dictionary &p_data) {
	const String assistant_message_id = String(p_data.get("assistant_message_id", p_data.get("assistantMessageID", String()))).strip_edges();
	const String call_id = String(p_data.get("call_id", p_data.get("callID", p_data.get("id", String())))).strip_edges();
	return assistant_message_id + "|" + call_id;
}

static Dictionary _ai_session_runner_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

static Variant _ai_session_runner_redact_sensitive_variant(const Variant &p_value, const String &p_key = String());

static bool _ai_session_runner_key_is_sensitive(const String &p_key) {
	const String key = p_key.strip_edges().to_lower();
	return key.contains("api_key") || key.contains("apikey") || key.contains("authorization") || key.contains("access_token") || key.contains("refresh_token") || key.contains("secret") || key.contains("password") || key.contains("credential");
}

static Dictionary _ai_session_runner_redact_sensitive_dictionary(const Dictionary &p_dict) {
	Dictionary redacted;
	const Array keys = p_dict.keys();
	for (int i = 0; i < keys.size(); i++) {
		const Variant key = keys[i];
		redacted[key] = _ai_session_runner_redact_sensitive_variant(p_dict[key], String(key));
	}
	return redacted;
}

static Array _ai_session_runner_redact_sensitive_array(const Array &p_array) {
	Array redacted;
	for (int i = 0; i < p_array.size(); i++) {
		redacted.push_back(_ai_session_runner_redact_sensitive_variant(p_array[i]));
	}
	return redacted;
}

static Variant _ai_session_runner_redact_sensitive_variant(const Variant &p_value, const String &p_key) {
	if (_ai_session_runner_key_is_sensitive(p_key)) {
		return String("[redacted]");
	}
	if (p_value.get_type() == Variant::DICTIONARY) {
		return _ai_session_runner_redact_sensitive_dictionary(p_value);
	}
	if (p_value.get_type() == Variant::ARRAY) {
		return _ai_session_runner_redact_sensitive_array(p_value);
	}
	if (p_value.get_type() == Variant::STRING) {
		const String value = String(p_value);
		const String lower = value.strip_edges().to_lower();
		if (lower.begins_with("authorization:") || lower.begins_with("x-api-key:")) {
			return String("[redacted]");
		}
	}
	return p_value;
}

static Array _ai_session_runner_config_sources_for_request(const Ref<AIConfigService> &p_config_service) {
	Array result;
	if (p_config_service.is_null()) {
		return result;
	}

	const Array entries = p_config_service->entries();
	for (int i = 0; i < entries.size(); i++) {
		if (entries[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary entry = entries[i];
		Dictionary source;
		source["source"] = entry.get("source", String());
		source["path"] = entry.get("path", String());
		source["priority"] = entry.get("priority", 0);
		result.push_back(source);
	}
	return result;
}

static bool _ai_session_runner_same_location(const AILocationRef &p_left, const AILocationRef &p_right) {
	return p_left.directory.strip_edges() == p_right.directory.strip_edges() && p_left.workspace_id.strip_edges() == p_right.workspace_id.strip_edges();
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
	ClassDB::bind_method(D_METHOD("set_context_source_registry", "registry"), &AISessionRunner::set_context_source_registry);
	ClassDB::bind_method(D_METHOD("get_context_source_registry"), &AISessionRunner::get_context_source_registry);
	ClassDB::bind_method(D_METHOD("set_context_epoch_service", "service"), &AISessionRunner::set_context_epoch_service);
	ClassDB::bind_method(D_METHOD("get_context_epoch_service"), &AISessionRunner::get_context_epoch_service);
	ClassDB::bind_method(D_METHOD("set_config_service", "config_service"), &AISessionRunner::set_config_service);
	ClassDB::bind_method(D_METHOD("get_config_service"), &AISessionRunner::get_config_service);
	ClassDB::bind_method(D_METHOD("set_runtime_registry", "registry"), &AISessionRunner::set_runtime_registry);
	ClassDB::bind_method(D_METHOD("get_runtime_registry"), &AISessionRunner::get_runtime_registry);
	ClassDB::bind_method(D_METHOD("set_tool_registry", "registry"), &AISessionRunner::set_tool_registry);
	ClassDB::bind_method(D_METHOD("get_tool_registry"), &AISessionRunner::get_tool_registry);
	ClassDB::bind_method(D_METHOD("set_session_store", "store"), &AISessionRunner::set_session_store);
	ClassDB::bind_method(D_METHOD("get_session_store"), &AISessionRunner::get_session_store);
	ClassDB::bind_method(D_METHOD("set_compaction_service", "service"), &AISessionRunner::set_compaction_service);
	ClassDB::bind_method(D_METHOD("get_compaction_service"), &AISessionRunner::get_compaction_service);
	ClassDB::bind_method(D_METHOD("set_model_part_builder", "builder"), &AISessionRunner::set_model_part_builder);
	ClassDB::bind_method(D_METHOD("get_model_part_builder"), &AISessionRunner::get_model_part_builder);
	ClassDB::bind_method(D_METHOD("set_skill_service", "service"), &AISessionRunner::set_skill_service);
	ClassDB::bind_method(D_METHOD("get_skill_service"), &AISessionRunner::get_skill_service);
	ClassDB::bind_method(D_METHOD("set_agent_service", "service"), &AISessionRunner::set_agent_service);
	ClassDB::bind_method(D_METHOD("get_agent_service"), &AISessionRunner::get_agent_service);
	ClassDB::bind_method(D_METHOD("run", "session_id", "force"), &AISessionRunner::run, DEFVAL(false));
}

AISessionRunner::AISessionRunner() {
	context_epoch_store.instantiate();
	context_source_registry.instantiate();
	context_epoch_service.instantiate();
	config_service.instantiate();
	runtime_registry.instantiate();
	tool_registry.instantiate();
	compaction_service.instantiate();
	model_part_builder.instantiate();
	skill_service.instantiate();
	agent_service.instantiate();
	tool_registry->register_builtin_tools();
	context_source_registry->set_config_service(config_service);
	context_epoch_service->set_epoch_store(context_epoch_store);
	compaction_service->set_projector(projector);
	compaction_service->set_context_source_registry(context_source_registry);
	compaction_service->set_context_epoch_service(context_epoch_service);
	skill_service->set_tool_registry(tool_registry);
	agent_service->set_config_service(config_service);
}

String AISessionRunner::_message_text(const AISessionMessage &p_message) {
	if (!p_message.text.is_empty()) {
		return p_message.text;
	}
	String text;
	for (int i = 0; i < p_message.content.size(); i++) {
		const AIAssistantContent &content = p_message.content[i];
		if (content.type == "text") {
			text += text.is_empty() ? content.text : "\n" + content.text;
		}
	}
	return text;
}

String AISessionRunner::_assistant_model_text(const AISessionMessage &p_message) {
	return _message_text(p_message);
}

String AISessionRunner::_tool_result_text(const AIAssistantContent &p_content) {
	const String tool_name = p_content.name.is_empty() ? String("tool") : p_content.name;
	if (p_content.tool_state.status == AI_TOOL_STATUS_FAILED) {
		Dictionary payload;
		payload["tool"] = tool_name;
		payload["status"] = "failed";
		payload["error"] = p_content.tool_state.error.to_dictionary();
		return JSON::stringify(payload);
	}

	Variant output = p_content.tool_state.output;
	if (output.get_type() == Variant::NIL) {
		output = p_content.tool_state.result;
	}
	if (output.get_type() == Variant::NIL) {
		return String();
	}
	if (output.get_type() == Variant::STRING) {
		return String(output);
	}
	return JSON::stringify(output);
}

String AISessionRunner::_latest_user_prompt_text(const Vector<AISessionMessage> &p_messages) {
	for (int i = p_messages.size() - 1; i >= 0; i--) {
		if (p_messages[i].type == AI_SESSION_MESSAGE_USER) {
			return _message_text(p_messages[i]);
		}
	}
	return String();
}

bool AISessionRunner::_append_message_to_model_messages(const AISessionMessage &p_message, const Dictionary &p_provider_config, Vector<AIModelMessage> &r_messages, AIError &r_error) {
	String role = "user";
	if (p_message.type == AI_SESSION_MESSAGE_ASSISTANT) {
		role = "assistant";
	} else if (p_message.type == AI_SESSION_MESSAGE_SYSTEM || p_message.type == AI_SESSION_MESSAGE_COMPACTION) {
		role = "system";
	}

	if (p_message.type == AI_SESSION_MESSAGE_ASSISTANT) {
		AIModelMessage assistant_message;
		assistant_message.id = p_message.id;
		assistant_message.role = "assistant";
		const String text = _assistant_model_text(p_message);
		if (!text.is_empty()) {
			assistant_message.parts.push_back(AIModelPart::text_part(text));
		}

		Vector<AIModelMessage> tool_result_messages;
		for (int i = 0; i < p_message.content.size(); i++) {
			const AIAssistantContent &content = p_message.content[i];
			if (content.type != "tool") {
				continue;
			}
			if (content.tool_state.status != AI_TOOL_STATUS_SUCCESS && content.tool_state.status != AI_TOOL_STATUS_FAILED) {
				continue;
			}
			if (content.id.strip_edges().is_empty() || content.name.strip_edges().is_empty()) {
				continue;
			}

			AIModelToolCall tool_call;
			tool_call.id = content.id;
			tool_call.name = content.name;
			tool_call.input = content.tool_state.input;
			tool_call.provider_metadata = content.provider_metadata.duplicate(true);
			if (tool_call.provider_metadata.is_empty() && !content.tool_state.provider.is_empty()) {
				tool_call.provider_metadata = content.tool_state.provider.duplicate(true);
			}
			assistant_message.tool_calls.push_back(tool_call);

			AIModelMessage tool_result = AIModelMessage::tool_result_message(content.id, content.name, _tool_result_text(content), content.id + "_result");
			tool_result_messages.push_back(tool_result);
		}

		if (!assistant_message.parts.is_empty() || !assistant_message.tool_calls.is_empty()) {
			r_messages.push_back(assistant_message);
		}
		for (int i = 0; i < tool_result_messages.size(); i++) {
			r_messages.push_back(tool_result_messages[i]);
		}
		r_error = AIError::none();
		return true;
	}

	String text = _message_text(p_message);
	if (p_message.type == AI_SESSION_MESSAGE_COMPACTION && !text.is_empty()) {
		text = "Compaction summary:\n" + text;
	}

	AIModelMessage message;
	message.id = p_message.id;
	message.role = role;
	if (!text.is_empty() || p_message.files.is_empty()) {
		message.parts.push_back(AIModelPart::text_part(text));
	}
	if (!p_message.files.is_empty()) {
		if (model_part_builder.is_null()) {
			r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no ModelPartBuilder.");
			return false;
		}
		if (!model_part_builder->append_attachment_parts_struct(p_message.files, p_provider_config, message.parts, r_error)) {
			return false;
		}
	}

	r_messages.push_back(message);
	r_error = AIError::none();
	return true;
}

Vector<AIModelPart> AISessionRunner::_system_parts_from_baseline(const String &p_baseline) {
	Vector<AIModelPart> result;
	const String baseline = p_baseline.strip_edges();
	if (!baseline.is_empty()) {
		result.push_back(AIModelPart::text_part(baseline));
	}
	return result;
}

int64_t AISessionRunner::_history_token_budget_from_config(const Dictionary &p_config) {
	const Dictionary history = _ai_session_runner_dictionary_from_variant(p_config.get("history", Dictionary()));
	int64_t budget = int64_t(history.get("max_tokens", history.get("maxTokens", 0)));
	if (budget > 0) {
		return budget;
	}

	const Dictionary context = _ai_session_runner_dictionary_from_variant(p_config.get("context", Dictionary()));
	budget = int64_t(context.get("history_token_budget", context.get("historyTokenBudget", 0)));
	return budget > 0 ? budget : 0;
}

bool AISessionRunner::_is_rebuild_request_conflict(const AIError &p_error) {
	return p_error.kind == AI_ERROR_CONFLICT && bool(p_error.details.get("rebuild_request", false));
}

AIError AISessionRunner::_error_from_result(const Dictionary &p_result, const String &p_fallback_message) {
	if (p_result.get("error", Variant()).get_type() != Variant::DICTIONARY) {
		return AIError::make(AI_ERROR_INTERNAL, p_fallback_message);
	}

	const Dictionary error = p_result["error"];
	Dictionary details;
	if (error.get("details", Variant()).get_type() == Variant::DICTIONARY) {
		details = Dictionary(error["details"]).duplicate(true);
	}
	const String kind = String(error.get("kind", String())).strip_edges();
	const String message = String(error.get("message", p_fallback_message)).strip_edges();
	return AIError::make(AIError::string_to_kind(kind), message.is_empty() ? p_fallback_message : message, details);
}

Array AISessionRunner::_permission_rules_from_config(const Dictionary &p_config) {
	if (p_config.get("permissions", Variant()).get_type() != Variant::DICTIONARY) {
		return Array();
	}
	const Dictionary permissions = p_config["permissions"];
	if (permissions.get("rules", Variant()).get_type() != Variant::ARRAY) {
		return Array();
	}
	return Array(permissions["rules"]).duplicate(true);
}

Dictionary AISessionRunner::_model_part_for_event_log(const AIModelPart &p_part) {
	Dictionary part = p_part.to_dictionary();
	if (p_part.type == AI_MODEL_PART_TEXT && bool(p_part.metadata.get("derived_from_attachment", false)) && !p_part.text.is_empty()) {
		part["text"] = "[redacted attachment text]";
		Dictionary metadata = part.get("metadata", Dictionary());
		metadata["text_redacted"] = true;
		metadata["text_length"] = p_part.text.length();
		part["metadata"] = metadata;
	}
	if (p_part.type != AI_MODEL_PART_TEXT && !p_part.data.is_empty()) {
		part["data"] = "[redacted attachment data]";
		Dictionary metadata = part.get("metadata", Dictionary());
		metadata["data_redacted"] = true;
		metadata["data_length"] = p_part.data.length();
		part["metadata"] = metadata;
	}
	return part;
}

Dictionary AISessionRunner::_model_message_for_event_log(const AIModelMessage &p_message) {
	Dictionary message;
	message["id"] = p_message.id;
	message["role"] = p_message.role;
	message["tool_call_id"] = p_message.tool_call_id;
	message["name"] = p_message.name;
	Array parts;
	for (int i = 0; i < p_message.parts.size(); i++) {
		parts.push_back(_model_part_for_event_log(p_message.parts[i]));
	}
	message["parts"] = parts;
	Array tool_calls;
	for (int i = 0; i < p_message.tool_calls.size(); i++) {
		tool_calls.push_back(p_message.tool_calls[i].to_dictionary());
	}
	message["tool_calls"] = tool_calls;
	message["metadata"] = p_message.metadata;
	return message;
}

Dictionary AISessionRunner::_request_for_event_log(const AIModelRequest &p_request) {
	Dictionary request;
	request["request_id"] = p_request.request_id;
	request["provider"] = p_request.provider;
	request["model"] = p_request.model;
	request["provider_options"] = _ai_session_runner_redact_sensitive_dictionary(p_request.provider_options);
	request["metadata"] = _ai_session_runner_redact_sensitive_dictionary(p_request.metadata);
	request["max_output_tokens"] = p_request.max_output_tokens;
	request["stream"] = p_request.stream;

	Array system;
	for (int i = 0; i < p_request.system.size(); i++) {
		system.push_back(_model_part_for_event_log(p_request.system[i]));
	}
	request["system"] = system;

	Array messages;
	for (int i = 0; i < p_request.messages.size(); i++) {
		messages.push_back(_model_message_for_event_log(p_request.messages[i]));
	}
	request["messages"] = messages;

	Array tools;
	for (int i = 0; i < p_request.tools.size(); i++) {
		tools.push_back(p_request.tools[i].to_dictionary());
	}
	request["tools"] = tools;
	return request;
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

bool AISessionRunner::_resolve_agent_for_session(const AISessionRow &p_session, AIAgentConfig &r_agent, AIError &r_error) const {
	if (agent_service.is_valid()) {
		if (agent_service->get_config_service().is_null()) {
			agent_service->set_config_service(config_service);
		}
		if (agent_service->get_session_store().is_null()) {
			agent_service->set_session_store(session_store);
		}
		if (!p_session.id.strip_edges().is_empty() && agent_service->get_session_store().is_valid()) {
			return agent_service->resolve_for_session_struct(p_session.id, r_agent, r_error);
		}
		return agent_service->resolve_struct(p_session.agent_id, r_agent, r_error);
	}

	if (config_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no AgentService or ConfigService.");
		return false;
	}
	const Dictionary config = config_service->get_config();
	if (!bool(config.get("success", true))) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "ConfigService failed to provide config.", config.get("error", Dictionary()));
		return false;
	}
	const Dictionary agents = _ai_session_runner_dictionary_from_variant(config.get("agents", Dictionary()));
	String agent_id = p_session.agent_id.strip_edges().is_empty() ? String(config.get("default_agent", "main")) : p_session.agent_id.strip_edges();
	if (agent_id.is_empty()) {
		agent_id = "main";
	}
	const Dictionary agent_dict = _ai_session_runner_dictionary_from_variant(agents.get(agent_id, Dictionary()));
	r_agent = AIAgentConfig::from_dictionary(agent_id, agent_dict, config_service->get_default_provider(), config_service->get_default_model());
	r_error = AIError::none();
	return true;
}

Array AISessionRunner::_permission_rules_for_agent(const Dictionary &p_config, const AIAgentConfig &p_agent) const {
	const Array base_rules = _permission_rules_from_config(p_config);
	if (agent_service.is_valid()) {
		return agent_service->permission_rules_for_agent(p_agent, base_rules);
	}
	return base_rules;
}

bool AISessionRunner::_project_durable_history(const String &p_session_id) {
	if (event_store.is_null() || projector.is_null()) {
		return false;
	}
	const int64_t after_seq = projector->get_projected_seq(p_session_id);
	projector->project_from_store(event_store, p_session_id, after_seq);
	return true;
}

bool AISessionRunner::_configure_skill_service_from_config(const Dictionary &p_config, AIError &r_error) const {
	if (skill_service.is_null()) {
		r_error = AIError::none();
		return true;
	}
	if (tool_registry.is_valid()) {
		skill_service->set_tool_registry(tool_registry);
	}
	if (!skill_service->import_config_struct(p_config, r_error)) {
		return false;
	}
	return skill_service->refresh_struct(r_error);
}

bool AISessionRunner::_select_skills_for_prompt(const Dictionary &p_config, const String &p_prompt, Array &r_selected_skills, AIError &r_error) const {
	r_selected_skills.clear();
	if (skill_service.is_null()) {
		r_error = AIError::none();
		return true;
	}
	if (!_configure_skill_service_from_config(p_config, r_error)) {
		return false;
	}

	r_selected_skills = skill_service->select(p_prompt, Array());
	r_error = AIError::none();
	return true;
}

bool AISessionRunner::_append_selected_skill_sources(const Array &p_selected_skills, Vector<AISystemContextSource> &r_sources, AIError &r_error) const {
	if (p_selected_skills.is_empty()) {
		r_error = AIError::none();
		return true;
	}
	if (skill_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no SkillService.");
		return false;
	}

	for (int i = 0; i < p_selected_skills.size(); i++) {
		if (p_selected_skills[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary selected = p_selected_skills[i];
		const String skill_id = String(selected.get("skill_id", selected.get("skillID", String()))).strip_edges();
		if (skill_id.is_empty()) {
			continue;
		}

		const Dictionary source_result = skill_service->make_context_source(skill_id, true, 150 + i);
		if (!bool(source_result.get("success", false))) {
			r_error = _error_from_result(source_result, "Failed to build selected Skill context source.");
			return false;
		}
		if (source_result.get("source", Variant()).get_type() != Variant::DICTIONARY) {
			r_error = AIError::make(AI_ERROR_INTERNAL, "Selected Skill did not produce a context source.");
			return false;
		}
		r_sources.push_back(AISystemContextSource::from_dictionary(source_result["source"]));
	}

	r_error = AIError::none();
	return true;
}

bool AISessionRunner::_load_system_context(const AISessionRow &p_session, const String &p_agent_id, const String &p_provider, const String &p_model, const Array &p_selected_skills, AISystemContext &r_context, AIError &r_error) const {
	if (context_source_registry.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no ContextSourceRegistry.");
		return false;
	}
	if (!context_source_registry->load_session_struct(p_session.id, p_agent_id, p_session.location, p_provider, p_model, r_context, r_error)) {
		return false;
	}
	if (!p_selected_skills.is_empty()) {
		Vector<AISystemContextSource> sources = r_context.sources;
		if (!_append_selected_skill_sources(p_selected_skills, sources, r_error)) {
			return false;
		}
		r_context = AISystemContext::combine(sources);
	}
	if (!r_context.is_available()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, r_context.blocked_reason.is_empty() ? String("System context is unavailable.") : r_context.blocked_reason);
		return false;
	}
	return true;
}

bool AISessionRunner::_prepare_context_epoch(const AISessionRow &p_session, const String &p_agent_id, const String &p_provider, const String &p_model, const Array &p_selected_skills, AIContextEpoch &r_epoch, AIError &r_error) {
	if (context_epoch_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no ContextEpochService.");
		return false;
	}

	AISystemContext context;
	if (!_load_system_context(p_session, p_agent_id, p_provider, p_model, p_selected_skills, context, r_error)) {
		return false;
	}
	return context_epoch_service->prepare_struct(p_session.id, p_session.location, p_agent_id, context, r_epoch, r_error);
}

bool AISessionRunner::_verify_context_epoch_current(const String &p_session_id, const String &p_agent_id, const AIModelRequest &p_request, AIError &r_error) const {
	if (context_epoch_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no ContextEpochService.");
		return false;
	}
	if (config_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no ConfigService.");
		return false;
	}
	if (p_request.metadata.get("context_epoch", Variant()).get_type() != Variant::DICTIONARY) {
		r_error = AIError::make(AI_ERROR_CONFLICT, "Model request has no context epoch fence.");
		return false;
	}

	const AIContextEpoch prepared_epoch = AIContextEpoch::from_dictionary(p_request.metadata["context_epoch"]);

	AISessionRow current_session;
	if (session_store.is_valid()) {
		if (!_resolve_session(p_session_id, current_session, r_error)) {
			return false;
		}
	} else {
		current_session.id = p_session_id;
		current_session.agent_id = p_agent_id;
		if (p_request.metadata.get("location", Variant()).get_type() == Variant::DICTIONARY) {
			current_session.location = AILocationRef::from_dictionary(p_request.metadata["location"]);
		}
	}

	AIAgentConfig agent_config;
	if (!_resolve_agent_for_session(current_session, agent_config, r_error)) {
		return false;
	}
	const String current_agent = agent_config.id.strip_edges().is_empty() ? (current_session.agent_id.strip_edges().is_empty() ? String("main") : current_session.agent_id.strip_edges()) : agent_config.id.strip_edges();
	const String prepared_agent = p_agent_id.strip_edges().is_empty() ? current_agent : p_agent_id.strip_edges();
	if (current_agent != prepared_agent) {
		Dictionary details;
		details["rebuild_request"] = true;
		details["expected_agent"] = prepared_agent;
		details["actual_agent"] = current_agent;
		r_error = AIError::make(AI_ERROR_CONFLICT, "Session agent changed before provider turn.", details);
		return false;
	}

	if (p_request.metadata.get("location", Variant()).get_type() == Variant::DICTIONARY) {
		const AILocationRef prepared_location = AILocationRef::from_dictionary(p_request.metadata["location"]);
		if (!_ai_session_runner_same_location(prepared_location, current_session.location)) {
			Dictionary details;
			details["rebuild_request"] = true;
			details["expected_location"] = prepared_location.to_dictionary();
			details["actual_location"] = current_session.location.to_dictionary();
			r_error = AIError::make(AI_ERROR_CONFLICT, "Session location changed before provider turn.", details);
			return false;
		}
	}

	const Dictionary config = config_service->get_config();
	if (!bool(config.get("success", true))) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "ConfigService failed to provide config.", config.get("error", Dictionary()));
		return false;
	}
	const String provider = agent_config.provider;
	const Dictionary provider_config = config_service->get_provider_config(provider);
	const String model = agent_config.model.strip_edges().is_empty() ? String(provider_config.get("model", config_service->get_default_model())).strip_edges() : agent_config.model.strip_edges();
	const String effective_provider = provider.is_empty() ? String("fake") : provider;
	const String effective_model = model.is_empty() ? String("fake-model") : model;
	const String prepared_provider = String(p_request.metadata.get("config_provider", p_request.provider)).strip_edges();
	const String prepared_model = String(p_request.metadata.get("config_model", p_request.model)).strip_edges();
	if (provider != prepared_provider || model != prepared_model || p_request.provider != effective_provider || p_request.model != effective_model) {
		Dictionary details;
		details["rebuild_request"] = true;
		details["expected_provider"] = prepared_provider;
		details["actual_provider"] = provider;
		details["expected_model"] = prepared_model;
		details["actual_model"] = model;
		r_error = AIError::make(AI_ERROR_CONFLICT, "Model selection changed before provider turn.", details);
		return false;
	}

	Array selected_skills;
	if (p_request.metadata.get("selected_skills", Variant()).get_type() == Variant::ARRAY) {
		selected_skills = Array(p_request.metadata["selected_skills"]).duplicate(true);
	}

	AISystemContext current_context;
	if (!_load_system_context(current_session, current_agent, provider, model, selected_skills, current_context, r_error)) {
		return false;
	}

	AIContextEpoch current_epoch;
	return context_epoch_service->current_struct(p_session_id, current_agent, prepared_epoch.revision, current_context, current_epoch, r_error);
}

bool AISessionRunner::_build_request(const AISessionRow &p_session, const String &p_agent_id, const String &p_root_dir, int64_t p_wake_seq, AIModelRequest &r_request, Ref<AIV1ToolMaterialization> &r_tool_materialization, AIError &r_error) {
	if (config_service.is_null() || runtime_registry.is_null() || projector.is_null() || context_epoch_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner dependencies are not wired.");
		return false;
	}

	_project_durable_history(p_session.id);

	const Dictionary config = config_service->get_config();
	if (!bool(config.get("success", true))) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "ConfigService failed to provide config.", config.get("error", Dictionary()));
		return false;
	}

	if (!runtime_registry->configure_from_config_struct(config, r_error)) {
		return false;
	}

	AIAgentConfig agent_config;
	if (!_resolve_agent_for_session(p_session, agent_config, r_error)) {
		return false;
	}
	const String provider = agent_config.provider;
	Dictionary provider_config = config_service->get_provider_config(provider);
	const String model = agent_config.model.strip_edges().is_empty() ? String(provider_config.get("model", config_service->get_default_model())).strip_edges() : agent_config.model.strip_edges();
	if (!model.is_empty()) {
		provider_config["model"] = model;
	}

	Vector<AISessionMessage> projected_messages = projector->get_messages_struct(p_session.id);
	Array selected_skills;
	if (!_select_skills_for_prompt(config, _latest_user_prompt_text(projected_messages), selected_skills, r_error)) {
		return false;
	}

	const int64_t history_token_budget = _history_token_budget_from_config(config);
	if (compaction_service.is_valid() && history_token_budget > 0) {
		bool compacted = false;
		Dictionary compaction_result;
		if (!compaction_service->maybe_compact_struct(p_session.id, "auto", history_token_budget, compacted, compaction_result, r_error)) {
			return false;
		}
		if (compacted) {
			projected_messages = projector->get_messages_struct(p_session.id);
		}
	}

	AIContextEpoch epoch;
	if (!_prepare_context_epoch(p_session, p_agent_id, provider, model, selected_skills, epoch, r_error)) {
		return false;
	}

	r_request.request_id = AIId::make("request");
	r_request.provider = provider.is_empty() ? String("fake") : provider;
	r_request.model = model.is_empty() ? String("fake-model") : model;
	r_request.system = _system_parts_from_baseline(epoch.baseline);
	r_request.provider_options = provider_config.duplicate(true);
	r_request.max_output_tokens = int(provider_config.get("max_output_tokens", provider_config.get("maxOutputTokens", 0)));
	if (r_request.max_output_tokens < 0) {
		r_request.max_output_tokens = 0;
	}
	r_request.metadata["session_id"] = p_session.id;
	r_request.metadata["wake_seq"] = p_wake_seq;
	r_request.metadata["location"] = p_session.location.to_dictionary();
	r_request.metadata["config_provider"] = provider;
	r_request.metadata["config_model"] = model;
	r_request.metadata["config_sources"] = _ai_session_runner_config_sources_for_request(config_service);
	r_request.metadata["context_epoch"] = epoch.to_dictionary();
	r_request.metadata["selected_skills"] = selected_skills.duplicate(true);

	const Vector<AISessionMessage> runner_messages = AISessionHistory::entries_for_runner(projected_messages, epoch.baseline_seq, history_token_budget);
	for (int i = 0; i < runner_messages.size(); i++) {
		if (!_append_message_to_model_messages(runner_messages[i], provider_config, r_request.messages, r_error)) {
			return false;
		}
	}

	if (tool_registry.is_valid()) {
		const Array rules = _permission_rules_for_agent(config, agent_config);
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
		settle_input["cancel_token"] = p_cancel_token;
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
	if (!_verify_context_epoch_current(p_session_id, p_agent_id, p_request, r_error)) {
		return false;
	}

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
	started["request"] = _request_for_event_log(p_request);

	AIEventRow start_row;
	if (!_append_event(p_session_id, AIDomainEventTypes::step_started(), started, false, start_row, r_error)) {
		return false;
	}

	Ref<AISessionRunnerEventRecorder> recorder;
	recorder.instantiate();
	recorder->setup(event_store, projector, p_tool_materialization, p_cancel_token, p_session_id, p_agent_id, assistant_message_id);

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
	const Dictionary usage = recorder->get_usage();
	if (!usage.is_empty()) {
		ended["tokens"] = usage;
	}
	AIEventRow end_row;
	if (!_append_event(p_session_id, AIDomainEventTypes::step_ended(), ended, false, end_row, r_error)) {
		return false;
	}
	if (!_ai_session_runner_merge_session_token_usage(session_store, p_session_id, usage, r_error)) {
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
	if (context_epoch_service.is_valid()) {
		context_epoch_service->set_event_store(event_store);
	}
	if (compaction_service.is_valid()) {
		compaction_service->set_event_store(event_store);
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
	if (context_epoch_service.is_valid()) {
		context_epoch_service->set_projector(projector);
	}
	if (compaction_service.is_valid()) {
		compaction_service->set_projector(projector);
	}
}

Ref<AISessionProjector> AISessionRunner::get_projector() const {
	return projector;
}

void AISessionRunner::set_context_epoch_store(const Ref<AIContextEpochStore> &p_store) {
	context_epoch_store = p_store;
	if (context_epoch_service.is_valid()) {
		context_epoch_service->set_epoch_store(context_epoch_store);
	}
}

Ref<AIContextEpochStore> AISessionRunner::get_context_epoch_store() const {
	return context_epoch_store;
}

void AISessionRunner::set_context_source_registry(const Ref<AIContextSourceRegistry> &p_registry) {
	context_source_registry = p_registry;
	if (context_source_registry.is_valid()) {
		context_source_registry->set_config_service(config_service);
	}
	if (compaction_service.is_valid()) {
		compaction_service->set_context_source_registry(context_source_registry);
	}
}

Ref<AIContextSourceRegistry> AISessionRunner::get_context_source_registry() const {
	return context_source_registry;
}

void AISessionRunner::set_context_epoch_service(const Ref<AIContextEpochService> &p_service) {
	context_epoch_service = p_service;
	if (context_epoch_service.is_valid()) {
		context_epoch_service->set_epoch_store(context_epoch_store);
		context_epoch_service->set_event_store(event_store);
		context_epoch_service->set_projector(projector);
	}
	if (compaction_service.is_valid()) {
		compaction_service->set_context_epoch_service(context_epoch_service);
	}
}

Ref<AIContextEpochService> AISessionRunner::get_context_epoch_service() const {
	return context_epoch_service;
}

void AISessionRunner::set_config_service(const Ref<AIConfigService> &p_config_service) {
	config_service = p_config_service;
	if (context_source_registry.is_valid()) {
		context_source_registry->set_config_service(config_service);
	}
	if (agent_service.is_valid()) {
		agent_service->set_config_service(config_service);
	}
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
	if (skill_service.is_valid()) {
		skill_service->set_tool_registry(tool_registry);
	}
}

Ref<AIV1ToolRegistry> AISessionRunner::get_tool_registry() const {
	return tool_registry;
}

void AISessionRunner::set_session_store(const Ref<AISessionStore> &p_store) {
	session_store = p_store;
	if (agent_service.is_valid()) {
		agent_service->set_session_store(session_store);
	}
}

Ref<AISessionStore> AISessionRunner::get_session_store() const {
	return session_store;
}

void AISessionRunner::set_compaction_service(const Ref<AICompactionService> &p_service) {
	compaction_service = p_service;
	if (compaction_service.is_valid()) {
		compaction_service->set_event_store(event_store);
		compaction_service->set_projector(projector);
		compaction_service->set_context_source_registry(context_source_registry);
		compaction_service->set_context_epoch_service(context_epoch_service);
	}
}

Ref<AICompactionService> AISessionRunner::get_compaction_service() const {
	return compaction_service;
}

void AISessionRunner::set_model_part_builder(const Ref<AIModelPartBuilder> &p_builder) {
	model_part_builder = p_builder;
	if (model_part_builder.is_null()) {
		model_part_builder.instantiate();
	}
}

Ref<AIModelPartBuilder> AISessionRunner::get_model_part_builder() const {
	return model_part_builder;
}

void AISessionRunner::set_skill_service(const Ref<AIV1SkillService> &p_service) {
	skill_service = p_service;
	if (skill_service.is_valid()) {
		skill_service->set_tool_registry(tool_registry);
	}
}

Ref<AIV1SkillService> AISessionRunner::get_skill_service() const {
	return skill_service;
}

void AISessionRunner::set_agent_service(const Ref<AIAgentService> &p_service) {
	agent_service = p_service;
	if (agent_service.is_valid()) {
		agent_service->set_config_service(config_service);
		agent_service->set_session_store(session_store);
	}
}

Ref<AIAgentService> AISessionRunner::get_agent_service() const {
	return agent_service;
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
	AIAgentConfig agent_config;
	if (!_resolve_agent_for_session(session, agent_config, r_error)) {
		return false;
	}
	const String agent_id = agent_config.id.strip_edges().is_empty() ? (session.agent_id.strip_edges().is_empty() ? String("main") : session.agent_id.strip_edges()) : agent_config.id.strip_edges();
	const String root_dir = session.location.directory.strip_edges();

	if (config_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner has no ConfigService.");
		return false;
	}
	Dictionary config = config_service.is_valid() ? config_service->get_config() : Dictionary();
	if (!bool(config.get("success", true))) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "ConfigService failed to provide config.", config.get("error", Dictionary()));
		return false;
	}
	const String provider = agent_config.provider;
	const Dictionary provider_config = config_service->get_provider_config(provider);
	const String model = agent_config.model.strip_edges().is_empty() ? String(provider_config.get("model", config_service->get_default_model())).strip_edges() : agent_config.model.strip_edges();

	AIContextEpoch admission_epoch;
	if (!_prepare_context_epoch(session, agent_id, provider, model, Array(), admission_epoch, r_error)) {
		return false;
	}

	if (!prompt_promoter->promote_eligible_struct(session_id, "new-activity", r_promoted, r_error)) {
		return false;
	}

	const Array rules = _permission_rules_for_agent(config, agent_config);
	if (!_configure_skill_service_from_config(config, r_error)) {
		return false;
	}

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
			if (_is_rebuild_request_conflict(r_error)) {
				continue;
			}
			return false;
		}
		if (provider_waiting_permission) {
			return true;
		}
		if (!needs_continuation) {
			return true;
		}

		bool settled_turn_tools = false;
		bool turn_waiting_permission = false;
		if (!_settle_open_tool_calls(session, agent_id, root_dir, rules, p_cancel_token, settled_turn_tools, turn_waiting_permission, r_error)) {
			return false;
		}
		if (turn_waiting_permission) {
			r_error = AIError::none();
			return true;
		}
		if (!settled_turn_tools) {
			r_error = AIError::none();
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
