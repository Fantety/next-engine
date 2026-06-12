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
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
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

Array _array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

Label *_make_field_label(const String &p_text, int p_width) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	label->add_theme_color_override(SceneStringName(font_color), label->get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	return label;
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

String _normalize_effect(const Variant &p_effect) {
	const String effect = String(p_effect).strip_edges().to_lower();
	if (effect == "allow" || effect == "ask" || effect == "deny") {
		return effect;
	}
	return "ask";
}

String _effect_label(const String &p_effect) {
	if (p_effect == "allow") {
		return TTR("Allow");
	}
	if (p_effect == "deny") {
		return TTR("Deny");
	}
	return TTR("Ask");
}

String _rule_action(const Dictionary &p_rule) {
	return String(p_rule.get("action", String())).strip_edges().to_lower();
}

String _rule_resource(const Dictionary &p_rule) {
	const String resource = String(p_rule.get("resource", "*")).strip_edges();
	return resource.is_empty() ? String("*") : resource;
}

String _rule_effect(const Dictionary &p_rule) {
	return _normalize_effect(p_rule.get("effect", "ask"));
}

String _rule_reason(const Dictionary &p_rule) {
	return String(p_rule.get("reason", String())).strip_edges();
}

Dictionary _normalize_rule_for_patch(Dictionary p_rule) {
	p_rule["action"] = _rule_action(p_rule);
	p_rule["resource"] = _rule_resource(p_rule);
	p_rule["effect"] = _rule_effect(p_rule);
	const String reason = _rule_reason(p_rule);
	if (reason.is_empty()) {
		p_rule.erase("reason");
	} else {
		p_rule["reason"] = reason;
	}
	return p_rule;
}

int _effect_to_option_index(const String &p_effect) {
	if (p_effect == "allow") {
		return 0;
	}
	if (p_effect == "deny") {
		return 2;
	}
	return 1;
}

String _effect_from_option(const OptionButton *p_option) {
	if (!p_option) {
		return "ask";
	}
	switch (p_option->get_selected_id()) {
		case 0:
			return "allow";
		case 2:
			return "deny";
		default:
			return "ask";
	}
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

Ref<AIAgentV1UIBridge> AISettingsRulesPage::_get_adapter() const {
	return AIAgentV1UIBridge::get_singleton();
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
	section_title->set_text(TTR("Permission Rules"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(TTR("agent_v1 evaluates permission rules by action, resource pattern, and effect. Later matching rules take precedence."));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	content->add_child(description);

	VBoxContainer *form = memnew(VBoxContainer);
	form->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	form->add_theme_constant_override("separation", 8 * EDSCALE);
	content->add_child(form);

	HBoxContainer *first_row = memnew(HBoxContainer);
	first_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	first_row->add_theme_constant_override("separation", 8 * EDSCALE);
	form->add_child(first_row);

	first_row->add_child(_make_field_label(TTR("Action"), 68));
	action_input = memnew(LineEdit);
	action_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	action_input->set_placeholder(TTR("file.write"));
	action_input->connect(SceneStringName(text_submitted), callable_mp(this, &AISettingsRulesPage::_rule_text_submitted));
	first_row->add_child(action_input);

	first_row->add_child(_make_field_label(TTR("Resource"), 78));
	resource_input = memnew(LineEdit);
	resource_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	resource_input->set_placeholder(TTR("*"));
	resource_input->connect(SceneStringName(text_submitted), callable_mp(this, &AISettingsRulesPage::_rule_text_submitted));
	first_row->add_child(resource_input);

	first_row->add_child(_make_field_label(TTR("Effect"), 62));
	effect_option = memnew(OptionButton);
	effect_option->set_custom_minimum_size(Size2(110, 0) * EDSCALE);
	effect_option->add_item(TTR("Allow"), 0);
	effect_option->add_item(TTR("Ask"), 1);
	effect_option->add_item(TTR("Deny"), 2);
	effect_option->select(1);
	first_row->add_child(effect_option);

	HBoxContainer *second_row = memnew(HBoxContainer);
	second_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	second_row->add_theme_constant_override("separation", 8 * EDSCALE);
	form->add_child(second_row);

	second_row->add_child(_make_field_label(TTR("Reason"), 68));
	reason_input = memnew(LineEdit);
	reason_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	reason_input->set_placeholder(TTR("Optional note shown with the decision."));
	reason_input->connect(SceneStringName(text_submitted), callable_mp(this, &AISettingsRulesPage::_rule_text_submitted));
	second_row->add_child(reason_input);

	add_rule_button = memnew(Button);
	add_rule_button->set_text(TTR("Add Rule"));
	add_rule_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsRulesPage::_add_rule_pressed));
	second_row->add_child(add_rule_button);

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

	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_valid()) {
		const Callable rules_changed = callable_mp(this, &AISettingsRulesPage::_rules_changed);
		if (!bridge->is_connected(SNAME("rules_changed"), rules_changed)) {
			bridge->connect(SNAME("rules_changed"), rules_changed);
		}
		const Callable config_changed = callable_mp(this, &AISettingsRulesPage::_config_changed);
		if (!bridge->is_connected(SNAME("config_changed"), config_changed)) {
			bridge->connect(SNAME("config_changed"), config_changed);
		}
	}

	_refresh_rule_table();
}

Array AISettingsRulesPage::_get_rules() const {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return Array();
	}
	const Dictionary snapshot = bridge->get_settings_snapshot();
	return _array_from_variant(snapshot.get("rules", Array()));
}

