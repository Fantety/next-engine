/**************************************************************************/
/*  ai_model_profile_dialog.h                                             */
/**************************************************************************/

#pragma once

#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/tab_container.h"

class Button;
class VBoxContainer;

class AIModelProfileDialog : public ConfirmationDialog {
	GDCLASS(AIModelProfileDialog, ConfirmationDialog);

	TabContainer *add_model_tabs = nullptr;
	LineEdit *provider_model_display_name = nullptr;
	OptionButton *provider_model_provider = nullptr;
	OptionButton *provider_model_model = nullptr;
	LineEdit *provider_model_api_key = nullptr;
	SpinBox *provider_max_output_tokens = nullptr;
	SpinBox *provider_timeout_msec = nullptr;
	Button *provider_model_submit_button = nullptr;
	LineEdit *custom_display_name = nullptr;
	LineEdit *custom_base_url = nullptr;
	CheckButton *custom_full_url = nullptr;
	LineEdit *custom_model_id = nullptr;
	CheckButton *custom_multimodal = nullptr;
	LineEdit *custom_api_key = nullptr;
	SpinBox *custom_max_output_tokens = nullptr;
	SpinBox *custom_timeout_msec = nullptr;
	Button *custom_model_submit_button = nullptr;

	Array provider_presets;
	bool editing_model = false;
	String editing_profile_id;
	bool editing_custom = false;

	void _build_ui();
	void _build_provider_add_tab(Control *p_page);
	void _build_custom_add_tab(Control *p_page);
	void _build_advanced_config(VBoxContainer *p_content, bool p_provider_tab);
	SpinBox *_add_advanced_spinbox(VBoxContainer *p_content, const String &p_label, int p_default_value, int p_min_value, int p_max_value, const String &p_tooltip = String());
	void _reset_advanced_config(bool p_provider_tab);
	void _apply_advanced_config(const Dictionary &p_profile, bool p_provider_tab);
	void _read_advanced_config(Dictionary &r_profile, bool p_provider_tab) const;
	void _reload_provider_presets();
	void _provider_model_provider_selected(int p_index);
	void _submit_pressed();
	void _reset_form();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIModelProfileDialog();

	void set_model_provider_presets(const Array &p_presets);
	void popup_add_model();
	void popup_edit_model(const Dictionary &p_profile);
	bool is_editing_model() const;
	bool is_editing_custom_model() const;
	String get_editing_profile_id() const;
	Dictionary get_submitted_profile() const;
};
