/**************************************************************************/
/*  ai_agent_base.cpp                                                     */
/**************************************************************************/

#include "ai_agent_base.h"

#include "editor/ai_component/prompts/agent_system_prompt.h"

void AIAgentBase::_bind_methods() {
}

AIAgentBase::AIAgentBase() {
	profile = AIAgentProfile::get_ask_profile();
	system_prompt = AIAgentPrompts::SYSTEM_PROMPT;

	runtime.instantiate();
	runtime_runner.instantiate();
	tool_registry.instantiate();

	Ref<AIOpenAICompatibleRuntimeClient> openai_client;
	openai_client.instantiate();
	runtime_client = openai_client;

	_sync_runtime_configuration();
}

void AIAgentBase::_sync_runtime_configuration() {
	if (runtime.is_valid()) {
		runtime->set_client(runtime_client);
		runtime->set_tool_registry(tool_registry);
		runtime->set_profile(profile);
		runtime->set_system_prompt(system_prompt);
		runtime->set_session_id(session_id);
	}
	if (runtime_runner.is_valid()) {
		runtime_runner->set_runtime(runtime);
	}
	_apply_provider_config();
}

void AIAgentBase::_apply_provider_config() {
	Ref<AIOpenAICompatibleRuntimeClient> openai_client = get_openai_runtime_client();
	if (openai_client.is_valid()) {
		openai_client->set_config(provider_config);
	}

	if (runtime.is_null()) {
		return;
	}
	runtime->set_max_provider_turns(provider_config.max_provider_turns);
	runtime->set_max_tool_calls(provider_config.max_tool_calls);

	Ref<AIContextManager> context_manager = runtime->get_context_manager();
	if (context_manager.is_null()) {
		return;
	}
	context_manager->set_max_input_chars(provider_config.max_input_chars);
	context_manager->set_max_context_chars(provider_config.max_context_chars);
	context_manager->set_max_history_chars(provider_config.max_history_chars);
	context_manager->set_max_tool_result_chars(provider_config.max_tool_result_chars);
	context_manager->set_min_recent_messages(provider_config.min_recent_messages);
}

void AIAgentBase::set_agent_profile_id(const String &p_profile_id) {
	if (p_profile_id == "auto") {
		set_profile(AIAgentProfile::get_auto_profile());
	} else {
		set_profile(AIAgentProfile::get_ask_profile());
	}
}

String AIAgentBase::get_agent_profile_id() const {
	return profile.id;
}

void AIAgentBase::set_profile(const AIAgentProfile &p_profile) {
	profile = p_profile;
	_sync_runtime_configuration();
}

AIAgentProfile AIAgentBase::get_profile() const {
	return profile;
}

void AIAgentBase::set_system_prompt(const String &p_system_prompt) {
	system_prompt = p_system_prompt;
	_sync_runtime_configuration();
}

String AIAgentBase::get_system_prompt() const {
	return system_prompt;
}

void AIAgentBase::set_provider_config(const AIProviderConfig &p_config) {
	provider_config = p_config;
	_apply_provider_config();
}

AIProviderConfig AIAgentBase::get_provider_config() const {
	return provider_config;
}

void AIAgentBase::set_model_profile(const AIModelProfile &p_profile) {
	AIProviderConfig config;
	config.provider_name = p_profile.provider_name;
	config.base_url = p_profile.base_url;
	config.api_key = p_profile.api_key;
	config.model = p_profile.model;
	static_cast<AIModelRuntimeOptions &>(config) = p_profile;
	set_provider_config(config);
}

void AIAgentBase::set_model_profile_id(const String &p_profile_id) {
	set_provider_config(AIModelSettings::get_provider_config(p_profile_id));
}

void AIAgentBase::set_session_id(const String &p_session_id) {
	session_id = p_session_id;
	_sync_runtime_configuration();
}

String AIAgentBase::get_session_id() const {
	return session_id;
}

void AIAgentBase::set_runtime_client(const Ref<AIAgentRuntimeClient> &p_client) {
	runtime_client = p_client;
	_sync_runtime_configuration();
}

Ref<AIAgentRuntimeClient> AIAgentBase::get_runtime_client() const {
	return runtime_client;
}

Ref<AIOpenAICompatibleRuntimeClient> AIAgentBase::get_openai_runtime_client() const {
	if (runtime_client.is_null()) {
		return Ref<AIOpenAICompatibleRuntimeClient>();
	}

	AIOpenAICompatibleRuntimeClient *openai_client = Object::cast_to<AIOpenAICompatibleRuntimeClient>(*runtime_client);
	if (!openai_client) {
		return Ref<AIOpenAICompatibleRuntimeClient>();
	}
	return Ref<AIOpenAICompatibleRuntimeClient>(openai_client);
}

Ref<AIAgentRuntime> AIAgentBase::get_runtime() const {
	return runtime;
}

Ref<AIAgentRuntimeRunner> AIAgentBase::get_runtime_runner() const {
	return runtime_runner;
}

Ref<AIToolRegistry> AIAgentBase::get_tool_registry() const {
	return tool_registry;
}

bool AIAgentBase::add_tool(const Ref<AITool> &p_tool, AIToolPermission p_permission, const String &p_permission_reason) {
	if (tool_registry.is_null()) {
		return false;
	}
	return tool_registry->register_tool(p_tool, p_permission, p_permission_reason);
}

void AIAgentBase::clear_tools() {
	if (tool_registry.is_valid()) {
		tool_registry->clear();
	}
}

AIAgentRuntimeResult AIAgentBase::run(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) {
	_sync_runtime_configuration();
	if (runtime.is_null()) {
		AIAgentRuntimeResult result;
		result.error = "Agent runtime is not configured.";
		return result;
	}
	return runtime->run(p_messages, p_context_documents);
}

bool AIAgentBase::start(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) {
	_sync_runtime_configuration();
	if (runtime_runner.is_null()) {
		return false;
	}
	return runtime_runner->start(p_messages, p_context_documents);
}