bool AISettingsRulesPage::_patch_rules(const Array &p_rules, const String &p_scope) {
	Ref<AIAgentV1UIBridge> bridge = _get_adapter();
	if (bridge.is_null()) {
		return false;
	}

	Dictionary permissions_patch;
	permissions_patch["rules"] = p_rules.duplicate(true);
	Dictionary patch;
	patch["permissions"] = permissions_patch;
	const Dictionary result = bridge->patch_settings(patch, p_scope);
	return bool(result.get("success", false));
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

	Label *action_header = _make_table_label(TTR("Action"), 150);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	Label *resource_header = _make_table_label(TTR("Resource"), 220);
	resource_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(resource_header);

	Label *effect_header = _make_table_label(TTR("Effect"), 90);
	effect_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(effect_header);

	Label *reason_header = _make_table_label(TTR("Reason"));
	reason_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(reason_header);

	Label *row_action_header = _make_table_label(TTR("Actions"), 150);
	row_action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(row_action_header);

	HSeparator *header_separator = memnew(HSeparator);
	rule_table->add_child(header_separator);

	const Array rules = _get_rules();
	for (int i = 0; i < rules.size(); i++) {
		if (rules[i].get_type() == Variant::DICTIONARY) {
			_add_rule_table_row(rules[i], i);
		}
	}

	if (rule_rows.is_empty()) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		rule_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(TTR("No permission rules yet."));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AISettingsRulesPage::_add_rule_table_row(const Dictionary &p_rule, int p_rule_index) {
	Dictionary rule = _normalize_rule_for_patch(p_rule.duplicate(true));
	rule_rows.push_back(rule);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	rule_table->add_child(row);

	const String action = _rule_action(rule);
	Label *action_label = _make_table_label(action, 150);
	action_label->set_tooltip_text(action);
	row->add_child(action_label);

	const String resource = _rule_resource(rule);
	Label *resource_label = _make_table_label(resource, 220);
	resource_label->set_tooltip_text(resource);
	row->add_child(resource_label);

	const String effect = _rule_effect(rule);
	Label *effect_label = _make_table_label(_effect_label(effect), 90);
	effect_label->set_tooltip_text(effect);
	row->add_child(effect_label);

	const String reason = _rule_reason(rule);
	Label *reason_label = _make_table_label(reason);
	reason_label->set_tooltip_text(reason);
	row->add_child(reason_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	Button *edit_button = memnew(Button);
	edit_button->set_text(TTR("Edit"));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsRulesPage::_edit_rule_pressed).bind(p_rule_index), CONNECT_DEFERRED);
	action_cell->add_child(edit_button);

	Button *remove_button = memnew(Button);
	remove_button->set_text(TTR("Remove"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AISettingsRulesPage::_remove_rule_pressed).bind(p_rule_index), CONNECT_DEFERRED);
	action_cell->add_child(remove_button);

	HSeparator *row_separator = memnew(HSeparator);
	rule_table->add_child(row_separator);
}

