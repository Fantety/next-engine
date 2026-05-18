/**************************************************************************/
/*  ai_agent_settings_dialog.cpp                                           */
/**************************************************************************/

#include "ai_agent_settings_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/editor_string_names.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/texture_rect.h"

namespace {

Vector<String> _split_model_lines(const String &p_models) {
	Vector<String> models;
	Vector<String> lines = p_models.replace(",", "\n").split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		String model = lines[i].strip_edges();
		if (!model.is_empty() && models.find(model) == -1) {
			models.push_back(model);
		}
	}
	return models;
}

bool _find_provider_preset(const String &p_provider_id, AIModelProviderPreset &r_preset) {
	Vector<AIModelProviderPreset> providers = AIModelSettings::get_provider_presets();
	for (int i = 0; i < providers.size(); i++) {
		if (providers[i].id == p_provider_id) {
			r_preset = providers[i];
			return true;
		}
	}
	return false;
}

void _select_option_metadata(OptionButton *p_option, const String &p_metadata) {
	ERR_FAIL_NULL(p_option);
	for (int i = 0; i < p_option->get_item_count(); i++) {
		if (String(p_option->get_item_metadata(i)) == p_metadata) {
			p_option->select(i);
			return;
		}
	}
}

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
	if (p_width > 0) {
		label->set_custom_minimum_size(Size2(p_width, 0) * EDSCALE);
	} else {
		label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	}
	return label;
}

String _ai_ui_text(const char *p_text) {
	return String::utf8(p_text);
}

} // namespace

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

	set_title(_ai_ui_text(u8"AI Agent \u8bbe\u7f6e"));
	set_min_size(Size2(900, 600) * EDSCALE);

	HBoxContainer *root = memnew(HBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 12 * EDSCALE);
	add_child(root);

	_build_navigation(root);

	VSeparator *main_separator = memnew(VSeparator);
	root->add_child(main_separator);

	_build_pages(root);
	_build_add_model_dialog();

	get_ok_button()->set_text(_ai_ui_text(u8"\u4fdd\u5b58"));
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
	navigation->add_item(_ai_ui_text(u8"\u6a21\u578b"), get_editor_theme_icon(SNAME("AIModel")));
	navigation->set_item_metadata(PAGE_MODELS, PAGE_MODELS);
	navigation->add_item(TTR("MCP"), get_editor_theme_icon(SNAME("AIMCP")));
	navigation->set_item_metadata(PAGE_MCP, PAGE_MCP);
	navigation->add_item(_ai_ui_text(u8"\u6280\u80fd"), get_editor_theme_icon(SNAME("AISkill")));
	navigation->set_item_metadata(PAGE_SKILLS, PAGE_SKILLS);
	navigation->add_item(_ai_ui_text(u8"\u89c4\u5219"), get_editor_theme_icon(SNAME("AIRules")));
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
	models_page->set_name(_ai_ui_text(u8"\u6a21\u578b"));
	pages->add_child(models_page);
	_build_models_page(models_page);

	MarginContainer *mcp_page = memnew(MarginContainer);
	mcp_page->set_name(TTR("MCP"));
	pages->add_child(mcp_page);
	_add_placeholder_page(mcp_page, _ai_ui_text(u8"MCP \u914d\u7f6e\u6682\u672a\u5b9e\u73b0\u3002"));

	MarginContainer *skills_page = memnew(MarginContainer);
	skills_page->set_name(_ai_ui_text(u8"\u6280\u80fd"));
	pages->add_child(skills_page);
	_add_placeholder_page(skills_page, _ai_ui_text(u8"\u6280\u80fd\u914d\u7f6e\u6682\u672a\u5b9e\u73b0\u3002"));

	MarginContainer *rules_page = memnew(MarginContainer);
	rules_page->set_name(_ai_ui_text(u8"\u89c4\u5219"));
	pages->add_child(rules_page);
	_add_placeholder_page(rules_page, _ai_ui_text(u8"\u89c4\u5219\u914d\u7f6e\u6682\u672a\u5b9e\u73b0\u3002"));
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

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 12 * EDSCALE);
	scroll->add_child(content);

	Label *title = memnew(Label);
	title->set_text(_ai_ui_text(u8"\u6a21\u578b"));
	title->add_theme_font_size_override(SceneStringName(font_size), int(22 * EDSCALE));
	content->add_child(title);

	Label *section_title = memnew(Label);
	section_title->set_text(_ai_ui_text(u8"\u6a21\u578b\u7ba1\u7406"));
	section_title->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	content->add_child(section_title);

	Label *description = memnew(Label);
	description->set_text(_ai_ui_text(u8"\u914d\u7f6e API key \u6dfb\u52a0\u66f4\u591a\u53ef\u7528\u6a21\u578b\uff0c\u9884\u7f6e\u6a21\u578b\u9ed8\u8ba4\u4f7f\u7528\u7a33\u5b9a\u7248\u672c\u3002"));
	description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	content->add_child(description);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(toolbar);

	add_model_button = memnew(Button);
	add_model_button->set_text(_ai_ui_text(u8"+ \u6dfb\u52a0\u6a21\u578b"));
	add_model_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentSettingsDialog::_popup_add_model_dialog));
	toolbar->add_child(add_model_button);

	PanelContainer *table_panel = memnew(PanelContainer);
	table_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	table_panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(table_panel);

	model_table = memnew(VBoxContainer);
	model_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_table->add_theme_constant_override("separation", 0);
	table_panel->add_child(model_table);

	_refresh_model_table();
}

