/**************************************************************************/
/*  ai_agent_settings_dialog.cpp                                           */
/**************************************************************************/

#include "ai_agent_settings_dialog.h"

#include "editor/ai_component/ui/ai_settings_models_page.h"
#include "editor/ai_component/ui/ai_settings_mcp_page.h"
#include "editor/ai_component/ui/ai_settings_rules_page.h"
#include "editor/ai_component/ui/ai_settings_skills_page.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/separator.h"

void AIAgentSettingsDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_save_settings"), &AIAgentSettingsDialog::_save_settings);
	ADD_SIGNAL(MethodInfo("ai_settings_changed"));
	ADD_SIGNAL(MethodInfo("ai_mcp_settings_changed"));
	ADD_SIGNAL(MethodInfo("ai_skill_settings_changed"));
	ADD_SIGNAL(MethodInfo("ai_rule_settings_changed"));
}

void AIAgentSettingsDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AIAgentSettingsDialog::AIAgentSettingsDialog() {
	singleton = this;
}

AIAgentSettingsDialog *AIAgentSettingsDialog::get_singleton() {
	return singleton;
}

void AIAgentSettingsDialog::_build_ui() {
	if (pages) {
		return;
	}

	set_title(TTR("NEXT AI Agent Settings"));
	set_min_size(Size2(1040, 640) * EDSCALE);

	HBoxContainer *root = memnew(HBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 12 * EDSCALE);
	add_child(root);

	_build_navigation(root);

	VSeparator *main_separator = memnew(VSeparator);
	root->add_child(main_separator);

	_build_pages(root);

	get_ok_button()->set_text(TTR("OK"));
	connect("confirmed", callable_mp(this, &AIAgentSettingsDialog::_save_settings));
}

void AIAgentSettingsDialog::_build_navigation(HBoxContainer *p_root) {
	navigation = memnew(ItemList);
	navigation->set_custom_minimum_size(Size2(190, 0) * EDSCALE);
	navigation->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	navigation->set_select_mode(ItemList::SELECT_SINGLE);
	navigation->set_auto_height(false);
	navigation->set_icon_mode(ItemList::ICON_MODE_LEFT);
	navigation->set_fixed_icon_size(Size2i(22, 22) * EDSCALE);
	navigation->set_max_columns(1);
	navigation->set_same_column_width(true);
	navigation->add_theme_font_size_override(SceneStringName(font_size), int(15 * EDSCALE));
	navigation->add_theme_constant_override("h_separation", int(16 * EDSCALE));
	navigation->add_theme_constant_override("v_separation", int(14 * EDSCALE));
	navigation->add_theme_constant_override("icon_margin", int(10 * EDSCALE));
	navigation->add_theme_constant_override("line_separation", int(4 * EDSCALE));
	navigation->add_item(TTR("LLM"), get_editor_theme_icon(SNAME("AIModel")));
	navigation->set_item_metadata(PAGE_MODELS, PAGE_MODELS);
	navigation->add_item(TTR("MCP"), get_editor_theme_icon(SNAME("AIMCP")));
	navigation->set_item_metadata(PAGE_MCP, PAGE_MCP);
	navigation->add_item(TTR("Skill"), get_editor_theme_icon(SNAME("AISkill")));
	navigation->set_item_metadata(PAGE_SKILLS, PAGE_SKILLS);
	navigation->add_item(TTR("Rules"), get_editor_theme_icon(SNAME("AIRules")));
	navigation->set_item_metadata(PAGE_RULES, PAGE_RULES);
	navigation->select(PAGE_MODELS);
	navigation->connect(SceneStringName(item_selected), callable_mp(this, &AIAgentSettingsDialog::_navigation_selected));
	p_root->add_child(navigation);
}

void AIAgentSettingsDialog::_build_pages(HBoxContainer *p_root) {
	pages = memnew(TabContainer);
	pages->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	pages->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	pages->set_tabs_visible(false);
	p_root->add_child(pages);

	models_page = memnew(AISettingsModelsPage);
	models_page->set_name(TTR("LLM"));
	models_page->connect("settings_changed", callable_mp(this, &AIAgentSettingsDialog::_save_model_settings));
	pages->add_child(models_page);

	mcp_page = memnew(AISettingsMCPPage);
	mcp_page->set_name(TTR("MCP"));
	mcp_page->connect("settings_changed", callable_mp(this, &AIAgentSettingsDialog::_save_mcp_settings));
	pages->add_child(mcp_page);

	skills_page = memnew(AISettingsSkillsPage);
	skills_page->set_name(TTR("Skill"));
	skills_page->connect("settings_changed", callable_mp(this, &AIAgentSettingsDialog::_save_skill_settings));
	pages->add_child(skills_page);

	rules_page = memnew(AISettingsRulesPage);
	rules_page->set_name(TTR("Rules"));
	rules_page->connect("settings_changed", callable_mp(this, &AIAgentSettingsDialog::_save_rule_settings));
	pages->add_child(rules_page);
}

