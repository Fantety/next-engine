/**************************************************************************/
/*  ai_agent_next_dock.h                                                  */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;
class AINextPanel;
class ScrollContainer;

class AIAgentNextDock : public VBoxContainer {
	GDCLASS(AIAgentNextDock, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	ScrollContainer *next_scroll = nullptr;
	AINextPanel *next_panel = nullptr;

	void _apply_agent_model_settings();

protected:
	static void _bind_methods();

public:
	AIAgentNextDock();
	void apply_agent_model_settings();
	AIAgentNextSession *get_next_session_for_test() const;
};