void AIAgentSettingsDialog::_build_add_model_dialog() {
	add_model_dialog = memnew(ConfirmationDialog);
	add_model_dialog->set_title(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	add_model_dialog->set_min_size(Size2(560, 560) * EDSCALE);
	add_model_dialog->set_ok_button_text(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	add_model_dialog->set_cancel_button_text(_ai_ui_text(u8"\u53d6\u6d88"));
	add_model_dialog->set_hide_on_ok(false);
	add_model_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AIAgentSettingsDialog::_add_model_confirmed));
	add_model_dialog->get_ok_button()->hide();
	add_model_dialog->get_cancel_button()->hide();
	add_child(add_model_dialog);

	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 8 * EDSCALE);
	margin->add_theme_constant_override("margin_right", 8 * EDSCALE);
	margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
	margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
	add_model_dialog->add_child(margin);

	add_model_tabs = memnew(TabContainer);
	add_model_tabs->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_model_tabs->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	margin->add_child(add_model_tabs);

	MarginContainer *provider_page = memnew(MarginContainer);
	provider_page->set_name(_ai_ui_text(u8"\u6a21\u578b\u670d\u52a1\u5546"));
	add_model_tabs->add_child(provider_page);
	_build_provider_add_tab(provider_page);

	MarginContainer *custom_page = memnew(MarginContainer);
	custom_page->set_name(_ai_ui_text(u8"\u81ea\u5b9a\u4e49\u914d\u7f6e"));
	add_model_tabs->add_child(custom_page);
	_build_custom_add_tab(custom_page);
}

