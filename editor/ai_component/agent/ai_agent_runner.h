/**************************************************************************/
/*  ai_agent_runner.h                                                      */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/vector.h"

#include "editor/ai_component/agent/ai_agent_message.h"
#include "editor/ai_component/providers/ai_agent_provider.h"

class AIAgentRunner : public RefCounted {
	GDCLASS(AIAgentRunner, RefCounted);

	AIAgentProvider *provider = nullptr;

	Array _build_provider_messages(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) const;

protected:
	static void _bind_methods();

public:
	void set_provider(AIAgentProvider *p_provider);
	bool start(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents);
	void cancel();
};