void AIAgentSettingsDialog::_navigation_selected(int p_index) {
	if (!pages || !navigation || p_index < 0) {
		return;
	}
	pages->set_current_tab((int)navigation->get_item_metadata(p_index));
}

void AIAgentSettingsDialog::_save_settings() {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	settings->save();
	emit_signal(SNAME("ai_settings_changed"));
	emit_signal(SNAME("ai_mcp_settings_changed"));
	emit_signal(SNAME("ai_skill_settings_changed"));
	emit_signal(SNAME("ai_rule_settings_changed"));
}

void AIAgentSettingsDialog::_save_model_settings() {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	settings->save();
	emit_signal(SNAME("ai_settings_changed"));
}

void AIAgentSettingsDialog::_save_mcp_settings() {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	settings->save();
	emit_signal(SNAME("ai_mcp_settings_changed"));
}

void AIAgentSettingsDialog::_save_skill_settings() {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	settings->save();
	emit_signal(SNAME("ai_skill_settings_changed"));
}

void AIAgentSettingsDialog::_save_rule_settings() {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	settings->save();
	emit_signal(SNAME("ai_rule_settings_changed"));
}

void AIAgentSettingsDialog::build_for_test() {
	_build_ui();
	if (models_page) {
		models_page->build_for_test();
	}
	if (mcp_page) {
		mcp_page->build_for_test();
	}
	if (skills_page) {
		skills_page->build_for_test();
	}
	if (rules_page) {
		rules_page->build_for_test();
	}
}

int AIAgentSettingsDialog::get_model_table_row_count_for_test() const {
	return models_page ? models_page->get_model_table_row_count_for_test() : 0;
}

int AIAgentSettingsDialog::get_custom_model_table_row_count_for_test() const {
	return models_page ? models_page->get_custom_model_table_row_count_for_test() : 0;
}

int AIAgentSettingsDialog::get_mcp_server_table_row_count_for_test() const {
	return mcp_page ? mcp_page->get_server_table_row_count_for_test() : 0;
}

int AIAgentSettingsDialog::get_skill_table_row_count_for_test() const {
	return skills_page ? skills_page->get_skill_table_row_count_for_test() : 0;
}

int AIAgentSettingsDialog::get_rule_table_row_count_for_test() const {
	return rules_page ? rules_page->get_rule_table_row_count_for_test() : 0;
}

void AIAgentSettingsDialog::add_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key) {
	ERR_FAIL_NULL(models_page);
	models_page->add_provider_model_for_test(p_provider_id, p_model, p_api_key);
}

void AIAgentSettingsDialog::add_custom_model_for_test(const String &p_model, const String &p_base_url, const String &p_api_key) {
	ERR_FAIL_NULL(models_page);
	models_page->add_custom_model_for_test(p_model, p_base_url, p_api_key);
}

void AIAgentSettingsDialog::add_mcp_server_for_test(const String &p_display_name, const String &p_command, bool p_enabled) {
	ERR_FAIL_NULL(mcp_page);
	mcp_page->add_server_for_test(p_display_name, p_command, p_enabled);
}

void AIAgentSettingsDialog::add_skill_for_test(const String &p_display_name, const String &p_description, const String &p_content, bool p_enabled) {
	ERR_FAIL_NULL(skills_page);
	skills_page->add_skill_for_test(p_display_name, p_description, p_content, p_enabled);
}

void AIAgentSettingsDialog::add_rule_for_test(const String &p_content, bool p_enabled) {
	ERR_FAIL_NULL(rules_page);
	rules_page->add_rule_for_test(p_content, p_enabled);
}

void AIAgentSettingsDialog::edit_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key) {
	ERR_FAIL_NULL(models_page);
	models_page->edit_provider_model_for_test(p_provider_id, p_model, p_api_key);
}

void AIAgentSettingsDialog::edit_custom_model_for_test(const String &p_current_model, const String &p_new_model, const String &p_base_url, const String &p_api_key) {
	ERR_FAIL_NULL(models_page);
	models_page->edit_custom_model_for_test(p_current_model, p_new_model, p_base_url, p_api_key);
}

void AIAgentSettingsDialog::remove_custom_model_for_test(const String &p_provider_id, const String &p_model) {
	ERR_FAIL_NULL(models_page);
	models_page->remove_custom_model_for_test(p_provider_id, p_model);
}

void AIAgentSettingsDialog::save_settings_for_test() {
	_save_settings();
}
