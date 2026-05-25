/**************************************************************************/
/*  ai_settings_rules_page.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/rules/ai_rule_settings.h"
#include "scene/gui/margin_container.h"

class Button;
class Label;
class LineEdit;
class VBoxContainer;

class AISettingsRulesPage : public MarginContainer {
	GDCLASS(AISettingsRulesPage, MarginContainer);

	VBoxContainer *rule_table = nullptr;
	LineEdit *rule_input = nullptr;
	Button *add_rule_button = nullptr;
	Label *status_label = nullptr;
	Vector<AIRuleConfig> rule_rows;
	String editing_rule_id;

	void _build_ui();
	void _refresh_rule_table();
	void _add_rule_table_row(const AIRuleConfig &p_rule);
	void _add_rule_pressed();
	void _rule_text_submitted(const String &p_text);
	void _edit_rule_pressed(const String &p_rule_id);
	void _remove_rule_pressed(const String &p_rule_id);
	void _rule_enabled_toggled(bool p_enabled, const String &p_rule_id);
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
