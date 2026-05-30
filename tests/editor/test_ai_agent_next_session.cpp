/**************************************************************************/
/*  test_ai_agent_next_session.cpp                                        */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/next/ai_agent_next_session.h"

TEST_FORCE_LINK(test_ai_agent_next_session);

namespace TestAIAgentNextSession {

TEST_CASE("[Editor][AI][NEXT] session initializes independent agents") {
	AIAgentNextSession *session = memnew(AIAgentNextSession);

	CHECK(session->get_project_state().is_valid());
	CHECK(session->has_agent("planning_agent"));
	CHECK(session->has_agent("script_agent"));
	CHECK(session->has_agent("scene_agent"));
	CHECK(session->has_agent("shader_agent"));
	CHECK(session->has_agent("review_agent"));

	memdelete(session);
}

} // namespace TestAIAgentNextSession