void AISettingsRulesPage::_add_rule_pressed() {
	if (!action_input || !resource_input || !effect_option || !reason_input) {
		return;
	}

	Dictionary rule;
	rule["action"] = action_input->get_text();
	rule["resource"] = resource_input->get_text();
	rule["effect"] = _effect_from_option(effect_option);
	rule["reason"] = reason_input->get_text();
	rule = _normalize_rule_for_patch(rule);

	if (_rule_action(rule).is_empty()) {
		_set_status(TTR("Permission action cannot be empty."), true);
		return;
	}

	Array rules = _get_rules();
	if (editing_rule_index >= 0) {
		if (editing_rule_index >= rules.size()) {
			_set_status(TTR("Rule is no longer available."), true);
			return;
		}
		rules[editing_rule_index] = rule;
	} else {
		rules.push_back(rule);
	}

	if (!_patch_rules(rules)) {
		_set_status(TTR("Could not save permission rule."), true);
		return;
	}

	editing_rule_index = -1;
	action_input->clear();
	resource_input->clear();
	reason_input->clear();
	effect_option->select(1);
	if (add_rule_button) {
		add_rule_button->set_text(TTR("Add Rule"));
	}
	_set_status(String(), false);
	_refresh_rule_table();
	emit_signal(SNAME("settings_changed"));
}

void AISettingsRulesPage::_rule_text_submitted(const String &p_text) {
	(void)p_text;
	_add_rule_pressed();
}

void AISettingsRulesPage::_edit_rule_pressed(int p_rule_index) {
	if (!action_input || !resource_input || !effect_option || !reason_input) {
		return;
	}

	const Array rules = _get_rules();
	if (p_rule_index < 0 || p_rule_index >= rules.size() || rules[p_rule_index].get_type() != Variant::DICTIONARY) {
		return;
	}

	const Dictionary rule = _normalize_rule_for_patch(Dictionary(rules[p_rule_index]).duplicate(true));
	editing_rule_index = p_rule_index;
	action_input->set_text(_rule_action(rule));
	resource_input->set_text(_rule_resource(rule));
	reason_input->set_text(_rule_reason(rule));
	effect_option->select(_effect_to_option_index(_rule_effect(rule)));
	action_input->grab_focus();
	if (add_rule_button) {
		add_rule_button->set_text(TTR("Save Rule"));
	}
}

void AISettingsRulesPage::_remove_rule_pressed(int p_rule_index) {
	Array rules = _get_rules();
	if (p_rule_index < 0 || p_rule_index >= rules.size()) {
		return;
	}

	rules.remove_at(p_rule_index);
	if (_patch_rules(rules)) {
		if (editing_rule_index == p_rule_index) {
			editing_rule_index = -1;
			if (action_input) {
				action_input->clear();
			}
			if (resource_input) {
				resource_input->clear();
			}
			if (reason_input) {
				reason_input->clear();
			}
			if (effect_option) {
				effect_option->select(1);
			}
			if (add_rule_button) {
				add_rule_button->set_text(TTR("Add Rule"));
			}
		} else if (editing_rule_index > p_rule_index) {
			editing_rule_index--;
		}
		_refresh_rule_table();
		emit_signal(SNAME("settings_changed"));
	}
}

void AISettingsRulesPage::_rules_changed(const Array &p_rules) {
	(void)p_rules;
	_refresh_rule_table();
}

void AISettingsRulesPage::_config_changed(const String &p_scope, const Dictionary &p_config) {
	(void)p_scope;
	(void)p_config;
	_refresh_rule_table();
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
	const String action = p_content.strip_edges().to_lower();
	if (action.is_empty()) {
		return;
	}

	Dictionary rule;
	rule["action"] = action;
	rule["resource"] = "*";
	rule["effect"] = p_enabled ? String("allow") : String("deny");

	Array rules = _get_rules();
	rules.push_back(rule);
	(void)_patch_rules(rules, "runtime");
	_refresh_rule_table();
}

void AISettingsRulesPage::set_rule_enabled_for_test(const String &p_rule_id, bool p_enabled) {
	const int index = p_rule_id.is_valid_int() ? p_rule_id.to_int() : -1;
	Array rules = _get_rules();
	if (index < 0 || index >= rules.size() || rules[index].get_type() != Variant::DICTIONARY) {
		_refresh_rule_table();
		return;
	}

	Dictionary rule = Dictionary(rules[index]).duplicate(true);
	rule["effect"] = p_enabled ? String("allow") : String("deny");
	rules[index] = _normalize_rule_for_patch(rule);
	(void)_patch_rules(rules, "runtime");
	_refresh_rule_table();
}