void AIAgentSettingsDialog::_build_provider_add_tab(Control *p_page) {
	MarginContainer *margin = Object::cast_to<MarginContainer>(p_page);
	if (margin) {
		margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	}

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 10 * EDSCALE);
	p_page->add_child(content);

	Label *provider_label = memnew(Label);
	provider_label->set_text(_ai_ui_text(u8"* \u670d\u52a1\u5546"));
	content->add_child(provider_label);

	provider_model_provider = memnew(OptionButton);
	provider_model_provider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_provider->set_fit_to_longest_item(false);
	provider_model_provider->connect(SceneStringName(item_selected), callable_mp(this, &AIAgentSettingsDialog::_provider_model_provider_selected));
	content->add_child(provider_model_provider);

	Label *model_label = memnew(Label);
	model_label->set_text(_ai_ui_text(u8"* \u6a21\u578b"));
	content->add_child(model_label);

	provider_model_model = memnew(OptionButton);
	provider_model_model->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_model->set_fit_to_longest_item(false);
	content->add_child(provider_model_model);

	Label *key_label = memnew(Label);
	key_label->set_text(_ai_ui_text(u8"* API \u5bc6\u94a5"));
	content->add_child(key_label);

	provider_model_api_key = memnew(LineEdit);
	provider_model_api_key->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_api_key->set_secret(true);
	provider_model_api_key->set_placeholder(_ai_ui_text(u8"\u8f93\u5165 API \u5bc6\u94a5"));
	content->add_child(provider_model_api_key);

	Label *advanced_label = memnew(Label);
	advanced_label->set_text(_ai_ui_text(u8"> \u9ad8\u7ea7\u914d\u7f6e"));
	content->add_child(advanced_label);

	Label *advanced_description = memnew(Label);
	advanced_description->set_text(_ai_ui_text(u8"\u5305\u542b\u6a21\u578b\u7cfb\u5217\uff08\u4f18\u5316\u7684 Prompt \u548c\u8d85\u53c2\uff09\u3001\u5c55\u793a\u540d\u79f0\u3001\u4e0a\u4e0b\u6587\u7a97\u53e3\u7b49\u914d\u7f6e\u3002"));
	advanced_description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	content->add_child(advanced_description);

	HSeparator *separator = memnew(HSeparator);
	content->add_child(separator);

	Button *add_button = memnew(Button);
	add_button->set_text(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	add_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentSettingsDialog::_add_model_confirmed));
	content->add_child(add_button);
	provider_model_submit_button = add_button;

	Vector<AIModelProviderPreset> providers = AIModelSettings::get_provider_presets();
	for (int i = 0; i < providers.size(); i++) {
		provider_model_provider->add_item(providers[i].display_name);
		provider_model_provider->set_item_metadata(i, providers[i].id);
	}
	if (provider_model_provider->get_item_count() > 0) {
		provider_model_provider->select(0);
		_provider_model_provider_selected(0);
	}
}

void AIAgentSettingsDialog::_build_custom_add_tab(Control *p_page) {
	MarginContainer *margin = Object::cast_to<MarginContainer>(p_page);
	if (margin) {
		margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	}

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 10 * EDSCALE);
	p_page->add_child(content);

	Label *format_label = memnew(Label);
	format_label->set_text(_ai_ui_text(u8"* API \u683c\u5f0f"));
	content->add_child(format_label);

	custom_api_format = memnew(OptionButton);
	custom_api_format->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_api_format->add_item(_ai_ui_text(u8"OpenAI Chat Completions \u683c\u5f0f"));
	custom_api_format->set_item_metadata(0, "openai_chat_completions");
	custom_api_format->select(0);
	content->add_child(custom_api_format);

	HBoxContainer *url_header = memnew(HBoxContainer);
	url_header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(url_header);

	Label *url_label = memnew(Label);
	url_label->set_text(_ai_ui_text(u8"* \u81ea\u5b9a\u4e49\u8bf7\u6c42\u5730\u5740"));
	url_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	url_header->add_child(url_label);

	custom_full_url = memnew(CheckButton);
	custom_full_url->set_text(_ai_ui_text(u8"\u5b8c\u6574 URL"));
	url_header->add_child(custom_full_url);

	custom_base_url = memnew(LineEdit);
	custom_base_url->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_base_url->set_placeholder("e.g. https://api.openai.com/v1");
	content->add_child(custom_base_url);

	PanelContainer *hint_panel = memnew(PanelContainer);
	hint_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(hint_panel);

	Label *hint_label = memnew(Label);
	hint_label->set_text(_ai_ui_text(u8"\u8bf7\u586b\u5199\u517c\u5bb9 OpenAI API \u7684\u670d\u52a1\u7aef\u70b9\u5730\u5740\uff0c\u4e0d\u8981\u4ee5\u659c\u6760\u7ed3\u5c3e\u3002\n/chat/completions \u5c06\u4f1a\u88ab\u8865\u5145\u5230\u4f60\u586b\u5199\u7684\u5730\u5740\u672b\u5c3e\u3002"));
	hint_panel->add_child(hint_label);

	HBoxContainer *model_header = memnew(HBoxContainer);
	model_header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(model_header);

	Label *model_label = memnew(Label);
	model_label->set_text(_ai_ui_text(u8"* \u6a21\u578b ID"));
	model_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_header->add_child(model_label);

	custom_multimodal = memnew(CheckButton);
	custom_multimodal->set_text(_ai_ui_text(u8"\u591a\u6a21\u6001"));
	custom_multimodal->set_pressed(true);
	model_header->add_child(custom_multimodal);

	custom_model_id = memnew(LineEdit);
	custom_model_id->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_model_id->set_placeholder(_ai_ui_text(u8"\u8f93\u5165\u6a21\u578b ID"));
	content->add_child(custom_model_id);

	Label *key_label = memnew(Label);
	key_label->set_text(_ai_ui_text(u8"* API \u5bc6\u94a5"));
	content->add_child(key_label);

	custom_api_key = memnew(LineEdit);
	custom_api_key->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_api_key->set_secret(true);
	custom_api_key->set_placeholder(_ai_ui_text(u8"\u8f93\u5165 API \u5bc6\u94a5"));
	content->add_child(custom_api_key);

	Label *advanced_label = memnew(Label);
	advanced_label->set_text(_ai_ui_text(u8"> \u9ad8\u7ea7\u914d\u7f6e"));
	content->add_child(advanced_label);

	Label *advanced_description = memnew(Label);
	advanced_description->set_text(_ai_ui_text(u8"\u5305\u542b\u6a21\u578b\u7cfb\u5217\uff08\u4f18\u5316\u7684 Prompt \u548c\u8d85\u53c2\uff09\u3001\u5c55\u793a\u540d\u79f0\u3001\u4e0a\u4e0b\u6587\u7a97\u53e3\u7b49\u914d\u7f6e\u3002"));
	advanced_description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	content->add_child(advanced_description);

	HSeparator *separator = memnew(HSeparator);
	content->add_child(separator);

	Button *add_button = memnew(Button);
	add_button->set_text(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	add_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentSettingsDialog::_add_model_confirmed));
	content->add_child(add_button);
	custom_model_submit_button = add_button;
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

