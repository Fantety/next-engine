/**************************************************************************/
/*  ai_todo_list_panel.h                                                  */
/**************************************************************************/

#pragma once

#include "core/variant/array.h"
#include "scene/gui/panel_container.h"

class Label;
class VBoxContainer;

class AITodoListPanel : public PanelContainer {
	GDCLASS(AITodoListPanel, PanelContainer);

	Label *summary_label = nullptr;
	VBoxContainer *items_box = nullptr;
	Array todos;
	bool applying_theme = false;

	void _apply_theme();
	void _refresh();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AITodoListPanel();

	void set_todos(const Array &p_todos);
	Array get_todos() const;
};
