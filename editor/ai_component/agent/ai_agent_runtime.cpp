/**************************************************************************/
/*  ai_agent_runtime.cpp                                                  */
/**************************************************************************/

#include "ai_agent_runtime.h"

#include "core/object/callable_mp.h"
#include "core/os/time.h"
#include "core/variant/variant.h"

#include "editor/ai_component/prompts/agent_system_prompt.h"
#include "editor/ai_component/tools/ai_tool_execution_context.h"
#include "editor/ai_component/tools/ai_tool_permission.h"

namespace {

const char *PROVIDER_TOOL_ARGUMENTS_PARSE_ERROR_KEY = "_provider_tool_arguments_parse_error";
const char *PROVIDER_TOOL_ARGUMENTS_RAW_KEY = "_provider_tool_arguments";

int _estimate_tokens_from_chars(int p_chars) {
	if (p_chars <= 0) {
		return 0;
	}
	return (p_chars + 3) / 4;
}

Dictionary _make_estimated_context_usage(const Dictionary &p_context_metadata) {
	Dictionary usage;
	const int estimated_input_chars = (int)p_context_metadata.get("estimated_input_chars", 0);
	usage["estimated_input_chars"] = estimated_input_chars;
	usage["estimated_input_tokens"] = _estimate_tokens_from_chars(estimated_input_chars);
	usage["max_input_chars"] = (int)p_context_metadata.get("max_input_chars", 0);
	usage["omitted_history_messages"] = (int)p_context_metadata.get("omitted_history_messages", 0);
	usage["truncated_tool_results"] = (int)p_context_metadata.get("truncated_tool_results", 0);
	usage["truncated_context_documents"] = (int)p_context_metadata.get("truncated_context_documents", 0);
	usage["source"] = "context_estimate";
	return usage;
}

void _add_provider_usage(Dictionary &r_total, const Dictionary &p_metadata) {
	if (!p_metadata.has("usage") || Variant(p_metadata["usage"]).get_type() != Variant::DICTIONARY) {
		return;
	}

	Dictionary usage = p_metadata["usage"];
	r_total["prompt_tokens"] = (int)r_total.get("prompt_tokens", 0) + (int)usage.get("prompt_tokens", 0);
	r_total["completion_tokens"] = (int)r_total.get("completion_tokens", 0) + (int)usage.get("completion_tokens", 0);
	r_total["total_tokens"] = (int)r_total.get("total_tokens", 0) + (int)usage.get("total_tokens", 0);
	r_total["source"] = "provider";
}

} // namespace

bool AIAgentRuntimeResponse::has_tool_calls() const {
	return !tool_calls.is_empty();
}

void AIAgentRuntimeClient::_bind_methods() {
}

AIAgentRuntimeResponse AIAgentRuntimeClient::complete(const Array &p_messages, const Array &p_tool_schemas) {
	(void)p_messages;
	(void)p_tool_schemas;

	AIAgentRuntimeResponse response;
	response.error = "Agent runtime client is not implemented.";
	return response;
}

AIAgentRuntimeResponse AIAgentRuntimeClient::complete_streaming(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback) {
	(void)p_partial_response_callback;
	return complete(p_messages, p_tool_schemas);
}

void AIAgentRuntime::_bind_methods() {
}

AIAgentRuntime::AIAgentRuntime() {
	profile = AIAgentProfile::get_ask_profile();
	system_prompt = AIAgentPrompts::SYSTEM_PROMPT;
	context_manager.instantiate();
}

void AIAgentRuntime::set_client(const Ref<AIAgentRuntimeClient> &p_client) {
	client = p_client;
}

Ref<AIAgentRuntimeClient> AIAgentRuntime::get_client() const {
	return client;
}

void AIAgentRuntime::set_tool_registry(const Ref<AIToolRegistry> &p_registry) {
	tool_registry = p_registry;
}

Ref<AIToolRegistry> AIAgentRuntime::get_tool_registry() const {
	return tool_registry;
}

void AIAgentRuntime::set_context_manager(const Ref<AIContextManager> &p_context_manager) {
	context_manager = p_context_manager;
}

Ref<AIContextManager> AIAgentRuntime::get_context_manager() const {
	return context_manager;
}

