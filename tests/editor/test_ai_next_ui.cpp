/**************************************************************************/
/*  test_ai_next_ui.cpp                                                   */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/ui/ai_agent_next_dock.h"

TEST_FORCE_LINK(test_ai_next_ui);

namespace TestAINextUI {

TEST_CASE("[Editor][AI][NEXT] next dock owns a next session") {
	AIAgentNextDock *dock = memnew(AIAgentNextDock);

	CHECK(dock->get_next_session_for_test() != nullptr);
	CHECK(dock->get_next_session_for_test()->get_project_state().is_valid());

	memdelete(dock);
}

} // namespace TestAINextUI
