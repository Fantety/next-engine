/**************************************************************************/
/*  ai_agent_next_dock.h                                                  */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;
class AINextPanel;

class AIAgentNextDock : public VBoxContainer {
	GDCLASS(AIAgentNextDock, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	AINextPanel *next_panel = nullptr;

protected:
	static void _bind_methods();

public:
	AIAgentNextDock();
	AIAgentNextSession *get_next_session_for_test() const;
};
