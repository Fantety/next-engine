/**************************************************************************/
/*  ai_agent_next_dock.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/ai_component/ui/ai_mode_panel.h"

class AIAgentNextSession;
class AINextPanel;
class ScrollContainer;

class AIAgentNextDock : public AIModePanel {
	GDCLASS(AIAgentNextDock, AIModePanel);

	AIAgentNextSession *next_session = nullptr;
	ScrollContainer *next_scroll = nullptr;
	AINextPanel *next_panel = nullptr;

	void _apply_agent_model_settings();

protected:
	static void _bind_methods();

public:
	AIAgentNextDock();
	void apply_agent_model_settings();
	void apply_settings() override;
	AIAgentNextSession *get_next_session_for_test() const;
};