void AIAgentSettingsDialog::_refresh_model_table() {
	ERR_FAIL_NULL(model_table);

	_clear_children(model_table);
	model_table_rows.clear();

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_custom_minimum_size(Size2(0, 32) * EDSCALE);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_theme_constant_override("separation", 8 * EDSCALE);
	model_table->add_child(header);

	Label *model_header = _make_table_label(_ai_ui_text(u8"\u6a21\u578b"));
	model_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(model_header);

	Label *provider_header = _make_table_label(_ai_ui_text(u8"\u670d\u52a1\u5546"), 170);
	provider_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(provider_header);

	Label *action_header = _make_table_label(_ai_ui_text(u8"\u64cd\u4f5c"), 150);
	action_header->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	header->add_child(action_header);

	HSeparator *header_separator = memnew(HSeparator);
	model_table->add_child(header_separator);

	Vector<AIModelProviderPreset> providers = AIModelSettings::get_provider_presets();
	int model_count = 0;
	for (int i = 0; i < providers.size(); i++) {
		const AIModelProviderPreset &provider = providers[i];
		for (int j = 0; j < provider.preset_models.size(); j++) {
			if (AIModelSettings::is_model_enabled(provider.id, provider.preset_models[j])) {
				_add_model_table_row(provider.id, provider.display_name, provider.preset_models[j], false);
				model_count++;
			}
		}

		Vector<String> custom_models = _split_model_lines(AIModelSettings::get_custom_models(provider.id));
		for (int j = 0; j < custom_models.size(); j++) {
			if (AIModelSettings::is_model_enabled(provider.id, custom_models[j])) {
				_add_model_table_row(provider.id, provider.display_name, custom_models[j], true);
				model_count++;
			}
		}
	}

	if (model_count == 0) {
		MarginContainer *empty_margin = memnew(MarginContainer);
		empty_margin->add_theme_constant_override("margin_left", 28 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		empty_margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
		model_table->add_child(empty_margin);

		Label *empty_label = memnew(Label);
		empty_label->set_text(_ai_ui_text(u8"\u6682\u65e0\u6a21\u578b\uff0c\u4f60\u53ef\u4ee5\u70b9\u51fb\u6dfb\u52a0\u6a21\u578b"));
		empty_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
		empty_margin->add_child(empty_label);
	}
}

void AIAgentSettingsDialog::_add_model_table_row(const String &p_provider_id, const String &p_provider_name, const String &p_model, bool p_custom) {
	ERR_FAIL_NULL(model_table);

	ModelTableRow row_data;
	row_data.provider_id = p_provider_id;
	row_data.model = p_model;
	row_data.custom = p_custom;
	model_table_rows.push_back(row_data);

	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->set_custom_minimum_size(Size2(0, 36) * EDSCALE);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	model_table->add_child(row);

	HBoxContainer *model_cell = memnew(HBoxContainer);
	model_cell->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_cell->add_theme_constant_override("separation", 8 * EDSCALE);
	row->add_child(model_cell);

	TextureRect *icon = memnew(TextureRect);
	icon->set_texture(get_editor_theme_icon(SNAME("AIModel")));
	icon->set_custom_minimum_size(Size2(16, 16) * EDSCALE);
	icon->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
	model_cell->add_child(icon);

	Label *model_label = memnew(Label);
	model_label->set_text(p_model);
	model_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	model_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_cell->add_child(model_label);

	Label *provider_label = _make_table_label(p_provider_name, 170);
	row->add_child(provider_label);

	HBoxContainer *action_cell = memnew(HBoxContainer);
	action_cell->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	action_cell->add_theme_constant_override("separation", 6 * EDSCALE);
	row->add_child(action_cell);

	Button *edit_button = memnew(Button);
	edit_button->set_text(_ai_ui_text(u8"\u7f16\u8f91"));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentSettingsDialog::_edit_model_pressed).bind(p_provider_id, p_model, p_custom), CONNECT_DEFERRED);
	action_cell->add_child(edit_button);

	Button *remove_button = memnew(Button);
	remove_button->set_text(_ai_ui_text(u8"\u79fb\u9664"));
	remove_button->connect(SceneStringName(pressed), callable_mp(this, &AIAgentSettingsDialog::_remove_model_pressed).bind(p_provider_id, p_model, p_custom), CONNECT_DEFERRED);
	action_cell->add_child(remove_button);

	HSeparator *row_separator = memnew(HSeparator);
	model_table->add_child(row_separator);
}

