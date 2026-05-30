/**************************************************************************/
/*  ai_agent_base.h                                                       */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

#include "editor/ai_component/agent/ai_agent_profile.h"
#include "editor/ai_component/agent/ai_agent_runtime.h"
#include "editor/ai_component/agent/ai_agent_runtime_runner.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/providers/ai_openai_runtime_client.h"
#include "editor/ai_component/providers/ai_provider_config.h"
#include "editor/ai_component/tools/ai_tool_permission.h"
#include "editor/ai_component/tools/ai_tool_registry.h"

class AIAgentBase : public RefCounted {
	GDCLASS(AIAgentBase, RefCounted);

	AIAgentProfile profile;
	AIProviderConfig provider_config;
	String system_prompt;
	String session_id;

	Ref<AIAgentRuntime> runtime;
	Ref<AIAgentRuntimeRunner> runtime_runner;
	Ref<AIAgentRuntimeClient> runtime_client;
	Ref<AIToolRegistry> tool_registry;

	void _sync_runtime_configuration();
	void _apply_provider_config();

protected:
	static void _bind_methods();

public:
	AIAgentBase();

	virtual void set_agent_profile_id(const String &p_profile_id);
	String get_agent_profile_id() const;

	virtual void set_profile(const AIAgentProfile &p_profile);
	AIAgentProfile get_profile() const;

	void set_system_prompt(const String &p_system_prompt);
	String get_system_prompt() const;

	void set_provider_config(const AIProviderConfig &p_config);
	AIProviderConfig get_provider_config() const;
	void set_model_profile(const AIModelProfile &p_profile);
	void set_model_profile_id(const String &p_profile_id);

	void set_session_id(const String &p_session_id);
	String get_session_id() const;

	void set_runtime_client(const Ref<AIAgentRuntimeClient> &p_client);
	Ref<AIAgentRuntimeClient> get_runtime_client() const;
	Ref<AIOpenAICompatibleRuntimeClient> get_openai_runtime_client() const;

	Ref<AIAgentRuntime> get_runtime() const;
	Ref<AIAgentRuntimeRunner> get_runtime_runner() const;
	Ref<AIToolRegistry> get_tool_registry() const;

	bool add_tool(const Ref<AITool> &p_tool, AIToolPermission p_permission = AI_TOOL_PERMISSION_ALLOW, const String &p_permission_reason = String());
	void clear_tools();

	AIAgentRuntimeResult run(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents = Array());
	bool start(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents = Array());
};
