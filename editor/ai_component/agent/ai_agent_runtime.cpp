/**************************************************************************/
/*  ai_agent_runtime.cpp                                                  */
/**************************************************************************/

#include "ai_agent_runtime.h"

#include "core/os/time.h"
#include "core/variant/variant.h"

#include "editor/ai_component/prompts/agent_system_prompt.h"
#include "editor/ai_component/tools/ai_tool_permission.h"

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

void AIAgentRuntime::_bind_methods() {
}

AIAgentRuntime::AIAgentRuntime() {
	profile = AIAgentProfile::get_plan_profile();
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

void AIAgentRuntime::set_profile(const AIAgentProfile &p_profile) {
	profile = p_profile;
}

AIAgentProfile AIAgentRuntime::get_profile() const {
	return profile;
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

Array AIAgentRuntime::_messages_to_array(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) const {
	Array messages;

	Dictionary system_message;
	system_message["role"] = "system";
	system_message["content"] = String(AIAgentPrompts::SYSTEM_PROMPT);
	messages.push_back(system_message);

	if (!p_context_documents.is_empty()) {
		String context_text = "Read-only project context:\n";
		for (int i = 0; i < p_context_documents.size(); i++) {
			if (Variant(p_context_documents[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary doc = p_context_documents[i];
			context_text += "\n## " + String(doc.get("title", "Context")) + "\n";
			context_text += "Source: " + String(doc.get("source", "")) + "\n";
			context_text += String(doc.get("content", "")) + "\n";
		}
		Dictionary context_message;
		context_message["role"] = "system";
		context_message["content"] = context_text;
		messages.push_back(context_message);
	}

	for (int i = 0; i < p_messages.size(); i++) {
		messages.push_back(p_messages[i].to_dict());
	}
	return messages;
}

Array AIAgentRuntime::_get_allowed_tool_schemas() const {
	Array schemas;
	if (tool_registry.is_null()) {
		return schemas;
	}

	for (HashSet<String>::Iterator it = profile.allowed_tools.begin(); it; ++it) {
		Ref<AITool> tool = tool_registry->get_tool(*it);
		if (tool.is_valid()) {
			schemas.push_back(tool->get_openai_schema());
		}
	}

	return schemas;
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

	int executed_tool_calls = 0;
	const Array tool_schemas = _get_allowed_tool_schemas();
	print_line(vformat("[AI Agent][Runtime] Allowed tool schemas prepared. count=%d max_provider_turns=%d max_tool_calls=%d", tool_schemas.size(), max_provider_turns, max_tool_calls));

	for (int turn = 0; turn < max_provider_turns; turn++) {
		Array provider_messages = _messages_to_array(result.messages, p_context_documents);
		print_line(vformat("[AI Agent][Runtime] Provider turn started. turn=%d provider_messages=%d executed_tools=%d", turn + 1, provider_messages.size(), executed_tool_calls));
		AIAgentRuntimeResponse response = client->complete(provider_messages, tool_schemas);

		if (!response.error.is_empty()) {
			result.error = response.error;
			print_line(vformat("[AI Agent][Runtime] Provider turn failed. turn=%d error=%s", turn + 1, result.error));
			return result;
		}
		print_line(vformat("[AI Agent][Runtime] Provider turn completed. turn=%d content_chars=%d tool_calls=%d", turn + 1, response.content.length(), response.tool_calls.size()));

		if (!response.has_tool_calls()) {
			if (!response.content.is_empty()) {
				AIAgentMessage assistant_message;
				assistant_message.role = AI_AGENT_ROLE_ASSISTANT;
				assistant_message.content = response.content;
				assistant_message.metadata = response.metadata;
				assistant_message.created_at = Time::get_singleton()->get_unix_time_from_system();
				result.messages.push_back(assistant_message);
				print_line(vformat("[AI Agent][Runtime] Final assistant message appended. chars=%d", response.content.length()));
			}
			result.success = true;
			print_line(vformat("[AI Agent][Runtime] Run completed successfully. result_messages=%d executed_tools=%d", result.messages.size(), executed_tool_calls));
			return result;
		}

		result.messages.push_back(_make_assistant_tool_call_message(response));
		print_line(vformat("[AI Agent][Runtime] Assistant tool-call message appended. tool_calls=%d", response.tool_calls.size()));

		for (int i = 0; i < response.tool_calls.size(); i++) {
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
			call.status = AI_TOOL_CALL_STATUS_RUNNING;

			Dictionary result_metadata;
			AIToolPermissionResult permission = AIToolPermissionPolicy::evaluate(profile, call.tool_name, call.arguments);
			if (permission.decision != AI_TOOL_PERMISSION_ALLOW) {
				call.status = AI_TOOL_CALL_STATUS_DENIED;
				call.updated_at = Time::get_singleton()->get_unix_time_from_system();
				result.tool_calls.push_back(call);
				result.messages.push_back(_make_tool_result_message(call, _make_tool_denied_message(call.tool_name, permission.reason), AIToolCall::status_to_string(call.status), result_metadata));
				executed_tool_calls++;
				print_line(vformat("[AI Agent][Runtime] Tool call denied. name=%s reason=%s executed_tools=%d", call.tool_name, permission.reason, executed_tool_calls));
				continue;
			}
			print_line(vformat("[AI Agent][Runtime] Tool call allowed. name=%s", call.tool_name));

			Ref<AITool> tool = tool_registry->get_tool(call.tool_name);
			if (tool.is_null()) {
				call.status = AI_TOOL_CALL_STATUS_FAILED;
				call.updated_at = Time::get_singleton()->get_unix_time_from_system();
				result.tool_calls.push_back(call);
				result.messages.push_back(_make_tool_result_message(call, _make_tool_failure_message(call.tool_name, "tool is not registered"), AIToolCall::status_to_string(call.status), result_metadata));
				executed_tool_calls++;
				print_line(vformat("[AI Agent][Runtime] Tool call failed: tool is not registered. name=%s executed_tools=%d", call.tool_name, executed_tool_calls));
				continue;
			}

			print_line(vformat("[AI Agent][Runtime] Executing tool. name=%s", call.tool_name));
			AIToolResult tool_result = tool->execute(call.arguments);
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
			result.tool_calls.push_back(call);
			result.messages.push_back(_make_tool_result_message(call, content, AIToolCall::status_to_string(call.status), result_metadata));
			executed_tool_calls++;
			print_line(vformat("[AI Agent][Runtime] Tool result message appended. name=%s status=%s executed_tools=%d", call.tool_name, AIToolCall::status_to_string(call.status), executed_tool_calls));
		}
	}

	result.error = "Agent provider turn limit exceeded.";
	print_line(vformat("[AI Agent][Runtime] Provider turn limit exceeded. max_provider_turns=%d", max_provider_turns));
	return result;
}
