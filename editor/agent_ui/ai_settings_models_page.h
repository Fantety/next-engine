/**************************************************************************/
/*  ai_settings_models_page.h                                             */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"

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
	String model_profile_scope = "project";

	Ref<AIAgentV1UIBridge> _get_adapter();
	void _build_ui();
	void _refresh_model_table();
	void _add_model_table_row(const Dictionary &p_profile);
	void _popup_add_model_dialog();
	void _edit_model_pressed(const String &p_profile_id);
	void _profile_submitted();
	void _remove_model_pressed(const String &p_profile_id);
	Dictionary _find_model_profile(const String &p_provider_id, const String &p_model, bool p_custom_only) const;

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
