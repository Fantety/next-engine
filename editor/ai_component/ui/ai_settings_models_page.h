/**************************************************************************/
/*  ai_settings_models_page.h                                             */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_model_settings.h"
#include "scene/gui/margin_container.h"

class AIModelProfileDialog;
class Button;
class VBoxContainer;

class AISettingsModelsPage : public MarginContainer {
	GDCLASS(AISettingsModelsPage, MarginContainer);

	struct ModelTableRow {
		String profile_id;
		String display_name;
		String provider_id;
		String model;
		bool custom = false;
	};

	VBoxContainer *model_table = nullptr;
	Vector<ModelTableRow> model_table_rows;
	Button *add_model_button = nullptr;
	AIModelProfileDialog *profile_dialog = nullptr;

	void _build_ui();
	void _refresh_model_table();
	void _add_model_table_row(const AIModelProfile &p_profile);
	void _popup_add_model_dialog();
	void _edit_model_pressed(const String &p_profile_id);
	void _profile_submitted();
	void _remove_model_pressed(const String &p_profile_id);
	void _append_custom_model(const String &p_provider_id, const String &p_model);
	void _remove_custom_model_if_unused(const String &p_provider_id, const String &p_model, const String &p_ignored_profile_id = String());

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsModelsPage();

	void build_for_test();
	int get_model_table_row_count_for_test() const;
	int get_custom_model_table_row_count_for_test() const;
	void add_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key = String());
	void add_custom_model_for_test(const String &p_model, const String &p_base_url, const String &p_api_key);
	void edit_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key);
	void edit_custom_model_for_test(const String &p_current_model, const String &p_new_model, const String &p_base_url, const String &p_api_key);
	void remove_custom_model_for_test(const String &p_provider_id, const String &p_model);
};
