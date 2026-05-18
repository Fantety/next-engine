/**************************************************************************/
/*  ai_agent_settings_dialog.h                                             */
/**************************************************************************/

#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tab_container.h"

class Button;
class OptionButton;

class AIAgentSettingsDialog : public ConfirmationDialog {
	GDCLASS(AIAgentSettingsDialog, ConfirmationDialog);

	enum SettingsPage {
		PAGE_MODELS,
		PAGE_MCP,
		PAGE_SKILLS,
		PAGE_RULES,
	};

	struct ModelTableRow {
		String provider_id;
		String model;
		bool custom = false;
	};

	ItemList *navigation = nullptr;
	TabContainer *pages = nullptr;
	VBoxContainer *model_table = nullptr;
	Vector<ModelTableRow> model_table_rows;

	Button *add_model_button = nullptr;
	ConfirmationDialog *add_model_dialog = nullptr;
	TabContainer *add_model_tabs = nullptr;
	OptionButton *provider_model_provider = nullptr;
	OptionButton *provider_model_model = nullptr;
	LineEdit *provider_model_api_key = nullptr;
	Button *provider_model_submit_button = nullptr;
	OptionButton *custom_api_format = nullptr;
	LineEdit *custom_base_url = nullptr;
	CheckButton *custom_full_url = nullptr;
	LineEdit *custom_model_id = nullptr;
	CheckButton *custom_multimodal = nullptr;
	LineEdit *custom_api_key = nullptr;
	Button *custom_model_submit_button = nullptr;
	bool editing_model = false;
	String editing_provider_id;
	String editing_model_id;
	bool editing_custom = false;

	static inline AIAgentSettingsDialog *singleton = nullptr;

	void _build_ui();
	void _build_navigation(HBoxContainer *p_root);
	void _build_pages(HBoxContainer *p_root);
	void _build_models_page(Control *p_page);
	void _build_add_model_dialog();
	void _build_provider_add_tab(Control *p_page);
	void _build_custom_add_tab(Control *p_page);
	void _add_placeholder_page(Control *p_page, const String &p_title);
	void _refresh_model_table();
	void _add_model_table_row(const String &p_provider_id, const String &p_provider_name, const String &p_model, bool p_custom);
	void _navigation_selected(int p_index);
	void _popup_add_model_dialog();
	void _edit_model_pressed(const String &p_provider_id, const String &p_model, bool p_custom);
	void _provider_model_provider_selected(int p_index);
	void _add_model_confirmed();
	void _remove_model_pressed(const String &p_provider_id, const String &p_model, bool p_custom);
	void _append_custom_model(const String &p_provider_id, const String &p_model);
	void _reset_add_model_dialog();
	void _save_settings();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIAgentSettingsDialog();
	static AIAgentSettingsDialog *get_singleton();

	void build_for_test();
	int get_model_table_row_count_for_test() const;
	int get_custom_model_table_row_count_for_test() const;
	void add_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key = String());
	void add_custom_model_for_test(const String &p_model, const String &p_base_url, const String &p_api_key);
	void edit_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key);
	void edit_custom_model_for_test(const String &p_current_model, const String &p_new_model, const String &p_base_url, const String &p_api_key);
	void remove_custom_model_for_test(const String &p_provider_id, const String &p_model);
	void save_settings_for_test();
};
