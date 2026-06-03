/**************************************************************************/
/*  ai_next_agents.h                                                      */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/next/ai_next_project_state.h"

class AINextPlanningAgent : public AIAgentBase {
	GDCLASS(AINextPlanningAgent, AIAgentBase);

	Ref<AINextProjectState> project_state;

	void _rebuild_tools();

protected:
	static void _bind_methods();

public:
	AINextPlanningAgent();

	void set_project_state(const Ref<AINextProjectState> &p_project_state);
	Ref<AINextProjectState> get_project_state() const;
};

class AINextScriptAgent : public AIAgentBase {
	GDCLASS(AINextScriptAgent, AIAgentBase);

protected:
	static void _bind_methods();

public:
	AINextScriptAgent();
};

class AINextSceneAgent : public AIAgentBase {
	GDCLASS(AINextSceneAgent, AIAgentBase);

protected:
	static void _bind_methods();

public:
	AINextSceneAgent();
};

class AINextShaderAgent : public AIAgentBase {
	GDCLASS(AINextShaderAgent, AIAgentBase);

protected:
	static void _bind_methods();

public:
	AINextShaderAgent();
};

class AINextReviewAgent : public AIAgentBase {
	GDCLASS(AINextReviewAgent, AIAgentBase);

protected:
	static void _bind_methods();

public:
	AINextReviewAgent();
};

namespace AINextAgents {

Ref<AIAgentBase> create_agent(const String &p_agent_id, const Ref<AINextProjectState> &p_project_state);

} // namespace AINextAgents
