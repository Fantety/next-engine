/**************************************************************************/
/*  ai_plan_panel.h                                                        */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "scene/gui/panel_container.h"

class Button;
class Label;
class VBoxContainer;

class AIPlanPanel : public PanelContainer {
	GDCLASS(AIPlanPanel, PanelContainer);

	Button *toggle_button = nullptr;
	Label *title_label = nullptr;
	VBoxContainer *task_list = nullptr;
	bool expanded = true;

	void _build_ui();
	void _clear_tasks();
	void _refresh();
	void _toggle_pressed();
	String _status_label(const String &p_status) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIPlanPanel();
	void refresh_plan();
};
