/**************************************************************************/
/*  ai_agent_next_dock.cpp                                                */
/**************************************************************************/

#include "ai_agent_next_dock.h"

#include "core/object/class_db.h"
#include "editor/ai_component/next/ai_agent_next_session.h"
#include "editor/ai_component/next/ai_next_agent_settings.h"
#include "editor/ai_component/ui/ai_next_panel.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/scroll_container.h"

void AIAgentNextDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply_agent_model_settings"), &AIAgentNextDock::apply_agent_model_settings);
}

AIAgentNextDock::AIAgentNextDock() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 8 * EDSCALE);

	next_session = memnew(AIAgentNextSession);
	add_child(next_session);

	next_scroll = memnew(ScrollContainer);
	next_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	next_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	next_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	next_scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
	next_scroll->set_follow_focus(true);
	add_child(next_scroll);

	next_panel = memnew(AINextPanel);
	next_panel->set_next_session(next_session);
	next_scroll->add_child(next_panel);

	_apply_agent_model_settings();
}

void AIAgentNextDock::_apply_agent_model_settings() {
	ERR_FAIL_NULL(next_session);

	Vector<String> agent_ids = AINextAgentSettings::get_agent_ids();
	for (int i = 0; i < agent_ids.size(); i++) {
		const String model_profile_id = AINextAgentSettings::get_effective_model_profile_id(agent_ids[i]);
		if (!model_profile_id.is_empty()) {
			next_session->set_agent_model_profile_id(agent_ids[i], model_profile_id);
		}
	}
}

void AIAgentNextDock::apply_agent_model_settings() {
	_apply_agent_model_settings();
}

AIAgentNextSession *AIAgentNextDock::get_next_session_for_test() const {
	return next_session;
}
