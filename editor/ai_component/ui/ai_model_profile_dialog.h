/**************************************************************************/
/*  ai_model_profile_dialog.h                                             */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_model_settings.h"
#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/tab_container.h"

class Button;

class AIModelProfileDialog : public ConfirmationDialog {
	GDCLASS(AIModelProfileDialog, ConfirmationDialog);

	TabContainer *add_model_tabs = nullptr;
	LineEdit *provider_model_display_name = nullptr;
	OptionButton *provider_model_provider = nullptr;
	OptionButton *provider_model_model = nullptr;
	LineEdit *provider_model_api_key = nullptr;
	Button *provider_model_submit_button = nullptr;
	OptionButton *custom_api_format = nullptr;
	LineEdit *custom_display_name = nullptr;
	LineEdit *custom_base_url = nullptr;
	CheckButton *custom_full_url = nullptr;
	LineEdit *custom_model_id = nullptr;
	CheckButton *custom_multimodal = nullptr;
	LineEdit *custom_api_key = nullptr;
	Button *custom_model_submit_button = nullptr;

	bool editing_model = false;
	String editing_profile_id;
	bool editing_custom = false;

	void _build_ui();
	void _build_provider_add_tab(Control *p_page);
	void _build_custom_add_tab(Control *p_page);
	void _provider_model_provider_selected(int p_index);
	void _submit_pressed();
	void _reset_form();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIModelProfileDialog();

	void popup_add_model();
	void popup_edit_model(const AIModelProfile &p_profile);
	bool is_editing_model() const;
	bool is_editing_custom_model() const;
	String get_editing_profile_id() const;
	AIModelProfile get_submitted_profile() const;
};
