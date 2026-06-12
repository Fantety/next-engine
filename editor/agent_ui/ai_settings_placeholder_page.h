/**************************************************************************/
/*  ai_settings_placeholder_page.h                                         */
/**************************************************************************/

#pragma once

#include "scene/gui/margin_container.h"

class AISettingsPlaceholderPage : public MarginContainer {
	GDCLASS(AISettingsPlaceholderPage, MarginContainer);

protected:
	static void _bind_methods();

public:
	AISettingsPlaceholderPage();

	void set_placeholder_text(const String &p_text);
};
