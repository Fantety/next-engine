/**************************************************************************/
/*  ai_next_plan_rows.h                                                   */
/**************************************************************************/

#pragma once

#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "scene/gui/box_container.h"

class Button;
class Control;

void setup_ai_next_plan_icon_button(const Control *p_theme_owner, Button *p_button, const StringName &p_icon_name, const String &p_tooltip);
Control *make_ai_next_plan_drag_preview(const String &p_text);

class AINextMilestoneRow : public HBoxContainer {
	GDCLASS(AINextMilestoneRow, HBoxContainer);

	double pulse_phase = 0.0;

protected:
	void _notification(int p_what);

public:
	struct Callbacks {
		Callable select;
		Callable edit;
		Callable merge;
		Callable move_up;
		Callable move_down;
		Callable remove;
		Callable get_drag_data;
		Callable can_drop_data;
		Callable drop_data;
	};

	void setup(const Dictionary &p_milestone, int p_index, int p_milestone_count, bool p_active, bool p_can_edit, bool p_workflow_active, const Callbacks &p_callbacks);
};

class AINextTaskRow : public HBoxContainer {
	GDCLASS(AINextTaskRow, HBoxContainer);

	double pulse_phase = 0.0;

protected:
	void _notification(int p_what);

public:
	struct Callbacks {
		Callable select;
		Callable edit;
		Callable dependencies;
		Callable move_up;
		Callable move_down;
		Callable remove;
		Callable run;
		Callable get_drag_data;
		Callable can_drop_data;
		Callable drop_data;
	};

	void setup(const Dictionary &p_task, int p_index, int p_task_count, const String &p_status_text, bool p_selected, bool p_locked, bool p_can_edit, bool p_workflow_active, bool p_can_run, const Callbacks &p_callbacks);
};
