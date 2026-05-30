/**************************************************************************/
/*  ai_agent_profile.h                                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

struct AIAgentProfile {
	String id;
	String display_name;

	static AIAgentProfile get_plan_profile();
	static AIAgentProfile get_build_profile();
	static AIAgentProfile get_review_profile();
	static AIAgentProfile get_write_profile();
};