void AIAgentRuntime::set_profile(const AIAgentProfile &p_profile) {
	profile = p_profile;
}

AIAgentProfile AIAgentRuntime::get_profile() const {
	return profile;
}

void AIAgentRuntime::set_system_prompt(const String &p_system_prompt) {
	system_prompt = p_system_prompt;
}

String AIAgentRuntime::get_system_prompt() const {
	return system_prompt;
}

void AIAgentRuntime::set_session_id(const String &p_session_id) {
	session_id = p_session_id;
}

String AIAgentRuntime::get_session_id() const {
	return session_id;
}

void AIAgentRuntime::set_max_provider_turns(int p_max_provider_turns) {
	max_provider_turns = MAX(1, p_max_provider_turns);
}

int AIAgentRuntime::get_max_provider_turns() const {
	return max_provider_turns;
}

void AIAgentRuntime::set_max_tool_calls(int p_max_tool_calls) {
	max_tool_calls = MAX(1, p_max_tool_calls);
}

int AIAgentRuntime::get_max_tool_calls() const {
	return max_tool_calls;
}

void AIAgentRuntime::set_progress_callbacks(const Callable &p_message_added_callback, const Callable &p_message_updated_callback) {
	message_added_callback = p_message_added_callback;
	message_updated_callback = p_message_updated_callback;
}

void AIAgentRuntime::clear_progress_callbacks() {
	message_added_callback = Callable();
	message_updated_callback = Callable();
}

void AIAgentRuntime::request_cancel() {
	cancel_requested.set();
	MutexLock lock(active_tool_context_mutex);
	if (active_tool_context.is_valid()) {
		active_tool_context->request_cancel();
	}
}

void AIAgentRuntime::clear_cancel_request() {
	cancel_requested.clear();
	MutexLock lock(active_tool_context_mutex);
	if (active_tool_context.is_valid()) {
		active_tool_context->clear_cancel_request();
	}
}

bool AIAgentRuntime::is_cancel_requested() const {
	return cancel_requested.is_set();
}

String AIAgentRuntime::_build_system_prompt_for_profile() const {
	String prompt = system_prompt;
	prompt += "\nCurrent agent profile: ";
	prompt += profile.display_name.is_empty() ? profile.id : profile.display_name;
	if (!profile.id.is_empty()) {
		prompt += " (" + profile.id + ")";
	}
	prompt += ".\n";
	prompt += "Agent capabilities: " + profile.get_capabilities_summary() + "\n";

	if (profile.id == "auto") {
		prompt += "Mode behavior: Auto. Use the tool schemas sent with this request as the current tool list; allowed mutating tools may be used, and ask-gated tools require user approval. Editor changes are recorded for review.\n";
	} else if (profile.id == "ask") {
		prompt += "Mode behavior: Ask. Inspect context and propose only; mutating editor/project tools are intentionally unavailable in this request.\n";
	} else {
		prompt += "Mode behavior: Custom. Use the tool schemas sent with this request as the current tool list; permissions are enforced by the editor.\n";
	}
	if (tool_registry.is_valid()) {
		prompt += "\n";
		prompt += tool_registry->get_tool_context_prompt();
	}
	return prompt;
}

Array AIAgentRuntime::_get_available_tool_schemas() const {
	if (tool_registry.is_null()) {
		return Array();
	}
	return tool_registry->get_available_tool_schemas();
}

AIAgentMessage AIAgentRuntime::_make_assistant_tool_call_message(const AIAgentRuntimeResponse &p_response) const {
	AIAgentMessage message;
	message.role = AI_AGENT_ROLE_ASSISTANT;
	message.content = p_response.content;
	message.created_at = Time::get_singleton()->get_unix_time_from_system();
	message.metadata = p_response.metadata;

	Array tool_calls;
	for (int i = 0; i < p_response.tool_calls.size(); i++) {
		tool_calls.push_back(p_response.tool_calls[i].to_dict());
	}
	message.metadata["tool_calls"] = tool_calls;

	return message;
}

