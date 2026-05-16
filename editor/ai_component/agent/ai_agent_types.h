/**************************************************************************/
/*  ai_agent_types.h                                                       */
/**************************************************************************/

#pragma once

enum AIAgentRole {
	AI_AGENT_ROLE_SYSTEM,
	AI_AGENT_ROLE_USER,
	AI_AGENT_ROLE_ASSISTANT,
	AI_AGENT_ROLE_CONTEXT,
	AI_AGENT_ROLE_ERROR,
};

enum AIAgentState {
	AI_AGENT_STATE_IDLE,
	AI_AGENT_STATE_PREPARING_CONTEXT,
	AI_AGENT_STATE_STREAMING,
	AI_AGENT_STATE_CANCELLED,
	AI_AGENT_STATE_FAILED,
};
