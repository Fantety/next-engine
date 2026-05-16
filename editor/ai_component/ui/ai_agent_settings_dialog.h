/**************************************************************************/
/*  ai_agent_settings_dialog.h                                             */
/**************************************************************************/

#pragma once

#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"

class AIAgentSettingsDialog : public ConfirmationDialog {
	GDCLASS(AIAgentSettingsDialog, ConfirmationDialog);

	LineEdit *deepseek_api_key = nullptr;
	LineEdit *deepseek_base_url = nullptr;
	CheckButton *deepseek_chat = nullptr;
	CheckButton *deepseek_reasoner = nullptr;

	static inline AIAgentSettingsDialog *singleton = nullptr;

	void _save_settings();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIAgentSettingsDialog();
	static AIAgentSettingsDialog *get_singleton();
};
