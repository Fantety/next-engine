/**************************************************************************/
/*  ai_settings_skills_page.h                                              */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"

#include "scene/gui/margin_container.h"

class Button;
class EditorFileDialog;
class Label;
class VBoxContainer;

class AISettingsSkillsPage : public MarginContainer {
	GDCLASS(AISettingsSkillsPage, MarginContainer);

	VBoxContainer *skill_table = nullptr;
	Button *add_skill_button = nullptr;
	Button *import_skill_button = nullptr;
	EditorFileDialog *import_skill_dialog = nullptr;
	Label *status_label = nullptr;
	Vector<Dictionary> skill_rows;

	Ref<AIAgentV1UIBridge> _get_adapter() const;
	void _build_ui();
	void _refresh_skill_table();
	void _add_skill_table_row(const Dictionary &p_skill);
	void _popup_import_skill_dialog();
	void _skill_folder_selected(const String &p_folder_path);
	void _remove_skill_pressed(const String &p_skill_id);
	void _skill_enabled_toggled(bool p_enabled, const String &p_skill_id);
	void _skill_status_changed(const Array &p_statuses, const Dictionary &p_summary);
	void _config_changed(const String &p_scope, const Dictionary &p_config);
	Array _get_skill_rows() const;
	Array _get_skill_sources() const;
	bool _patch_skill_sources(const Array &p_sources, const String &p_scope = "project");
	void _set_status(const String &p_status, bool p_error);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsSkillsPage();

	void build_for_test();
	int get_skill_table_row_count_for_test() const;
	void add_skill_for_test(const String &p_display_name, const String &p_description, const String &p_content, bool p_enabled = true);
	void set_skill_enabled_for_test(const String &p_skill_id, bool p_enabled);
};
