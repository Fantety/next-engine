/**************************************************************************/
/*  ai_model_profile_dialog.cpp                                           */
/**************************************************************************/

#include "ai_model_profile_dialog.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"

namespace {

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

String _ai_ui_text(const char *p_text) {
	return String::utf8(p_text);
}

} // namespace

void AIModelProfileDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("profile_submitted"));
}

void AIModelProfileDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_build_ui();
	}
}

AIModelProfileDialog::AIModelProfileDialog() {
	set_title(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	set_min_size(Size2(680, 560) * EDSCALE);
	set_ok_button_text(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
	set_cancel_button_text(_ai_ui_text(u8"\u53d6\u6d88"));
	set_hide_on_ok(false);
}

void AIModelProfileDialog::_build_ui() {
	if (add_model_tabs) {
		return;
	}

	get_ok_button()->hide();
	get_cancel_button()->hide();

	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 8 * EDSCALE);
	margin->add_theme_constant_override("margin_right", 8 * EDSCALE);
	margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
	margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
	add_child(margin);

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

void AIModelProfileDialog::_build_provider_add_tab(Control *p_page) {
	MarginContainer *margin = Object::cast_to<MarginContainer>(p_page);
	if (margin) {
		margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	}

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 10 * EDSCALE);
	p_page->add_child(content);

	Label *name_label = memnew(Label);
	name_label->set_text(_ai_ui_text(u8"* \u914d\u7f6e\u540d\u79f0"));
	content->add_child(name_label);

	provider_model_display_name = memnew(LineEdit);
	provider_model_display_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_display_name->set_placeholder(_ai_ui_text(u8"\u4f8b\u5982\uff1aOpenAI \u4e3b\u8d26\u53f7"));
	content->add_child(provider_model_display_name);

	Label *provider_label = memnew(Label);
	provider_label->set_text(_ai_ui_text(u8"* \u670d\u52a1\u5546"));
	content->add_child(provider_label);

	provider_model_provider = memnew(OptionButton);
	provider_model_provider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_provider->set_fit_to_longest_item(false);
	provider_model_provider->connect(SceneStringName(item_selected), callable_mp(this, &AIModelProfileDialog::_provider_model_provider_selected));
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
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AIModelProfileDialog::_submit_pressed));
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

void AIModelProfileDialog::_build_custom_add_tab(Control *p_page) {
	MarginContainer *margin = Object::cast_to<MarginContainer>(p_page);
	if (margin) {
		margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	}

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 10 * EDSCALE);
	p_page->add_child(content);

	Label *name_label = memnew(Label);
	name_label->set_text(_ai_ui_text(u8"* \u914d\u7f6e\u540d\u79f0"));
	content->add_child(name_label);

	custom_display_name = memnew(LineEdit);
	custom_display_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_display_name->set_placeholder(_ai_ui_text(u8"\u4f8b\u5982\uff1a\u672c\u5730 Ollama"));
	content->add_child(custom_display_name);

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
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AIModelProfileDialog::_submit_pressed));
	content->add_child(add_button);
	custom_model_submit_button = add_button;
}

