/**************************************************************************/
/*  ai_next_task_inspector.h                                              */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;
class Label;

class AINextTaskInspector : public VBoxContainer {
	GDCLASS(AINextTaskInspector, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	Label *title_label = nullptr;
	Label *status_label = nullptr;
	Label *agent_label = nullptr;
	Label *depends_label = nullptr;
	Label *outputs_label = nullptr;
	Label *result_label = nullptr;

protected:
	static void _bind_methods();

public:
	AINextTaskInspector();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
