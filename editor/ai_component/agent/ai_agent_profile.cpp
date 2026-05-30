/**************************************************************************/
/*  ai_agent_profile.cpp                                                  */
/**************************************************************************/

#include "ai_agent_profile.h"

AIAgentProfile AIAgentProfile::get_plan_profile() {
	AIAgentProfile profile;
	profile.id = "plan";
	profile.display_name = "Plan";
	return profile;
}

AIAgentProfile AIAgentProfile::get_write_profile() {
	AIAgentProfile profile;
	profile.id = "write";
	profile.display_name = "Write";
	return profile;
}

AIAgentProfile AIAgentProfile::get_review_profile() {
	AIAgentProfile profile;
	profile.id = "review";
	profile.display_name = "Review";
	return profile;
}

AIAgentProfile AIAgentProfile::get_build_profile() {
	AIAgentProfile profile;
	profile.id = "build";
	profile.display_name = "Build";
	return profile;
}
