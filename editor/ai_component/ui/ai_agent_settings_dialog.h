/**************************************************************************/
/*  ai_agent_settings_dialog.h                                             */
/**************************************************************************/

#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/tab_container.h"

class AISettingsModelsPage;
class AISettingsMCPPage;
class AISettingsNextMarqueePage;
class AISettingsRulesPage;
class AISettingsSkillsPage;
class HBoxContainer;

class AIAgentSettingsDialog : public ConfirmationDialog {
	GDCLASS(AIAgentSettingsDialog, ConfirmationDialog);

	enum SettingsPage {
		PAGE_MODELS,
		PAGE_NEXT_MARQUEE,
		PAGE_MCP,
		PAGE_SKILLS,
		PAGE_RULES,
	};

	ItemList *navigation = nullptr;
	TabContainer *pages = nullptr;
	AISettingsModelsPage *models_page = nullptr;
	AISettingsNextMarqueePage *next_marquee_page = nullptr;
	AISettingsMCPPage *mcp_page = nullptr;
	AISettingsSkillsPage *skills_page = nullptr;
	AISettingsRulesPage *rules_page = nullptr;

	static inline AIAgentSettingsDialog *singleton = nullptr;

	void _build_ui();
	void _build_navigation(HBoxContainer *p_root);
	void _build_pages(HBoxContainer *p_root);
	void _navigation_selected(int p_index);
	void _save_settings();
	void _save_model_settings();
	void _save_next_marquee_settings();
	void _save_mcp_settings();
	void _save_skill_settings();
	void _save_rule_settings();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIAgentSettingsDialog();
	~AIAgentSettingsDialog();
	static AIAgentSettingsDialog *get_singleton();

	void build_for_test();
	int get_model_table_row_count_for_test() const;
	int get_custom_model_table_row_count_for_test() const;
	int get_next_marquee_preset_count_for_test() const;
	int get_mcp_server_table_row_count_for_test() const;
	int get_skill_table_row_count_for_test() const;
	int get_rule_table_row_count_for_test() const;
	void add_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key = String());
	void add_custom_model_for_test(const String &p_model, const String &p_base_url, const String &p_api_key);
	void add_mcp_server_for_test(const String &p_display_name, const String &p_command, bool p_enabled = true);
	void add_skill_for_test(const String &p_display_name, const String &p_description, const String &p_content, bool p_enabled = true);
	void add_rule_for_test(const String &p_content, bool p_enabled = true);
	void select_next_marquee_preset_for_test(const String &p_preset_id);
	String add_next_marquee_for_test(const String &p_display_name, const String &p_shader_code);
	void edit_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key);
	void edit_custom_model_for_test(const String &p_current_model, const String &p_new_model, const String &p_base_url, const String &p_api_key);
	void remove_custom_model_for_test(const String &p_provider_id, const String &p_model);
	void save_settings_for_test();
};
