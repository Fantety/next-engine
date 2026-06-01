/**************************************************************************/
/*  ai_next_task_inspector.h                                              */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;
class MarkdownViewer;

class AINextTaskInspector : public VBoxContainer {
	GDCLASS(AINextTaskInspector, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	MarkdownViewer *detail_viewer = nullptr;

protected:
	static void _bind_methods();

public:
	AINextTaskInspector();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
