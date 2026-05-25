/**************************************************************************/
/*  ai_settings_rules_page.cpp                                             */
/**************************************************************************/

#include "ai_settings_rules_page.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "servers/text/text_server.h"

namespace {

void _clear_children(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	while (p_node->get_child_count() > 0) {
		Node *child = p_node->get_child(0);
		p_node->remove_child(child);
		memdelete(child);
	}
}

Label *_make_table_label(const String &p_text, int p_width = 0) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	label->set_clip_text(true);
	if (p_width > 0) {
		label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	} else {
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	}
	return label;
}

} // namespace

void AISettingsRulesPage::_bind_methods() {
	ADD_SIGNAL(MethodInfo("settings_changed"));
}

void AISettingsRulesPage::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AISettingsRulesPage::AISettingsRulesPage() {
}

void AISettingsRulesPage::_build_ui() {
	if (rule_table) {
		return;
	}

	add_theme_constant_override("margin_left", 8 * EDSCALE);
	add_theme_constant_override("margin_right", 8 * EDSCALE);
	add_theme_constant_override("margin_top", 8 * EDSCALE);
	add_theme_constant_override("margin_bottom", 8 * EDSCALE);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(scroll);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 12 * EDSCALE);
	scroll->add_child(content);

	Label *title = memnew(Label);
	title->set_text(TTR("Rules"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *section_title = memnew(Label);
	section_title->set_text(TTR("User Rules"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(TTR("Rules are short user instructions injected into every AI Agent context. Each rule is limited to 100 characters."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->add_theme_constant_override("separation", 8 * EDSCALE);
	content->add_child(toolbar);

	rule_input = memnew(LineEdit);
	rule_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	rule_input->set_max_length(AIRuleSettings::MAX_RULE_CHARS);
	rule_input->set_placeholder(TTR("Add a rule, up to 100 characters."));
	rule_input->connect(SceneStringName(text_submitted), callable_mp(this, &AISettingsRulesPage::_rule_text_submitted));
	toolbar->add_child(rule_input);

	add_rule_button = memnew(Button);
	add_rule_button->set_text(TTR("Add Rule"));
	add_rule_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsRulesPage::_add_rule_pressed));
	toolbar->add_child(add_rule_button);

	status_label = memnew(Label);
	status_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	status_label->hide();
	content->add_child(status_label);

	PanelContainer *table_panel = memnew(PanelContainer);
	table_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	table_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(table_panel);

	rule_table = memnew(VBoxContainer);
	rule_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	rule_table->add_theme_constant_override("separation", 0);
	table_panel->add_child(rule_table);

	_refresh_rule_table();
}

void AISettingsRulesPage::_refresh_rule_table() {
	ERR_FAIL_NULL(rule_table);

	_clear_children(rule_table);
	rule_rows.clear();

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_custom_minimum_size(Size2(0, 32) * EDSCALE);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_theme_constant_override("separation", 8 * EDSCALE);
	rule_table->add_child(header);

	Label *enabled_header = _make_table_label(TTR("Enabled"), 82);
	enabled_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(enabled_header);

	Label *rule_header = _make_table_label(TTR("Rule"));
	rule_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(rule_header);

	Label *action_header = _make_table_label(TTR("Actions"), 150);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	HSeparator *header_separator = memnew(HSeparator);
	rule_table->add_child(header_separator);

	Vector<AIRuleConfig> rules = AIRuleSettings::get_rules(false);
	for (int i = 0; i < rules.size(); i++) {
		_add_rule_table_row(rules[i]);
	}

	if (rules.is_empty()) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		rule_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(TTR("No rules yet. Add a short rule to include it in AI Agent context."));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AISettingsRulesPage::_add_rule_table_row(const AIRuleConfig &p_rule) {
	rule_rows.push_back(p_rule);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	rule_table->add_child(row);

	CheckBox *enabled_check = memnew(CheckBox);
	enabled_check->set_pressed(p_rule.enabled);
	enabled_check->set_custom_minimum_size(Size2(82, 0) * EDSCALE);
	enabled_check->connect(SceneStringName(toggled), callable_mp(this, &AISettingsRulesPage::_rule_enabled_toggled).bind(p_rule.id), CONNECT_DEFERRED);
	row->add_child(enabled_check);

	Label *rule_label = _make_table_label(p_rule.content);
	rule_label->set_tooltip_text(p_rule.content);
	row->add_child(rule_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	Button *edit_button = memnew(Button);
	edit_button->set_text(TTR("Edit"));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsRulesPage::_edit_rule_pressed).bind(p_rule.id), CONNECT_DEFERRED);
	action_cell->add_child(edit_button);

	Button *remove_button = memnew(Button);
	remove_button->set_text(TTR("Remove"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsRulesPage::_remove_rule_pressed).bind(p_rule.id), CONNECT_DEFERRED);
	action_cell->add_child(remove_button);

	HSeparator *row_separator = memnew(HSeparator);
	rule_table->add_child(row_separator);
}

void AISettingsRulesPage::_add_rule_pressed() {
	if (!rule_input) {
		return;
	}
	const String content = AIRuleSettings::normalize_rule_content(rule_input->get_text());
	if (content.is_empty()) {
		_set_status(TTR("Rule cannot be empty."), true);
		return;
	}
	if (!editing_rule_id.is_empty()) {
		AIRuleConfig existing = AIRuleSettings::get_rule(editing_rule_id);
		if (existing.id.is_empty() || !AIRuleSettings::update_rule(editing_rule_id, content, existing.enabled)) {
			_set_status(TTR("Could not save rule."), true);
			return;
		}
		editing_rule_id.clear();
		if (add_rule_button) {
			add_rule_button->set_text(TTR("Add Rule"));
		}
	} else {
		if (AIRuleSettings::add_rule(content, true).is_empty()) {
			_set_status(TTR("Could not add rule."), true);
			return;
		}
	}
	rule_input->clear();
	_set_status(String(), false);
	_refresh_rule_table();
	emit_signal(SNAME("settings_changed"));
}

void AISettingsRulesPage::_rule_text_submitted(const String &p_text) {
	(void)p_text;
	_add_rule_pressed();
}

void AISettingsRulesPage::_edit_rule_pressed(const String &p_rule_id) {
	if (!rule_input) {
		return;
	}
	AIRuleConfig rule = AIRuleSettings::get_rule(p_rule_id);
	if (rule.id.is_empty()) {
		return;
	}
	editing_rule_id = p_rule_id;
	rule_input->set_text(rule.content);
	rule_input->grab_focus();
	if (add_rule_button) {
		add_rule_button->set_text(TTR("Save Rule"));
	}
}

void AISettingsRulesPage::_remove_rule_pressed(const String &p_rule_id) {
	if (AIRuleSettings::remove_rule(p_rule_id)) {
		if (editing_rule_id == p_rule_id) {
			editing_rule_id.clear();
			if (rule_input) {
				rule_input->clear();
			}
			if (add_rule_button) {
				add_rule_button->set_text(TTR("Add Rule"));
			}
		}
		_refresh_rule_table();
		emit_signal(SNAME("settings_changed"));
	}
}

void AISettingsRulesPage::_rule_enabled_toggled(bool p_enabled, const String &p_rule_id) {
	if (AIRuleSettings::set_rule_enabled(p_rule_id, p_enabled)) {
		_refresh_rule_table();
		emit_signal(SNAME("settings_changed"));
	}
}

void AISettingsRulesPage::_set_status(const String &p_status, bool p_error) {
	if (!status_label) {
		return;
	}
	status_label->set_text(p_status);
	status_label->set_visible(!p_status.is_empty());
	status_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(p_error ? SNAME("error_color") : SNAME("disabled_font_color"), EditorStringName(Editor)));
}

void AISettingsRulesPage::build_for_test() {
	_build_ui();
}

int AISettingsRulesPage::get_rule_table_row_count_for_test() const {
	return rule_rows.size();
}

void AISettingsRulesPage::add_rule_for_test(const String &p_content, bool p_enabled) {
	(void)AIRuleSettings::add_rule(p_content, p_enabled);
	_refresh_rule_table();
}

void AISettingsRulesPage::set_rule_enabled_for_test(const String &p_rule_id, bool p_enabled) {
	AIRuleSettings::set_rule_enabled(p_rule_id, p_enabled);
	_refresh_rule_table();
}