void AIModelProfileDialog::_provider_model_provider_selected(int p_index) {
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

void AIModelProfileDialog::_submit_pressed() {
	emit_signal(SNAME("profile_submitted"));
}

void AIModelProfileDialog::_reset_form() {
	editing_model = false;
	editing_profile_id.clear();
	editing_custom = false;

	set_title(_ai_ui_text(u8"\u6dfb\u52a0\u6a21\u578b"));
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
	if (provider_model_display_name) {
		provider_model_display_name->clear();
	}
	if (custom_display_name) {
		custom_display_name->clear();
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

void AIModelProfileDialog::popup_add_model() {
	_build_ui();
	_reset_form();

	popup_centered(Size2(680, 560) * EDSCALE);
}

void AIModelProfileDialog::popup_edit_model(const AIModelProfile &p_profile) {
	_build_ui();

	if (p_profile.id.is_empty()) {
		return;
	}

	_reset_form();

	editing_model = true;
	editing_profile_id = p_profile.id;
	editing_custom = p_profile.custom;
	set_title(_ai_ui_text(u8"\u7f16\u8f91\u6a21\u578b"));

	if (p_profile.custom) {
		if (custom_model_submit_button) {
			custom_model_submit_button->set_text(_ai_ui_text(u8"\u4fdd\u5b58\u6a21\u578b"));
		}
		if (add_model_tabs) {
			add_model_tabs->set_current_tab(1);
		}
		if (custom_api_format && custom_api_format->get_item_count() > 0) {
			custom_api_format->select(0);
		}
		if (custom_display_name) {
			custom_display_name->set_text(p_profile.display_name);
		}
		if (custom_base_url) {
			custom_base_url->set_text(p_profile.base_url);
		}
		if (custom_model_id) {
			custom_model_id->set_text(p_profile.model);
		}
		if (custom_api_key) {
			custom_api_key->set_text(p_profile.api_key);
		}
		if (custom_full_url) {
			custom_full_url->set_pressed(false);
		}
		if (custom_multimodal) {
			custom_multimodal->set_pressed(true);
		}
	} else {
		if (provider_model_submit_button) {
			provider_model_submit_button->set_text(_ai_ui_text(u8"\u4fdd\u5b58\u6a21\u578b"));
		}
		if (add_model_tabs) {
			add_model_tabs->set_current_tab(0);
		}
		if (provider_model_provider) {
			_select_option_metadata(provider_model_provider, p_profile.provider_id);
			_provider_model_provider_selected(provider_model_provider->get_selected());
		}
		if (provider_model_model) {
			_select_option_metadata(provider_model_model, p_profile.model);
		}
		if (provider_model_display_name) {
			provider_model_display_name->set_text(p_profile.display_name);
		}
		if (provider_model_api_key) {
			provider_model_api_key->set_text(p_profile.api_key);
		}
	}

	popup_centered(Size2(680, 560) * EDSCALE);
}

bool AIModelProfileDialog::is_editing_model() const {
	return editing_model;
}

bool AIModelProfileDialog::is_editing_custom_model() const {
	return editing_custom;
}

String AIModelProfileDialog::get_editing_profile_id() const {
	return editing_profile_id;
}

AIModelProfile AIModelProfileDialog::get_submitted_profile() const {
	AIModelProfile profile;

	if (!editing_custom && add_model_tabs && add_model_tabs->get_current_tab() == 0) {
		if (!provider_model_provider || !provider_model_model || provider_model_provider->get_selected() < 0 || provider_model_model->get_selected() < 0) {
			return profile;
		}

		profile.provider_id = String(provider_model_provider->get_selected_metadata());
		profile.model = String(provider_model_model->get_selected_metadata());
		if (profile.provider_id.is_empty() || profile.model.is_empty()) {
			return AIModelProfile();
		}

		profile.display_name = provider_model_display_name ? provider_model_display_name->get_text().strip_edges() : String();
		profile.api_key = provider_model_api_key ? provider_model_api_key->get_text().strip_edges() : String();
		profile.base_url = AIModelSettings::get_provider_base_url(profile.provider_id);
		profile.custom = false;
	} else {
		profile.model = custom_model_id ? custom_model_id->get_text().strip_edges() : String();
		if (profile.model.is_empty()) {
			return AIModelProfile();
		}

		profile.provider_id = "compatible";
		profile.display_name = custom_display_name ? custom_display_name->get_text().strip_edges() : String();
		profile.api_key = custom_api_key ? custom_api_key->get_text().strip_edges() : String();
		profile.base_url = custom_base_url ? custom_base_url->get_text().strip_edges() : String();
		profile.custom = true;
	}

	return profile;
}