AIAgentMessage AIAgentRuntime::_make_tool_result_message(const AIToolCall &p_call, const String &p_content, const String &p_status, const Dictionary &p_metadata) const {
	AIAgentMessage message;
	message.role = AI_AGENT_ROLE_TOOL;
	message.content = p_content;
	message.created_at = Time::get_singleton()->get_unix_time_from_system();
	message.metadata = p_metadata;
	message.metadata["tool_call_id"] = p_call.id;
	message.metadata["tool_name"] = p_call.tool_name;
	message.metadata["status"] = p_status;
	return message;
}

String AIAgentRuntime::_make_tool_denied_message(const String &p_tool_name, const String &p_reason) const {
	String reason = p_reason.is_empty() ? String("permission policy denied the request") : p_reason;
	return "Tool call denied for `" + p_tool_name + "`: " + reason;
}

String AIAgentRuntime::_make_tool_failure_message(const String &p_tool_name, const String &p_reason) const {
	String reason = p_reason.is_empty() ? String("unknown error") : p_reason;
	return "Tool call failed for `" + p_tool_name + "`: " + reason;
}

void AIAgentRuntime::_emit_message_added(int p_index, const AIAgentMessage &p_message) const {
	if (message_added_callback.is_valid()) {
		message_added_callback.call(p_index, p_message.to_dict());
	}
}

void AIAgentRuntime::_emit_message_updated(int p_index, const AIAgentMessage &p_message) const {
	if (message_updated_callback.is_valid()) {
		message_updated_callback.call(p_index, p_message.to_dict());
	}
}

bool AIAgentRuntime::_finish_if_cancel_requested(AIAgentRuntimeResult &r_result, const String &p_stage) const {
	if (!is_cancel_requested()) {
		return false;
	}

	r_result.success = false;
	r_result.error = "Agent run cancelled.";
	r_result.metadata["cancelled"] = true;
	r_result.metadata["cancel_stage"] = p_stage;
	print_line(vformat("[AI Agent][Runtime] Run cancelled. stage=%s result_messages=%d tool_calls=%d", p_stage, r_result.messages.size(), r_result.tool_calls.size()));
	return true;
}

void AIAgentRuntime::_set_active_tool_context(const Ref<AIToolExecutionContext> &p_context) {
	MutexLock lock(active_tool_context_mutex);
	active_tool_context = p_context;
	if (cancel_requested.is_set() && active_tool_context.is_valid()) {
		active_tool_context->request_cancel();
	}
}

void AIAgentRuntime::_clear_active_tool_context(const Ref<AIToolExecutionContext> &p_context) {
	MutexLock lock(active_tool_context_mutex);
	if (active_tool_context == p_context) {
		active_tool_context.unref();
	}
}

void AIAgentRuntime::_on_provider_partial_response(const Dictionary &p_response) {
	if (!streaming_result || is_cancel_requested()) {
		return;
	}

	AIAgentRuntimeResponse partial_response;
	if (p_response.has("content") && Variant(p_response["content"]).get_type() != Variant::NIL) {
		partial_response.content = p_response["content"];
	}
	if (p_response.has("metadata") && Variant(p_response["metadata"]).get_type() == Variant::DICTIONARY) {
		partial_response.metadata = p_response["metadata"];
	}

	if (streaming_assistant_message_index < 0) {
		if (partial_response.content.is_empty()) {
			return;
		}

		AIAgentMessage assistant_message;
		assistant_message.role = AI_AGENT_ROLE_ASSISTANT;
		assistant_message.content = partial_response.content;
		assistant_message.metadata = partial_response.metadata;
		assistant_message.created_at = Time::get_singleton()->get_unix_time_from_system();
		streaming_assistant_message_index = streaming_result->messages.size();
		streaming_result->messages.push_back(assistant_message);
		_emit_message_added(streaming_assistant_message_index, assistant_message);
		print_line(vformat("[AI Agent][Runtime] Streaming assistant message started. index=%d chars=%d", streaming_assistant_message_index, partial_response.content.length()));
		return;
	}

	if (streaming_assistant_message_index >= streaming_result->messages.size()) {
		return;
	}

	AIAgentMessage assistant_message = streaming_result->messages[streaming_assistant_message_index];
	assistant_message.content = partial_response.content;
	assistant_message.metadata = partial_response.metadata;
	streaming_result->messages.write[streaming_assistant_message_index] = assistant_message;
	_emit_message_updated(streaming_assistant_message_index, assistant_message);
}

