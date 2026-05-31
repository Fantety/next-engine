/**************************************************************************/
/*  ai_next_task_tree.h                                                   */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;

class AINextTaskTree : public VBoxContainer {
	GDCLASS(AINextTaskTree, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;

	void _clear_rows();
	void _task_pressed(const String &p_task_id);
	void _run_task_pressed(const String &p_task_id);

protected:
	static void _bind_methods();

public:
	AINextTaskTree();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
