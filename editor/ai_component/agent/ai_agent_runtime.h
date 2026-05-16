/**************************************************************************/
/*  ai_agent_runtime.h                                                    */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/agent/ai_agent_message.h"
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
};

class AIAgentRuntime : public RefCounted {
	GDCLASS(AIAgentRuntime, RefCounted);

	Ref<AIAgentRuntimeClient> client;
	Ref<AIToolRegistry> tool_registry;
	AIAgentProfile profile;
	int max_provider_turns = 6;
	int max_tool_calls = 20;

	Array _messages_to_array(const Vector<AIAgentMessage> &p_messages) const;
	Array _get_allowed_tool_schemas() const;
	AIAgentMessage _make_assistant_tool_call_message(const AIAgentRuntimeResponse &p_response) const;
	AIAgentMessage _make_tool_result_message(const AIToolCall &p_call, const String &p_content, const String &p_status, const Dictionary &p_metadata) const;
	String _make_tool_denied_message(const String &p_tool_name, const String &p_reason) const;
	String _make_tool_failure_message(const String &p_tool_name, const String &p_reason) const;

protected:
	static void _bind_methods();

public:
	AIAgentRuntime();

	void set_client(const Ref<AIAgentRuntimeClient> &p_client);
	Ref<AIAgentRuntimeClient> get_client() const;

	void set_tool_registry(const Ref<AIToolRegistry> &p_registry);
	Ref<AIToolRegistry> get_tool_registry() const;

	void set_profile(const AIAgentProfile &p_profile);
	AIAgentProfile get_profile() const;

	void set_max_provider_turns(int p_max_provider_turns);
	int get_max_provider_turns() const;

	void set_max_tool_calls(int p_max_tool_calls);
	int get_max_tool_calls() const;

	AIAgentRuntimeResult run(const Vector<AIAgentMessage> &p_messages);
};