AIAgentRuntimeResult AIAgentRuntime::run(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) {
	AIAgentRuntimeResult result;
	result.messages = p_messages;
	print_line(vformat("[AI Agent][Runtime] Starting run. input_messages=%d context_documents=%d profile=%s", p_messages.size(), p_context_documents.size(), profile.id));

	if (client.is_null()) {
		result.error = "Agent runtime client is not configured.";
		print_line("[AI Agent][Runtime] Failed before provider turn: runtime client is not configured.");
		return result;
	}

	if (tool_registry.is_null()) {
		result.error = "Agent tool registry is not configured.";
		print_line("[AI Agent][Runtime] Failed before provider turn: tool registry is not configured.");
		return result;
	}
	if (context_manager.is_null()) {
		result.error = "Agent context manager is not configured.";
		print_line("[AI Agent][Runtime] Failed before provider turn: context manager is not configured.");
		return result;
	}

	int executed_tool_calls = 0;
	Dictionary token_usage;
	print_line(vformat("[AI Agent][Runtime] Tool runtime prepared. max_provider_turns=%d max_tool_calls=%d", max_provider_turns, max_tool_calls));

	for (int turn = 0; turn < max_provider_turns; turn++) {
		if (_finish_if_cancel_requested(result, "before_provider_turn")) {
			return result;
		}

		const Array tool_schemas = _get_available_tool_schemas();
		AIContextBuildResult context_result = context_manager->build_messages(_build_system_prompt_for_profile(), result.messages, p_context_documents);
		Array provider_messages = context_result.messages;
		result.metadata["last_context"] = context_result.metadata;
		print_line(vformat("[AI Agent][Runtime] Provider turn started. turn=%d provider_messages=%d tool_schemas=%d estimated_chars=%d omitted_history=%d truncated_tools=%d truncated_context=%d executed_tools=%d",
				turn + 1,
				provider_messages.size(),
				tool_schemas.size(),
				(int)context_result.metadata.get("estimated_input_chars", 0),
				(int)context_result.metadata.get("omitted_history_messages", 0),
				(int)context_result.metadata.get("truncated_tool_results", 0),
				(int)context_result.metadata.get("truncated_context_documents", 0),
				executed_tool_calls));
		streaming_result = &result;
		streaming_assistant_message_index = -1;
		AIAgentRuntimeResponse response = client->complete_streaming(provider_messages, tool_schemas, callable_mp(this, &AIAgentRuntime::_on_provider_partial_response));
		streaming_result = nullptr;

		if (_finish_if_cancel_requested(result, "after_provider_turn")) {
			streaming_assistant_message_index = -1;
			return result;
		}

		if (!response.error.is_empty()) {
			streaming_assistant_message_index = -1;
			result.error = response.error;
			print_line(vformat("[AI Agent][Runtime] Provider turn failed. turn=%d error=%s", turn + 1, result.error));
			return result;
		}
		response.metadata["estimated_context_usage"] = _make_estimated_context_usage(context_result.metadata);
		_add_provider_usage(token_usage, response.metadata);
		if (!token_usage.is_empty()) {
			result.metadata["token_usage"] = token_usage;
		}
		if (response.metadata.has("usage") && Variant(response.metadata["usage"]).get_type() == Variant::DICTIONARY) {
			Dictionary usage = response.metadata["usage"];
			print_line(vformat("[AI Agent][Runtime] Provider token usage. turn=%d prompt=%d completion=%d total=%d cumulative_total=%d",
					turn + 1,
					(int)usage.get("prompt_tokens", 0),
					(int)usage.get("completion_tokens", 0),
					(int)usage.get("total_tokens", 0),
					(int)token_usage.get("total_tokens", 0)));
		}
		print_line(vformat("[AI Agent][Runtime] Provider turn completed. turn=%d content_chars=%d tool_calls=%d", turn + 1, response.content.length(), response.tool_calls.size()));

		if (!response.has_tool_calls()) {
			if (streaming_assistant_message_index >= 0 && streaming_assistant_message_index < result.messages.size()) {
				AIAgentMessage assistant_message = result.messages[streaming_assistant_message_index];
				assistant_message.content = response.content;
				assistant_message.metadata = response.metadata;
				result.messages.write[streaming_assistant_message_index] = assistant_message;
				_emit_message_updated(streaming_assistant_message_index, assistant_message);
				print_line(vformat("[AI Agent][Runtime] Final streaming assistant message committed. chars=%d", response.content.length()));
				streaming_assistant_message_index = -1;
			} else if (!response.content.is_empty()) {
				AIAgentMessage assistant_message;
				assistant_message.role = AI_AGENT_ROLE_ASSISTANT;
				assistant_message.content = response.content;
				assistant_message.metadata = response.metadata;
				assistant_message.created_at = Time::get_singleton()->get_unix_time_from_system();
				result.messages.push_back(assistant_message);
				_emit_message_added(result.messages.size() - 1, assistant_message);
				print_line(vformat("[AI Agent][Runtime] Final assistant message appended. chars=%d", response.content.length()));
			}
			result.success = true;
			print_line(vformat("[AI Agent][Runtime] Run completed successfully. result_messages=%d executed_tools=%d", result.messages.size(), executed_tool_calls));
			return result;
		}

		AIAgentMessage assistant_tool_call_message = _make_assistant_tool_call_message(response);
		int assistant_tool_call_message_index = result.messages.size();
		if (streaming_assistant_message_index >= 0 && streaming_assistant_message_index < result.messages.size()) {
			assistant_tool_call_message_index = streaming_assistant_message_index;
			result.messages.write[assistant_tool_call_message_index] = assistant_tool_call_message;
			_emit_message_updated(assistant_tool_call_message_index, assistant_tool_call_message);
			streaming_assistant_message_index = -1;
		} else {
			result.messages.push_back(assistant_tool_call_message);
			_emit_message_added(assistant_tool_call_message_index, assistant_tool_call_message);
		}
		print_line(vformat("[AI Agent][Runtime] Assistant tool-call message appended. tool_calls=%d", response.tool_calls.size()));

		for (int i = 0; i < response.tool_calls.size(); i++) {
			if (_finish_if_cancel_requested(result, "before_tool_call")) {
				return result;
			}

			if (executed_tool_calls >= max_tool_calls) {
				result.error = "Agent tool call limit exceeded.";
				print_line(vformat("[AI Agent][Runtime] Tool call limit exceeded. max_tool_calls=%d", max_tool_calls));
				return result;
			}

			AIToolCall call = response.tool_calls[i];
			print_line(vformat("[AI Agent][Runtime] Tool call received. index=%d id=%s name=%s", i, call.id, call.tool_name));
			const uint64_t now = Time::get_singleton()->get_unix_time_from_system();
			if (call.created_at == 0) {
				call.created_at = now;
			}
			call.updated_at = now;
			const String provider_tool_arguments_parse_error = String(call.arguments.get(PROVIDER_TOOL_ARGUMENTS_PARSE_ERROR_KEY, String()));
			if (!provider_tool_arguments_parse_error.is_empty()) {
				call.status = AI_TOOL_CALL_STATUS_FAILED;
				if (assistant_tool_call_message.metadata.has("tool_calls") && Variant(assistant_tool_call_message.metadata["tool_calls"]).get_type() == Variant::ARRAY) {
					Array failed_tool_calls = assistant_tool_call_message.metadata["tool_calls"];
					if (i < failed_tool_calls.size()) {
						failed_tool_calls[i] = call.to_dict();
						assistant_tool_call_message.metadata["tool_calls"] = failed_tool_calls;
						result.messages.write[assistant_tool_call_message_index] = assistant_tool_call_message;
						_emit_message_updated(assistant_tool_call_message_index, assistant_tool_call_message);
					}
				}

				Dictionary result_metadata;
				result_metadata["provider_tool_arguments_parse_error"] = provider_tool_arguments_parse_error;
				result_metadata["provider_tool_arguments"] = String(call.arguments.get(PROVIDER_TOOL_ARGUMENTS_RAW_KEY, String()));
				result.tool_calls.push_back(call);
				AIAgentMessage tool_message = _make_tool_result_message(call, _make_tool_failure_message(call.tool_name, provider_tool_arguments_parse_error), AIToolCall::status_to_string(call.status), result_metadata);
				result.messages.push_back(tool_message);
				_emit_message_added(result.messages.size() - 1, tool_message);
				executed_tool_calls++;
				print_line(vformat("[AI Agent][Runtime] Tool call failed before execution: %s name=%s executed_tools=%d", provider_tool_arguments_parse_error, call.tool_name, executed_tool_calls));
				continue;
			}
			call.status = AI_TOOL_CALL_STATUS_RUNNING;
			if (assistant_tool_call_message.metadata.has("tool_calls") && Variant(assistant_tool_call_message.metadata["tool_calls"]).get_type() == Variant::ARRAY) {
				Array running_tool_calls = assistant_tool_call_message.metadata["tool_calls"];
				if (i < running_tool_calls.size()) {
					running_tool_calls[i] = call.to_dict();
					assistant_tool_call_message.metadata["tool_calls"] = running_tool_calls;
					result.messages.write[assistant_tool_call_message_index] = assistant_tool_call_message;
					_emit_message_updated(assistant_tool_call_message_index, assistant_tool_call_message);
				}
			}

			Dictionary result_metadata;
			AIToolPermissionResult permission_result = AIToolPermissionPolicy::evaluate(tool_registry->get_tool_permission(call.tool_name), call.tool_name, tool_registry->get_tool_permission_reason(call.tool_name));
			if (permission_result.permission == AI_TOOL_PERMISSION_ASK) {
				call.status = AI_TOOL_CALL_STATUS_PENDING;
				call.updated_at = Time::get_singleton()->get_unix_time_from_system();
				if (assistant_tool_call_message.metadata.has("tool_calls") && Variant(assistant_tool_call_message.metadata["tool_calls"]).get_type() == Variant::ARRAY) {
					Array pending_tool_calls = assistant_tool_call_message.metadata["tool_calls"];
					if (i < pending_tool_calls.size()) {
						pending_tool_calls[i] = call.to_dict();
						assistant_tool_call_message.metadata["tool_calls"] = pending_tool_calls;
						result.messages.write[assistant_tool_call_message_index] = assistant_tool_call_message;
						_emit_message_updated(assistant_tool_call_message_index, assistant_tool_call_message);
					}
				}
				result.pending_approval = call.to_dict();
				result.pending_approval["reason"] = permission_result.reason;
				result.success = true;
				print_line(vformat("[AI Agent][Runtime] Tool call requires approval. name=%s reason=%s", call.tool_name, permission_result.reason));
				return result;
			}
			if (permission_result.permission != AI_TOOL_PERMISSION_ALLOW) {
				call.status = AI_TOOL_CALL_STATUS_DENIED;
				call.updated_at = Time::get_singleton()->get_unix_time_from_system();
				if (assistant_tool_call_message.metadata.has("tool_calls") && Variant(assistant_tool_call_message.metadata["tool_calls"]).get_type() == Variant::ARRAY) {
					Array denied_tool_calls = assistant_tool_call_message.metadata["tool_calls"];
					if (i < denied_tool_calls.size()) {
						denied_tool_calls[i] = call.to_dict();
						assistant_tool_call_message.metadata["tool_calls"] = denied_tool_calls;
						result.messages.write[assistant_tool_call_message_index] = assistant_tool_call_message;
						_emit_message_updated(assistant_tool_call_message_index, assistant_tool_call_message);
					}
				}
				result.tool_calls.push_back(call);
				AIAgentMessage tool_message = _make_tool_result_message(call, _make_tool_denied_message(call.tool_name, permission_result.reason), AIToolCall::status_to_string(call.status), result_metadata);
				result.messages.push_back(tool_message);
				_emit_message_added(result.messages.size() - 1, tool_message);
				executed_tool_calls++;
				print_line(vformat("[AI Agent][Runtime] Tool call denied. name=%s reason=%s executed_tools=%d", call.tool_name, permission_result.reason, executed_tool_calls));
				continue;
			}
			print_line(vformat("[AI Agent][Runtime] Tool call allowed. name=%s", call.tool_name));

			Ref<AITool> tool = tool_registry->get_tool(call.tool_name);
			if (tool.is_null()) {
				call.status = AI_TOOL_CALL_STATUS_FAILED;
				call.updated_at = Time::get_singleton()->get_unix_time_from_system();
				if (assistant_tool_call_message.metadata.has("tool_calls") && Variant(assistant_tool_call_message.metadata["tool_calls"]).get_type() == Variant::ARRAY) {
					Array failed_tool_calls = assistant_tool_call_message.metadata["tool_calls"];
					if (i < failed_tool_calls.size()) {
						failed_tool_calls[i] = call.to_dict();
						assistant_tool_call_message.metadata["tool_calls"] = failed_tool_calls;
						result.messages.write[assistant_tool_call_message_index] = assistant_tool_call_message;
						_emit_message_updated(assistant_tool_call_message_index, assistant_tool_call_message);
					}
				}
				result.tool_calls.push_back(call);
				AIAgentMessage tool_message = _make_tool_result_message(call, _make_tool_failure_message(call.tool_name, "tool is not registered"), AIToolCall::status_to_string(call.status), result_metadata);
				result.messages.push_back(tool_message);
				_emit_message_added(result.messages.size() - 1, tool_message);
				executed_tool_calls++;
				print_line(vformat("[AI Agent][Runtime] Tool call failed: tool is not registered. name=%s executed_tools=%d", call.tool_name, executed_tool_calls));
				continue;
			}

			print_line(vformat("[AI Agent][Runtime] Executing tool. name=%s", call.tool_name));
			Ref<AIToolExecutionContext> tool_context;
			tool_context.instantiate();
			tool_context->set_agent_profile_id(profile.id);
			tool_context->set_review_changes(profile.review_changes);
			tool_context->set_session_id(session_id);
			tool_context->set_tool_call_id(call.id);
			_set_active_tool_context(tool_context);
			AIToolExecutionContext::set_current(tool_context);
			AIToolResult tool_result = tool->execute(call.arguments);
			AIToolExecutionContext::clear_current();
			_clear_active_tool_context(tool_context);

			if (_finish_if_cancel_requested(result, "after_tool_execution")) {
				return result;
			}

			result_metadata = tool_result.metadata;
			result_metadata["truncated"] = tool_result.truncated;

			String content;
			if (tool_result.is_error()) {
				call.status = AI_TOOL_CALL_STATUS_FAILED;
				content = _make_tool_failure_message(call.tool_name, tool_result.error);
				print_line(vformat("[AI Agent][Runtime] Tool execution failed. name=%s error=%s", call.tool_name, tool_result.error));
			} else {
				call.status = AI_TOOL_CALL_STATUS_COMPLETED;
				content = tool_result.content;
				print_line(vformat("[AI Agent][Runtime] Tool execution completed. name=%s content_chars=%d truncated=%s", call.tool_name, content.length(), tool_result.truncated ? "yes" : "no"));
			}

			call.updated_at = Time::get_singleton()->get_unix_time_from_system();
			if (assistant_tool_call_message.metadata.has("tool_calls") && Variant(assistant_tool_call_message.metadata["tool_calls"]).get_type() == Variant::ARRAY) {
				Array finished_tool_calls = assistant_tool_call_message.metadata["tool_calls"];
				if (i < finished_tool_calls.size()) {
					finished_tool_calls[i] = call.to_dict();
					assistant_tool_call_message.metadata["tool_calls"] = finished_tool_calls;
					result.messages.write[assistant_tool_call_message_index] = assistant_tool_call_message;
					_emit_message_updated(assistant_tool_call_message_index, assistant_tool_call_message);
				}
			}
			result.tool_calls.push_back(call);
			AIAgentMessage tool_message = _make_tool_result_message(call, content, AIToolCall::status_to_string(call.status), result_metadata);
			result.messages.push_back(tool_message);
			_emit_message_added(result.messages.size() - 1, tool_message);
			executed_tool_calls++;
			print_line(vformat("[AI Agent][Runtime] Tool result message appended. name=%s status=%s executed_tools=%d", call.tool_name, AIToolCall::status_to_string(call.status), executed_tool_calls));
		}
	}

	result.error = "Agent provider turn limit exceeded.";
	print_line(vformat("[AI Agent][Runtime] Provider turn limit exceeded. max_provider_turns=%d", max_provider_turns));
	return result;
}
