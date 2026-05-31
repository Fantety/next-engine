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
class VBoxContainer;
class Label;
class TextEdit;

class AINextPanel : public VBoxContainer {
	GDCLASS(AINextPanel, VBoxContainer);

	AIAgentNextSession *next_session = nullptr;
	TextEdit *brief_input = nullptr;
	Label *state_label = nullptr;
	Label *operation_label = nullptr;
	VBoxContainer *activity_list = nullptr;
	Button *submit_button = nullptr;
	Button *plan_button = nullptr;
	Button *run_button = nullptr;
	Button *review_button = nullptr;
	AINextMilestoneList *milestone_list = nullptr;
	AINextTaskTree *task_tree = nullptr;
	AINextTaskInspector *task_inspector = nullptr;
	AINextFeedbackPanel *feedback_panel = nullptr;
	int spinner_frame = 0;
	double spinner_elapsed = 0.0;

	void _submit_brief_pressed();
	void _generate_plan_pressed();
	void _run_milestone_pressed();
	void _review_milestone_pressed();
	void _refresh();
	void _refresh_activity();
	void _update_operation_label();
	Label *_add_section_label(const String &p_text);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AINextPanel();
	void set_next_session(AIAgentNextSession *p_session);
	void refresh();
};
