/**************************************************************************/
/*  ai_agent_runtime.h                                                    */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/agent/ai_agent_message.h"
#include "editor/ai_component/agent/ai_context_manager.h"
#include "editor/ai_component/agent/ai_agent_profile.h"
#include "editor/ai_component/tools/ai_tool_call.h"
#include "editor/ai_component/tools/ai_tool_registry.h"

struct AIAgentRuntimeResponse {
	String content;
	Vector<AIToolCall> tool_calls;
	String error;
	Dictionary metadata;

	bool has_tool_calls() const;
};

struct AIAgentRuntimeResult {
	bool success = false;
	String error;
	Vector<AIAgentMessage> messages;
	Vector<AIToolCall> tool_calls;
	Dictionary metadata;
};

class AIAgentRuntimeClient : public RefCounted {
	GDCLASS(AIAgentRuntimeClient, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual AIAgentRuntimeResponse complete(const Array &p_messages, const Array &p_tool_schemas);
	virtual AIAgentRuntimeResponse complete_streaming(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback);
};

class AIAgentRuntime : public RefCounted {
	GDCLASS(AIAgentRuntime, RefCounted);

	Ref<AIAgentRuntimeClient> client;
	Ref<AIToolRegistry> tool_registry;
	Ref<AIContextManager> context_manager;
	AIAgentProfile profile;
	Callable message_added_callback;
	Callable message_updated_callback;
	AIAgentRuntimeResult *streaming_result = nullptr;
	int streaming_assistant_message_index = -1;
	int max_provider_turns = 255;
	int max_tool_calls = 60;

	Array _get_allowed_tool_schemas() const;
	AIAgentMessage _make_assistant_tool_call_message(const AIAgentRuntimeResponse &p_response) const;
	AIAgentMessage _make_tool_result_message(const AIToolCall &p_call, const String &p_content, const String &p_status, const Dictionary &p_metadata) const;
	String _make_tool_denied_message(const String &p_tool_name, const String &p_reason) const;
	String _make_tool_failure_message(const String &p_tool_name, const String &p_reason) const;
	void _emit_message_added(const AIAgentMessage &p_message) const;
	void _emit_message_updated(int p_index, const AIAgentMessage &p_message) const;
	void _on_provider_partial_response(const Dictionary &p_response);

protected:
	static void _bind_methods();

public:
	AIAgentRuntime();

	void set_client(const Ref<AIAgentRuntimeClient> &p_client);
	Ref<AIAgentRuntimeClient> get_client() const;

	void set_tool_registry(const Ref<AIToolRegistry> &p_registry);
	Ref<AIToolRegistry> get_tool_registry() const;

	void set_context_manager(const Ref<AIContextManager> &p_context_manager);
	Ref<AIContextManager> get_context_manager() const;

	void set_profile(const AIAgentProfile &p_profile);
	AIAgentProfile get_profile() const;

	void set_max_provider_turns(int p_max_provider_turns);
	int get_max_provider_turns() const;

	void set_max_tool_calls(int p_max_tool_calls);
	int get_max_tool_calls() const;

	void set_progress_callbacks(const Callable &p_message_added_callback, const Callable &p_message_updated_callback);
	void clear_progress_callbacks();

	AIAgentRuntimeResult run(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents = Array());
};
