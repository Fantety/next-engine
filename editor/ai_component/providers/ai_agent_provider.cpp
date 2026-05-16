/**************************************************************************/
/*  ai_agent_provider.cpp                                                  */
/**************************************************************************/

#include "ai_agent_provider.h"

void AIAgentProvider::_bind_methods() {
	ADD_SIGNAL(MethodInfo("response_started"));
	ADD_SIGNAL(MethodInfo("response_delta", PropertyInfo(Variant::STRING, "delta")));
	ADD_SIGNAL(MethodInfo("response_finished", PropertyInfo(Variant::STRING, "finish_reason")));
	ADD_SIGNAL(MethodInfo("request_failed", PropertyInfo(Variant::STRING, "message")));
}

void AIAgentProvider::set_config(const AIProviderConfig &p_config) {
	config = p_config;
}

AIProviderConfig AIAgentProvider::get_config() const {
	return config;
}

bool AIAgentProvider::start_chat(const Array &p_messages) {
	ERR_FAIL_V_MSG(false, "AIAgentProvider::start_chat must be implemented by subclasses.");
}

void AIAgentProvider::cancel() {
}
