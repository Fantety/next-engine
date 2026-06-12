/**************************************************************************/
/*  ai_settings_rules_page.h                                               */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"

#include "scene/gui/margin_container.h"

class Button;
class Label;
class LineEdit;
class OptionButton;
class VBoxContainer;

class AISettingsRulesPage : public MarginContainer {
	GDCLASS(AISettingsRulesPage, MarginContainer);

	VBoxContainer *rule_table = nullptr;
	LineEdit *action_input = nullptr;
	LineEdit *resource_input = nullptr;
	OptionButton *effect_option = nullptr;
	LineEdit *reason_input = nullptr;
	Button *add_rule_button = nullptr;
	Label *status_label = nullptr;
	Vector<Dictionary> rule_rows;
	int editing_rule_index = -1;

	Ref<AIAgentV1UIBridge> _get_adapter() const;
	void _build_ui();
	void _refresh_rule_table();
	void _add_rule_table_row(const Dictionary &p_rule, int p_rule_index);
	void _add_rule_pressed();
	void _rule_text_submitted(const String &p_text);
	void _edit_rule_pressed(int p_rule_index);
	void _remove_rule_pressed(int p_rule_index);
	void _rules_changed(const Array &p_rules);
	void _config_changed(const String &p_scope, const Dictionary &p_config);
	Array _get_rules() const;
	bool _patch_rules(const Array &p_rules, const String &p_scope = "project");
	void _set_status(const String &p_status, bool p_error);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsRulesPage();

	void build_for_test();
	int get_rule_table_row_count_for_test() const;
	void add_rule_for_test(const String &p_content, bool p_enabled = true);
	void set_rule_enabled_for_test(const String &p_rule_id, bool p_enabled);
};
