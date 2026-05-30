/**************************************************************************/
/*  ai_next_milestone_list.h                                              */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;

class AINextMilestoneList : public VBoxContainer {
	GDCLASS(AINextMilestoneList, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;

	void _clear_rows();

protected:
	static void _bind_methods();

public:
	AINextMilestoneList();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
