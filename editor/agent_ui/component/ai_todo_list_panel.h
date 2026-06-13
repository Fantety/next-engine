/**************************************************************************/
/*  ai_todo_list_panel.h                                                  */
/**************************************************************************/

#pragma once

#include "core/variant/array.h"
#include "scene/gui/panel_container.h"

class Button;
class Label;
class VBoxContainer;

class AITodoListPanel : public PanelContainer {
	GDCLASS(AITodoListPanel, PanelContainer);

	Button *toggle_button = nullptr;
	Label *summary_label = nullptr;
	Label *current_task_label = nullptr;
	VBoxContainer *items_box = nullptr;
	Array todos;
	bool collapsed = false;
	bool applying_theme = false;

	void _apply_theme();
	void _refresh();
	void _toggle_collapsed();
	void _update_toggle_button();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AITodoListPanel();

	void set_todos(const Array &p_todos);
	Array get_todos() const;
	void set_collapsed(bool p_collapsed);
	bool is_collapsed() const;
};
