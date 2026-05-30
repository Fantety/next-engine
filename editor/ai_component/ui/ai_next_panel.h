/**************************************************************************/
/*  ai_next_panel.h                                                       */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIAgentNextSession;
class AINextFeedbackPanel;
class AINextMilestoneList;
class AINextTaskInspector;
class AINextTaskTree;
class Button;
class Label;
class TextEdit;

class AINextPanel : public VBoxContainer {
	GDCLASS(AINextPanel, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	TextEdit *brief_input = nullptr;
	Label *state_label = nullptr;
	AINextMilestoneList *milestone_list = nullptr;
	AINextTaskTree *task_tree = nullptr;
	AINextTaskInspector *task_inspector = nullptr;
	AINextFeedbackPanel *feedback_panel = nullptr;

	void _submit_brief_pressed();
	void _generate_plan_pressed();
	void _run_milestone_pressed();
	void _refresh();
	Label *_add_section_label(const String &p_text);

protected:
	static void _bind_methods();

public:
	AINextPanel();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
