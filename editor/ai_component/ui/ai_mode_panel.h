/**************************************************************************/
/*  ai_mode_panel.h                                                       */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"

class AIModePanel : public VBoxContainer {
	GDCLASS(AIModePanel, VBoxContainer);

protected:
	static void _bind_methods();

public:
	virtual void apply_settings();
	virtual void on_activated();
	virtual void on_deactivated();
};
