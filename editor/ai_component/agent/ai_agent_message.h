/**************************************************************************/
/*  ai_agent_message.h                                                     */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/agent/ai_agent_types.h"

struct AIAgentMessage {
	AIAgentRole role = AI_AGENT_ROLE_USER;
	String content;
	Dictionary metadata;
	uint64_t created_at = 0;

	Dictionary to_dict() const;
	static AIAgentMessage from_dict(const Dictionary &p_dict);
	static String role_to_string(AIAgentRole p_role);
	static AIAgentRole string_to_role(const String &p_role);
};
