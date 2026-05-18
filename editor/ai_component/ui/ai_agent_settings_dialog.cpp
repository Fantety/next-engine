/**************************************************************************/
/*  ai_agent_settings_dialog.cpp                                           */
/**************************************************************************/

#include "ai_agent_settings_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"

void AIAgentSettingsDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_save_settings"), &AIAgentSettingsDialog::_save_settings);
	ADD_SIGNAL(MethodInfo("ai_settings_changed"));
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

	set_title(TTR("AI Agent Settings"));
	set_min_size(Size2(860, 560) * EDSCALE);

	HBoxContainer *root = memnew(HBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 12 * EDSCALE);
	add_child(root);

	_build_navigation(root);
	_build_pages(root);

	get_ok_button()->set_text(TTR("Save"));
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
	navigation->add_item(TTR("Models"), get_editor_theme_icon(SNAME("AIModel")));
	navigation->set_item_metadata(PAGE_MODELS, PAGE_MODELS);
	navigation->add_item(TTR("MCP"), get_editor_theme_icon(SNAME("AIMCP")));
	navigation->set_item_metadata(PAGE_MCP, PAGE_MCP);
	navigation->add_item(TTR("Skills"), get_editor_theme_icon(SNAME("AISkill")));
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

	MarginContainer *models_page = memnew(MarginContainer);
	models_page->set_name(TTR("Models"));
	pages->add_child(models_page);
	_build_models_page(models_page);

	MarginContainer *mcp_page = memnew(MarginContainer);
	mcp_page->set_name(TTR("MCP"));
	pages->add_child(mcp_page);
	_add_placeholder_page(mcp_page, TTR("MCP configuration is not implemented yet."));

	MarginContainer *skills_page = memnew(MarginContainer);
	skills_page->set_name(TTR("Skills"));
	pages->add_child(skills_page);
	_add_placeholder_page(skills_page, TTR("Skill configuration is not implemented yet."));

	MarginContainer *rules_page = memnew(MarginContainer);
	rules_page->set_name(TTR("Rules"));
	pages->add_child(rules_page);
	_add_placeholder_page(rules_page, TTR("Rules configuration is not implemented yet."));
}

void AIAgentSettingsDialog::_build_models_page(Control *p_page) {
	MarginContainer *margin = Object::cast_to<MarginContainer>(p_page);
	if (margin) {
		margin->add_theme_constant_override("margin_left", 8 * EDSCALE);
		margin->add_theme_constant_override("margin_right", 8 * EDSCALE);
		margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
	}

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	p_page->add_child(scroll);

	VBoxContainer *main = memnew(VBoxContainer);
	main->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main->add_theme_constant_override("separation", 12 * EDSCALE);
	scroll->add_child(main);

	Label *models_title = memnew(Label);
	models_title->set_text(TTR("Model Providers"));
	models_title->add_theme_font_size_override("font_size", 18 * EDSCALE);
	main->add_child(models_title);

	Vector<AIModelProviderPreset> presets = AIModelSettings::get_provider_presets();
	for (int i = 0; i < presets.size(); i++) {
		_add_provider_section(main, presets[i].id, presets[i].display_name, presets[i].default_base_url, presets[i].preset_models);
	}
}

void AIAgentSettingsDialog::_add_provider_section(VBoxContainer *p_parent, const String &p_provider_id, const String &p_display_name, const String &p_default_base_url, const Vector<String> &p_models) {
	if (!provider_controls.is_empty()) {
		HSeparator *separator = memnew(HSeparator);
		p_parent->add_child(separator);
	}

	ProviderControls controls;
	controls.provider_id = p_provider_id;
	controls.preset_models = p_models;

	Label *provider_label = memnew(Label);
	provider_label->set_text(p_display_name);
	provider_label->add_theme_font_size_override("font_size", 15 * EDSCALE);
	p_parent->add_child(provider_label);

	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(2);
	grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_parent->add_child(grid);

	Label *key_label = memnew(Label);
	key_label->set_text(TTR("API Key:"));
	grid->add_child(key_label);

	controls.api_key = memnew(LineEdit);
	controls.api_key->set_secret(true);
	controls.api_key->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	controls.api_key->set_text(AIModelSettings::get_provider_api_key(p_provider_id));
	grid->add_child(controls.api_key);

	Label *url_label = memnew(Label);
	url_label->set_text(TTR("Base URL:"));
	grid->add_child(url_label);

	controls.base_url = memnew(LineEdit);
	controls.base_url->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	controls.base_url->set_placeholder(p_default_base_url);
	controls.base_url->set_text(AIModelSettings::get_provider_base_url(p_provider_id));
	grid->add_child(controls.base_url);

	Label *preset_label = memnew(Label);
	preset_label->set_text(TTR("Preset Models"));
	p_parent->add_child(preset_label);

	GridContainer *models_grid = memnew(GridContainer);
	models_grid->set_columns(2);
	models_grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p_parent->add_child(models_grid);

	for (int i = 0; i < p_models.size(); i++) {
		CheckButton *model_check = memnew(CheckButton);
		model_check->set_text(p_models[i]);
		model_check->set_pressed(AIModelSettings::is_model_enabled(p_provider_id, p_models[i]));
		models_grid->add_child(model_check);
		controls.preset_model_checks.push_back(model_check);
	}

	Label *custom_label = memnew(Label);
	custom_label->set_text(TTR("Custom Models"));
	p_parent->add_child(custom_label);

	controls.custom_models = memnew(TextEdit);
	controls.custom_models->set_custom_minimum_size(Size2(0, 72) * EDSCALE);
	controls.custom_models->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	controls.custom_models->set_placeholder(TTR("One model id per line"));
	controls.custom_models->set_text(AIModelSettings::get_custom_models(p_provider_id));
	p_parent->add_child(controls.custom_models);

	provider_controls.push_back(controls);
}

void AIAgentSettingsDialog::_add_placeholder_page(Control *p_page, const String &p_title) {
	MarginContainer *margin = Object::cast_to<MarginContainer>(p_page);
	if (margin) {
		margin->add_theme_constant_override("margin_left", 16 * EDSCALE);
		margin->add_theme_constant_override("margin_right", 16 * EDSCALE);
		margin->add_theme_constant_override("margin_top", 16 * EDSCALE);
		margin->add_theme_constant_override("margin_bottom", 16 * EDSCALE);
	}

	Label *label = memnew(Label);
	label->set_text(p_title);
	label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	p_page->add_child(label);
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

	for (int i = 0; i < provider_controls.size(); i++) {
		ProviderControls &controls = provider_controls.write[i];
		AIModelSettings::set_provider_auth(controls.provider_id, controls.api_key->get_text(), controls.base_url->get_text());
		for (int j = 0; j < controls.preset_models.size(); j++) {
			AIModelSettings::set_model_enabled(controls.provider_id, controls.preset_models[j], controls.preset_model_checks[j]->is_pressed());
		}
		AIModelSettings::set_custom_models(controls.provider_id, controls.custom_models->get_text());
		PackedStringArray custom_models = controls.custom_models->get_text().replace(",", "\n").split("\n", false);
		for (int j = 0; j < custom_models.size(); j++) {
			String custom_model = custom_models[j].strip_edges();
			if (!custom_model.is_empty()) {
				AIModelSettings::set_model_enabled(controls.provider_id, custom_model, true);
			}
		}
	}

	settings->save();
	emit_signal(SNAME("ai_settings_changed"));
}
