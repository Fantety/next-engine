/**************************************************************************/
/*  ai_agent_next_dock.cpp                                                */
/**************************************************************************/

#include "ai_agent_next_dock.h"

#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/ui/ai_next_panel.h"
#include "editor/themes/editor_scale.h"

void AIAgentNextDock::_bind_methods() {
}

AIAgentNextDock::AIAgentNextDock() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 8 * EDSCALE);

	next_session = memnew(AIAgentNextSession);
	add_child(next_session);

	next_panel = memnew(AINextPanel);
	next_panel->set_next_session(next_session);
	add_child(next_panel);
}

AIAgentNextSession *AIAgentNextDock::get_next_session_for_test() const {
	return next_session;
}
