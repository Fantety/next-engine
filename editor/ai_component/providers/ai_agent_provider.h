/**************************************************************************/
/*  ai_agent_provider.h                                                    */
/**************************************************************************/

#pragma once

#include "core/object/class_db.h"
#include "scene/main/node.h"

#include "editor/ai_component/providers/ai_provider_config.h"

class AIAgentProvider : public Node {
	GDCLASS(AIAgentProvider, Node);

protected:
	AIProviderConfig config;
	static void _bind_methods();

public:
	void set_config(const AIProviderConfig &p_config);
	AIProviderConfig get_config() const;

	virtual bool start_chat(const Array &p_messages);
	virtual void cancel();
};
