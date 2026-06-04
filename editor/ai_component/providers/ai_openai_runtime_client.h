/**************************************************************************/
/*  ai_openai_runtime_client.h                                            */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/providers/ai_provider_config.h"

class AIOpenAIRuntimeTransport : public RefCounted {
	GDCLASS(AIOpenAIRuntimeTransport, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual bool request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error);
	virtual bool request_chat_completion_stream(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, const Callable &p_stream_event_callback, String &r_error);
};

class AIOpenAIHTTPRuntimeTransport : public AIOpenAIRuntimeTransport {
	GDCLASS(AIOpenAIHTTPRuntimeTransport, AIOpenAIRuntimeTransport);

protected:
	static void _bind_methods();

public:
	virtual bool request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error) override;
	virtual bool request_chat_completion_stream(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, const Callable &p_stream_event_callback, String &r_error) override;
};

class AIOpenAICompatibleRuntimeClient : public AIAgentRuntimeClient {
	GDCLASS(AIOpenAICompatibleRuntimeClient, AIAgentRuntimeClient);

	AIProviderConfig config;
	Ref<AIOpenAIRuntimeTransport> transport;

	static String _to_provider_tool_name(const String &p_internal_tool_name);
	static String _to_internal_tool_name(const String &p_provider_tool_name, const Dictionary &p_tool_name_map);
	static Array _build_provider_tool_schemas(const Array &p_tool_schemas, const AIProviderConfig &p_config, Dictionary &r_tool_name_map);
	static Array _build_chat_messages(const Array &p_messages, const AIProviderConfig &p_config = AIProviderConfig(), const Dictionary &p_tool_name_map = Dictionary());
	static void _apply_tool_name_map(AIAgentRuntimeResponse &r_response, const Dictionary &p_tool_name_map);
	AIAgentRuntimeResponse _complete_internal(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback, bool p_stream);

protected:
	static void _bind_methods();

public:
	AIOpenAICompatibleRuntimeClient();

	void set_config(const AIProviderConfig &p_config);
	AIProviderConfig get_config() const;

	void set_transport(const Ref<AIOpenAIRuntimeTransport> &p_transport);
	Ref<AIOpenAIRuntimeTransport> get_transport() const;

	static Array build_chat_messages_for_test(const Array &p_messages);
	static Array build_chat_messages_for_test(const Array &p_messages, const AIProviderConfig &p_config);
	static Array build_provider_tool_schemas_for_test(const Array &p_tool_schemas, Dictionary &r_tool_name_map);
	static Array build_provider_tool_schemas_for_test(const Array &p_tool_schemas, const AIProviderConfig &p_config, Dictionary &r_tool_name_map);
	static void apply_tool_name_map_for_test(AIAgentRuntimeResponse &r_response, const Dictionary &p_tool_name_map);

	virtual AIAgentRuntimeResponse complete(const Array &p_messages, const Array &p_tool_schemas) override;
	virtual AIAgentRuntimeResponse complete_streaming(const Array &p_messages, const Array &p_tool_schemas, const Callable &p_partial_response_callback) override;
};
