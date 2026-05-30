/**************************************************************************/
/*  ai_main_agent.h                                                       */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_agent_base.h"

class AIMainAgent : public AIAgentBase {
	GDCLASS(AIMainAgent, AIAgentBase);

	void _register_local_tools();
	void _register_mcp_tools();

protected:
	static void _bind_methods();

public:
	AIMainAgent();

	virtual void set_profile(const AIAgentProfile &p_profile) override;
	virtual void set_agent_profile_id(const String &p_profile_id) override;
	void reload_tools();
};