void AIAgentSettingsDialog::_navigation_selected(int p_index) {
	if (!pages || !navigation || p_index < 0) {
		return;
	}
	pages->set_current_tab((int)navigation->get_item_metadata(p_index));
}

void AIAgentSettingsDialog::_popup_add_model_dialog() {
	ERR_FAIL_NULL(add_model_dialog);
	_reset_add_model_dialog();
	add_model_dialog->popup_centered(Size2(560, 560) * EDSCALE);
}

void AIAgentSettingsDialog::_edit_model_pressed(const String &p_provider_id, const String &p_model, bool p_custom) {
	ERR_FAIL_NULL(add_model_dialog);
	ERR_FAIL_NULL(add_model_tabs);

	_reset_add_model_dialog();

	editing_model = true;
	editing_provider_id = p_provider_id;
	editing_model_id = p_model;
	editing_custom = p_custom;

	if (p_custom) {
		add_model_dialog->set_title(_ai_ui_text(u8"\u7f16\u8f91\u6a21\u578b"));
		if (custom_model_submit_button) {
			custom_model_submit_button->set_text(_ai_ui_text(u8"\u4fdd\u5b58\u6a21\u578b"));
		}
		if (add_model_tabs) {
			add_model_tabs->set_current_tab(1);
		}
		if (custom_api_format && custom_api_format->get_item_count() > 0) {
			custom_api_format->select(0);
		}
		if (custom_base_url) {
			custom_base_url->set_text(AIModelSettings::get_provider_base_url(p_provider_id));
		}
		if (custom_model_id) {
			custom_model_id->set_text(p_model);
		}
		if (custom_api_key) {
			custom_api_key->set_text(AIModelSettings::get_provider_api_key(p_provider_id));
		}
		if (custom_full_url) {
			custom_full_url->set_pressed(false);
		}
		if (custom_multimodal) {
			custom_multimodal->set_pressed(true);
		}
	} else {
		add_model_dialog->set_title(_ai_ui_text(u8"\u7f16\u8f91\u6a21\u578b"));
		if (provider_model_submit_button) {
			provider_model_submit_button->set_text(_ai_ui_text(u8"\u4fdd\u5b58\u6a21\u578b"));
		}
		if (add_model_tabs) {
			add_model_tabs->set_current_tab(0);
		}
		if (provider_model_provider) {
			_select_option_metadata(provider_model_provider, p_provider_id);
			_provider_model_provider_selected(provider_model_provider->get_selected());
		}
		if (provider_model_model) {
			_select_option_metadata(provider_model_model, p_model);
		}
		if (provider_model_api_key) {
			provider_model_api_key->set_text(AIModelSettings::get_provider_api_key(p_provider_id));
		}
	}

	add_model_dialog->popup_centered(Size2(560, 560) * EDSCALE);
}

