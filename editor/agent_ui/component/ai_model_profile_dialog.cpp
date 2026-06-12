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
#include "scene/gui/scroll_container.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/separator.h"
#include "servers/text/text_server.h"

namespace {

bool _find_provider_preset(const Array &p_presets, const String &p_provider_id, Dictionary &r_preset) {
	for (int i = 0; i < p_presets.size(); i++) {
		if (p_presets[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary preset = p_presets[i];
		if (String(preset.get("id", String())) == p_provider_id) {
			r_preset = preset;
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

VBoxContainer *_make_tab_scroll_content(Control *p_page) {
	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	p_page->add_child(scroll);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 10 * EDSCALE);
	scroll->add_child(content);
	return content;
}

const int DEFAULT_MAX_OUTPUT_TOKENS = 0;
const int DEFAULT_TIMEOUT_MSEC = 120000;

int _profile_int(const Dictionary &p_profile, const String &p_key, int p_default_value) {
	return int(p_profile.get(p_key, p_default_value));
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
	set_title(TTR("Add Model"));
	set_min_size(Size2(680, 560) * EDSCALE);
	set_ok_button_text(TTR("Add Model"));
	set_cancel_button_text(TTR("Cancel"));
	set_hide_on_ok(false);
}

void AIModelProfileDialog::set_model_provider_presets(const Array &p_presets) {
	provider_presets = p_presets.duplicate(true);
	_reload_provider_presets();
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
	provider_page->set_name(TTR("Provider"));
	add_model_tabs->add_child(provider_page);
	_build_provider_add_tab(provider_page);

	MarginContainer *custom_page = memnew(MarginContainer);
	custom_page->set_name(TTR("Custom"));
	add_model_tabs->add_child(custom_page);
	_build_custom_add_tab(custom_page);
}

void AIModelProfileDialog::_build_provider_add_tab(Control *p_page) {
	MarginContainer *margin = Object::cast_to<MarginContainer>(p_page);
	if (margin) {
		margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	}

	VBoxContainer *content = _make_tab_scroll_content(p_page);

	Label *name_label = memnew(Label);
	name_label->set_text(TTR("* Display Name"));
	content->add_child(name_label);

	provider_model_display_name = memnew(LineEdit);
	provider_model_display_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_display_name->set_placeholder(TTR("e.g. OpenAI Primary"));
	content->add_child(provider_model_display_name);

	Label *provider_label = memnew(Label);
	provider_label->set_text(TTR("* Provider"));
	content->add_child(provider_label);

	provider_model_provider = memnew(OptionButton);
	provider_model_provider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_provider->set_fit_to_longest_item(false);
	provider_model_provider->connect(SceneStringName(item_selected), callable_mp(this, &AIModelProfileDialog::_provider_model_provider_selected));
	content->add_child(provider_model_provider);

	Label *model_label = memnew(Label);
	model_label->set_text(TTR("* Model"));
	content->add_child(model_label);

	provider_model_model = memnew(OptionButton);
	provider_model_model->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_model->set_fit_to_longest_item(false);
	content->add_child(provider_model_model);

	Label *key_label = memnew(Label);
	key_label->set_text(TTR("* API Key"));
	content->add_child(key_label);

	provider_model_api_key = memnew(LineEdit);
	provider_model_api_key->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	provider_model_api_key->set_secret(true);
	provider_model_api_key->set_placeholder(TTR("Enter API key"));
	content->add_child(provider_model_api_key);

	_build_advanced_config(content, true);

	HSeparator *separator = memnew(HSeparator);
	content->add_child(separator);

	Button *add_button = memnew(Button);
	add_button->set_text(TTR("Add Model"));
	add_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AIModelProfileDialog::_submit_pressed));
	content->add_child(add_button);
	provider_model_submit_button = add_button;

	_reload_provider_presets();
}

void AIModelProfileDialog::_reload_provider_presets() {
	if (!provider_model_provider) {
		return;
	}

	provider_model_provider->clear();
	if (provider_model_model) {
		provider_model_model->clear();
	}

	for (int i = 0; i < provider_presets.size(); i++) {
		if (provider_presets[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary preset = provider_presets[i];
		const int item_index = provider_model_provider->get_item_count();
		provider_model_provider->add_item(String(preset.get("display_name", preset.get("id", String()))));
		provider_model_provider->set_item_metadata(item_index, String(preset.get("id", String())));
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

	VBoxContainer *content = _make_tab_scroll_content(p_page);

	Label *name_label = memnew(Label);
	name_label->set_text(TTR("* Display Name"));
	content->add_child(name_label);

	custom_display_name = memnew(LineEdit);
	custom_display_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_display_name->set_placeholder(TTR("e.g. Local Ollama"));
	content->add_child(custom_display_name);

	HBoxContainer *url_header = memnew(HBoxContainer);
	url_header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(url_header);

	Label *url_label = memnew(Label);
	url_label->set_text(TTR("* Custom Endpoint"));
	url_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	url_header->add_child(url_label);

	custom_full_url = memnew(CheckButton);
	custom_full_url->set_text(TTR("Full URL"));
	url_header->add_child(custom_full_url);

	custom_base_url = memnew(LineEdit);
	custom_base_url->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_base_url->set_placeholder("e.g. https://api.openai.com/v1");
	content->add_child(custom_base_url);

	PanelContainer *hint_panel = memnew(PanelContainer);
	hint_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(hint_panel);

	Label *hint_label = memnew(Label);
	hint_label->set_text(TTR("Enter an OpenAI-compatible server endpoint without a trailing slash.\n/chat/completions will be appended to the endpoint."));
	hint_panel->add_child(hint_label);

	HBoxContainer *model_header = memnew(HBoxContainer);
	model_header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_child(model_header);

	Label *model_label = memnew(Label);
	model_label->set_text(TTR("* Model ID"));
	model_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	model_header->add_child(model_label);

	custom_multimodal = memnew(CheckButton);
	custom_multimodal->set_text(TTR("Multimodal"));
	custom_multimodal->set_pressed(false);
	model_header->add_child(custom_multimodal);

	custom_model_id = memnew(LineEdit);
	custom_model_id->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_model_id->set_placeholder(TTR("Enter model ID"));
	content->add_child(custom_model_id);

	Label *key_label = memnew(Label);
	key_label->set_text(TTR("* API Key"));
	content->add_child(key_label);

	custom_api_key = memnew(LineEdit);
	custom_api_key->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_api_key->set_secret(true);
	custom_api_key->set_placeholder(TTR("Enter API key"));
	content->add_child(custom_api_key);

	_build_advanced_config(content, false);

	HSeparator *separator = memnew(HSeparator);
	content->add_child(separator);

	Button *add_button = memnew(Button);
	add_button->set_text(TTR("Add Model"));
	add_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &AIModelProfileDialog::_submit_pressed));
	content->add_child(add_button);
	custom_model_submit_button = add_button;
}

SpinBox *AIModelProfileDialog::_add_advanced_spinbox(VBoxContainer *p_content, const String &p_label, int p_default_value, int p_min_value, int p_max_value, const String &p_tooltip) {
	HBoxContainer *row = memnew(HBoxContainer);
	row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	row->add_theme_constant_override("separation", 8 * EDSCALE);
	p_content->add_child(row);

	Label *label = memnew(Label);
	label->set_text(p_label);
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	if (!p_tooltip.is_empty()) {
		label->set_tooltip_text(p_tooltip);
	}
	row->add_child(label);

	SpinBox *spinbox = memnew(SpinBox);
	spinbox->set_h_size_flags(Control::SIZE_SHRINK_END);
	spinbox->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	spinbox->set_min(p_min_value);
	spinbox->set_max(p_max_value);
	spinbox->set_step(1);
	spinbox->set_value(p_default_value);
	spinbox->set_update_on_text_changed(true);
	if (!p_tooltip.is_empty()) {
		spinbox->set_tooltip_text(p_tooltip);
	}
	row->add_child(spinbox);
	return spinbox;
}

void AIModelProfileDialog::_build_advanced_config(VBoxContainer *p_content, bool p_provider_tab) {
	HSeparator *separator = memnew(HSeparator);
	p_content->add_child(separator);

	Label *advanced_label = memnew(Label);
	advanced_label->set_text(TTR("Advanced Configuration"));
	advanced_label->add_theme_font_size_override(SceneStringName(font_size), int(14 * EDSCALE));
	p_content->add_child(advanced_label);

	Label *advanced_description = memnew(Label);
	advanced_description->set_text(TTR("Output tokens set to 0 does not send max_tokens."));
	advanced_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	advanced_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	advanced_description->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("disabled_font_color"), EditorStringName(Editor)));
	p_content->add_child(advanced_description);

	SpinBox *max_output_tokens = _add_advanced_spinbox(p_content, TTR("Output Tokens"), DEFAULT_MAX_OUTPUT_TOKENS, 0, 1000000, TTR("0 means max_tokens is not sent to OpenAI-compatible requests."));
	SpinBox *timeout_msec = _add_advanced_spinbox(p_content, TTR("Request Timeout (ms)"), DEFAULT_TIMEOUT_MSEC, 1, 3600000, TTR("Connection and read timeout in milliseconds."));

	if (p_provider_tab) {
		provider_max_output_tokens = max_output_tokens;
		provider_timeout_msec = timeout_msec;
	} else {
		custom_max_output_tokens = max_output_tokens;
		custom_timeout_msec = timeout_msec;
	}
}

void AIModelProfileDialog::_reset_advanced_config(bool p_provider_tab) {
	Dictionary defaults;
	_apply_advanced_config(defaults, p_provider_tab);
}

void AIModelProfileDialog::_apply_advanced_config(const Dictionary &p_profile, bool p_provider_tab) {
	SpinBox *max_output_tokens = p_provider_tab ? provider_max_output_tokens : custom_max_output_tokens;
	SpinBox *timeout_msec = p_provider_tab ? provider_timeout_msec : custom_timeout_msec;

	if (max_output_tokens) {
		max_output_tokens->set_value(_profile_int(p_profile, "max_output_tokens", DEFAULT_MAX_OUTPUT_TOKENS));
	}
	if (timeout_msec) {
		timeout_msec->set_value(_profile_int(p_profile, "timeout_msec", DEFAULT_TIMEOUT_MSEC));
	}
}

void AIModelProfileDialog::_read_advanced_config(Dictionary &r_profile, bool p_provider_tab) const {
	SpinBox *max_output_tokens = p_provider_tab ? provider_max_output_tokens : custom_max_output_tokens;
	SpinBox *timeout_msec = p_provider_tab ? provider_timeout_msec : custom_timeout_msec;

	r_profile["max_output_tokens"] = max_output_tokens ? (int)max_output_tokens->get_value() : DEFAULT_MAX_OUTPUT_TOKENS;
	r_profile["timeout_msec"] = timeout_msec ? (int)timeout_msec->get_value() : DEFAULT_TIMEOUT_MSEC;
}

void AIModelProfileDialog::_provider_model_provider_selected(int p_index) {
	if (!provider_model_provider || !provider_model_model || p_index < 0) {
		return;
	}

	provider_model_model->clear();
	const String provider_id = String(provider_model_provider->get_item_metadata(p_index));
	Dictionary provider;
	if (!_find_provider_preset(provider_presets, provider_id, provider)) {
		return;
	}

	const Array preset_models = provider.get("preset_models", Array());
	for (int i = 0; i < preset_models.size(); i++) {
		const String model = String(preset_models[i]);
		provider_model_model->add_item(model);
		provider_model_model->set_item_metadata(i, model);
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

	set_title(TTR("Add Model"));
	if (provider_model_submit_button) {
		provider_model_submit_button->set_text(TTR("Add Model"));
	}
	if (custom_model_submit_button) {
		custom_model_submit_button->set_text(TTR("Add Model"));
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
		custom_multimodal->set_pressed(false);
	}
	_reset_advanced_config(true);
	_reset_advanced_config(false);
}

void AIModelProfileDialog::popup_add_model() {
	_build_ui();
	_reset_form();

	popup_centered(Size2(680, 560) * EDSCALE);
}

void AIModelProfileDialog::popup_edit_model(const Dictionary &p_profile) {
	_build_ui();

	const String profile_id = String(p_profile.get("id", String())).strip_edges();
	if (profile_id.is_empty()) {
		return;
	}

	_reset_form();

	editing_model = true;
	editing_profile_id = profile_id;
	editing_custom = bool(p_profile.get("custom", false));
	set_title(TTR("Edit Model"));

	if (editing_custom) {
		if (custom_model_submit_button) {
			custom_model_submit_button->set_text(TTR("Save Model"));
		}
		if (add_model_tabs) {
			add_model_tabs->set_current_tab(1);
		}
		if (custom_display_name) {
			custom_display_name->set_text(String(p_profile.get("display_name", String())));
		}
		if (custom_base_url) {
			custom_base_url->set_text(String(p_profile.get("base_url", String())));
		}
		if (custom_model_id) {
			custom_model_id->set_text(String(p_profile.get("model", String())));
		}
		if (custom_api_key) {
			custom_api_key->set_text(String(p_profile.get("api_key", String())));
		}
		if (custom_full_url) {
			custom_full_url->set_pressed(false);
		}
		if (custom_multimodal) {
			custom_multimodal->set_pressed(bool(p_profile.get("supports_multimodal", false)));
		}
		_apply_advanced_config(p_profile, false);
	} else {
		if (provider_model_submit_button) {
			provider_model_submit_button->set_text(TTR("Save Model"));
		}
		if (add_model_tabs) {
			add_model_tabs->set_current_tab(0);
		}
		if (provider_model_provider) {
			_select_option_metadata(provider_model_provider, String(p_profile.get("provider_id", String())));
			_provider_model_provider_selected(provider_model_provider->get_selected());
		}
		if (provider_model_model) {
			_select_option_metadata(provider_model_model, String(p_profile.get("model", String())));
		}
		if (provider_model_display_name) {
			provider_model_display_name->set_text(String(p_profile.get("display_name", String())));
		}
		if (provider_model_api_key) {
			provider_model_api_key->set_text(String(p_profile.get("api_key", String())));
		}
		_apply_advanced_config(p_profile, true);
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

Dictionary AIModelProfileDialog::get_submitted_profile() const {
	Dictionary profile;

	if (!editing_custom && add_model_tabs && add_model_tabs->get_current_tab() == 0) {
		if (!provider_model_provider || !provider_model_model || provider_model_provider->get_selected() < 0 || provider_model_model->get_selected() < 0) {
			return profile;
		}

		const String provider_id = String(provider_model_provider->get_selected_metadata());
		const String model = String(provider_model_model->get_selected_metadata());
		if (provider_id.is_empty() || model.is_empty()) {
			return Dictionary();
		}

		Dictionary preset;
		_find_provider_preset(provider_presets, provider_id, preset);
		profile["provider_id"] = provider_id;
		profile["model"] = model;
		profile["display_name"] = provider_model_display_name ? provider_model_display_name->get_text().strip_edges() : String();
		profile["api_key"] = provider_model_api_key ? provider_model_api_key->get_text().strip_edges() : String();
		profile["base_url"] = preset.get("default_base_url", String());
		profile["type"] = preset.get("type", "openai-compatible");
		profile["custom"] = false;
		_read_advanced_config(profile, true);
	} else {
		const String model = custom_model_id ? custom_model_id->get_text().strip_edges() : String();
		if (model.is_empty()) {
			return Dictionary();
		}

		profile["provider_id"] = "compatible";
		profile["display_name"] = custom_display_name ? custom_display_name->get_text().strip_edges() : String();
		profile["api_key"] = custom_api_key ? custom_api_key->get_text().strip_edges() : String();
		profile["base_url"] = custom_base_url ? custom_base_url->get_text().strip_edges() : String();
		profile["model"] = model;
		profile["type"] = "openai-compatible";
		profile["supports_multimodal"] = custom_multimodal && custom_multimodal->is_pressed();
		profile["custom"] = true;
		_read_advanced_config(profile, false);
	}

	return profile;
}
