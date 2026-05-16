/**************************************************************************/
/*  ai_openai_runtime_client.h                                            */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/providers/ai_provider_config.h"

class AIOpenAIRuntimeTransport : public RefCounted {
	GDCLASS(AIOpenAIRuntimeTransport, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual bool request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error);
};

class AIOpenAIHTTPRuntimeTransport : public AIOpenAIRuntimeTransport {
	GDCLASS(AIOpenAIHTTPRuntimeTransport, AIOpenAIRuntimeTransport);

protected:
	static void _bind_methods();

public:
	virtual bool request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error) override;
};

class AIOpenAICompatibleRuntimeClient : public AIAgentRuntimeClient {
	GDCLASS(AIOpenAICompatibleRuntimeClient, AIAgentRuntimeClient);

	AIProviderConfig config;
	Ref<AIOpenAIRuntimeTransport> transport;

protected:
	static void _bind_methods();

public:
	AIOpenAICompatibleRuntimeClient();

	void set_config(const AIProviderConfig &p_config);
	AIProviderConfig get_config() const;

	void set_transport(const Ref<AIOpenAIRuntimeTransport> &p_transport);
	Ref<AIOpenAIRuntimeTransport> get_transport() const;

	virtual AIAgentRuntimeResponse complete(const Array &p_messages, const Array &p_tool_schemas) override;
};