void AIAgentSettingsDialog::_provider_model_provider_selected(int p_index) {
	if (!provider_model_provider || !provider_model_model || p_index < 0) {
		return;
	}

	provider_model_model->clear();
	const String provider_id = String(provider_model_provider->get_item_metadata(p_index));
	AIModelProviderPreset provider;
	if (!_find_provider_preset(provider_id, provider)) {
		return;
	}

	for (int i = 0; i < provider.preset_models.size(); i++) {
		provider_model_model->add_item(provider.preset_models[i]);
		provider_model_model->set_item_metadata(i, provider.preset_models[i]);
	}
	if (provider_model_model->get_item_count() > 0) {
		provider_model_model->select(0);
	}
}

void AIAgentSettingsDialog::_add_model_confirmed() {
	ERR_FAIL_NULL(add_model_tabs);

	if (!editing_custom && add_model_tabs->get_current_tab() == 0) {
		if (!provider_model_provider || !provider_model_model || provider_model_provider->get_selected() < 0 || provider_model_model->get_selected() < 0) {
			return;
		}

		const String provider_id = String(provider_model_provider->get_selected_metadata());
		const String model = String(provider_model_model->get_selected_metadata());
		if (provider_id.is_empty() || model.is_empty()) {
			return;
		}

		const String api_key = provider_model_api_key ? provider_model_api_key->get_text().strip_edges() : String();
		if (!api_key.is_empty()) {
			AIModelSettings::set_provider_auth(provider_id, api_key, AIModelSettings::get_provider_base_url(provider_id));
		}
		if (editing_model && !editing_custom && (editing_provider_id != provider_id || editing_model_id != model)) {
			AIModelSettings::set_model_enabled(editing_provider_id, editing_model_id, false);
		}
		AIModelSettings::set_model_enabled(provider_id, model, true);
	} else {
		const String model = custom_model_id ? custom_model_id->get_text().strip_edges() : String();
		if (model.is_empty()) {
			return;
		}

		const String base_url = custom_base_url ? custom_base_url->get_text().strip_edges() : String();
		const String api_key = custom_api_key ? custom_api_key->get_text().strip_edges() : String();
		const String provider_id = editing_model && editing_custom ? editing_provider_id : String("compatible");
		if (editing_model && editing_custom && editing_model_id != model) {
			Vector<String> custom_models = _split_model_lines(AIModelSettings::get_custom_models(provider_id));
			for (int i = custom_models.size() - 1; i >= 0; i--) {
				if (custom_models[i] == editing_model_id) {
					custom_models.remove_at(i);
				}
			}
			AIModelSettings::set_custom_models(provider_id, String("\n").join(custom_models));
			AIModelSettings::set_model_enabled(provider_id, editing_model_id, false);
		}
		AIModelSettings::set_provider_auth(provider_id, api_key, base_url);
		_append_custom_model(provider_id, model);
		AIModelSettings::set_model_enabled(provider_id, model, true);
	}

	_refresh_model_table();
	_save_settings();
	editing_model = false;
	editing_provider_id.clear();
	editing_model_id.clear();
	editing_custom = false;
	if (add_model_dialog) {
		add_model_dialog->hide();
	}
}

void AIAgentSettingsDialog::_remove_model_pressed(const String &p_provider_id, const String &p_model, bool p_custom) {
	if (p_custom) {
		Vector<String> custom_models = _split_model_lines(AIModelSettings::get_custom_models(p_provider_id));
		for (int i = custom_models.size() - 1; i >= 0; i--) {
			if (custom_models[i] == p_model) {
				custom_models.remove_at(i);
			}
		}
		AIModelSettings::set_custom_models(p_provider_id, String("\n").join(custom_models));
	}

	AIModelSettings::set_model_enabled(p_provider_id, p_model, false);
	_refresh_model_table();
	_save_settings();
}

void AIAgentSettingsDialog::_append_custom_model(const String &p_provider_id, const String &p_model) {
	Vector<String> custom_models = _split_model_lines(AIModelSettings::get_custom_models(p_provider_id));
	if (custom_models.find(p_model) == -1) {
		custom_models.push_back(p_model);
	}
	AIModelSettings::set_custom_models(p_provider_id, String("\n").join(custom_models));
}

void AIAgentSettingsDialog::_reset_add_model_dialog() {
	editing_model = false;
	editing_provider_id.clear();
	editing_model_id.clear();
	editing_custom = false;
	if (add_model_dialog) {
		add_model_dialog->set_title(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	}
	if (provider_model_submit_button) {
		provider_model_submit_button->set_text(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	}
	if (custom_model_submit_button) {
		custom_model_submit_button->set_text(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	}
	if (add_model_tabs) {
		add_model_tabs->set_current_tab(0);
	}
	if (provider_model_provider && provider_model_provider->get_item_count() > 0) {
		provider_model_provider->select(0);
		_provider_model_provider_selected(0);
	}
	if (provider_model_api_key) {
		provider_model_api_key->clear();
	}
	if (custom_base_url) {
		custom_base_url->clear();
	}
	if (custom_model_id) {
		custom_model_id->clear();
	}
	if (custom_api_key) {
		custom_api_key->clear();
	}
	if (custom_full_url) {
		custom_full_url->set_pressed(false);
	}
	if (custom_multimodal) {
		custom_multimodal->set_pressed(true);
	}
}

void AIAgentSettingsDialog::_save_settings() {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	settings->save();
	emit_signal(SNAME("ai_settings_changed"));
}

void AIAgentSettingsDialog::build_for_test() {
	_build_ui();
}

int AIAgentSettingsDialog::get_model_table_row_count_for_test() const {
	return model_table_rows.size();
}

int AIAgentSettingsDialog::get_custom_model_table_row_count_for_test() const {
	int count = 0;
	for (int i = 0; i < model_table_rows.size(); i++) {
		if (model_table_rows[i].custom) {
			count++;
		}
	}
	return count;
}

void AIAgentSettingsDialog::add_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key) {
	if (!p_api_key.is_empty()) {
		AIModelSettings::set_provider_auth(p_provider_id, p_api_key, AIModelSettings::get_provider_base_url(p_provider_id));
	}
	AIModelSettings::set_model_enabled(p_provider_id, p_model, true);
	_refresh_model_table();
}

void AIAgentSettingsDialog::add_custom_model_for_test(const String &p_model, const String &p_base_url, const String &p_api_key) {
	AIModelSettings::set_provider_auth("compatible", p_api_key, p_base_url);
	_append_custom_model("compatible", p_model);
	AIModelSettings::set_model_enabled("compatible", p_model, true);
	_refresh_model_table();
}

void AIAgentSettingsDialog::edit_provider_model_for_test(const String &p_provider_id, const String &p_model, const String &p_api_key) {
	AIModelSettings::set_provider_auth(p_provider_id, p_api_key, AIModelSettings::get_provider_base_url(p_provider_id));
	AIModelSettings::set_model_enabled(p_provider_id, p_model, true);
	_refresh_model_table();
}

void AIAgentSettingsDialog::edit_custom_model_for_test(const String &p_current_model, const String &p_new_model, const String &p_base_url, const String &p_api_key) {
	Vector<String> custom_models = _split_model_lines(AIModelSettings::get_custom_models("compatible"));
	for (int i = custom_models.size() - 1; i >= 0; i--) {
		if (custom_models[i] == p_current_model) {
			custom_models.remove_at(i);
		}
	}
	if (custom_models.find(p_new_model) == -1) {
		custom_models.push_back(p_new_model);
	}
	AIModelSettings::set_custom_models("compatible", String("\n").join(custom_models));
	AIModelSettings::set_provider_auth("compatible", p_api_key, p_base_url);
	AIModelSettings::set_model_enabled("compatible", p_current_model, false);
	AIModelSettings::set_model_enabled("compatible", p_new_model, true);
	_refresh_model_table();
}

void AIAgentSettingsDialog::remove_custom_model_for_test(const String &p_provider_id, const String &p_model) {
	_remove_model_pressed(p_provider_id, p_model, true);
}

void AIAgentSettingsDialog::save_settings_for_test() {
	_save_settings();
}
