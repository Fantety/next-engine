/**************************************************************************/
/*  ai_model_settings.cpp                                                 */
/**************************************************************************/

#include "ai_model_settings.h"

#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "editor/settings/editor_settings.h"

String AIModelSettings::_get_provider_path(const String &p_provider_id, const String &p_property) {
	return "ai_agent/providers/" + p_provider_id + "/" + p_property;
}

String AIModelSettings::_get_model_path(const String &p_provider_id, const String &p_model) {
	return "ai_agent/models/" + get_model_id(p_provider_id, p_model) + "/enabled";
}

String AIModelSettings::_get_profiles_path() {
	return "ai_agent/model_profiles";
}

//为了兼容旧版本的deepseek模型配置
String AIModelSettings::_get_legacy_deepseek_value(const String &p_property, const String &p_default_value) {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings || !settings->has_setting("deepseek/" + p_property)) {
		return p_default_value;
	}
	return String(settings->get("deepseek/" + p_property));
}

bool AIModelSettings::_is_default_model_enabled(const String &p_provider_id, const String &p_model) {
	if (p_provider_id == "deepseek" && (p_model == "deepseek-chat" || p_model == "deepseek-reasoner")) {
		EditorSettings *settings = EditorSettings::get_singleton();
		String legacy_path = "deepseek/models/" + p_model;
		if (settings && settings->has_setting(legacy_path)) {
			return bool(settings->get(legacy_path));
		}
	}
	return false;
}

PackedStringArray AIModelSettings::_split_custom_models(const String &p_models) {
	PackedStringArray models;
	Vector<String> lines = p_models.replace(",", "\n").split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		String model = lines[i].strip_edges();
		if (!model.is_empty() && !models.has(model)) {
			models.push_back(model);
		}
	}
	return models;
}

bool AIModelSettings::_has_profile_storage() {
	EditorSettings *settings = EditorSettings::get_singleton();
	return settings && settings->has_setting(_get_profiles_path());
}

Array AIModelSettings::_get_profile_storage() {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return Array();
	}

	const String path = _get_profiles_path();
	if (!settings->has_setting(path)) {
		Array legacy_profiles = _build_legacy_profile_storage();
		if (!legacy_profiles.is_empty()) {
			_set_profile_storage(legacy_profiles);
		}
		return legacy_profiles;
	}

	Variant value = settings->get(path);
	if (value.get_type() != Variant::ARRAY) {
		return Array();
	}
	return value;
}

void AIModelSettings::_set_profile_storage(const Array &p_profiles) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	settings->set(_get_profiles_path(), p_profiles);
}

Array AIModelSettings::_build_legacy_profile_storage() {
	Array profiles;
	Vector<AIModelProviderPreset> providers = get_provider_presets();
	for (int i = 0; i < providers.size(); i++) {
		const AIModelProviderPreset &provider = providers[i];
		for (int j = 0; j < provider.preset_models.size(); j++) {
			const String model = provider.preset_models[j];
			if (!is_model_enabled(provider.id, model)) {
				continue;
			}

			AIModelProfile profile;
			profile.id = _make_profile_id(provider.id, model);
			profile.display_name = _make_profile_display_name(provider.display_name, model);
			profile.provider_id = provider.id;
			profile.provider_name = provider.display_name;
			profile.model = model;
			profile.base_url = get_provider_base_url(provider.id);
			profile.api_key = get_provider_api_key(provider.id);
			profile.enabled = true;
			profile.custom = false;
			profiles.push_back(_profile_to_dictionary(profile));
		}

		PackedStringArray custom_models = _split_custom_models(get_custom_models(provider.id));
		for (int j = 0; j < custom_models.size(); j++) {
			const String model = custom_models[j];
			if (!is_model_enabled(provider.id, model)) {
				continue;
			}

			AIModelProfile profile;
			profile.id = _make_profile_id(provider.id, model);
			profile.display_name = _make_profile_display_name(provider.display_name, model);
			profile.provider_id = provider.id;
			profile.provider_name = provider.display_name;
			profile.model = model;
			profile.base_url = get_provider_base_url(provider.id);
			profile.api_key = get_provider_api_key(provider.id);
			profile.enabled = true;
			profile.custom = true;
			profiles.push_back(_profile_to_dictionary(profile));
		}
	}
	return profiles;
}

AIModelProviderPreset AIModelSettings::_get_provider_preset(const String &p_provider_id) {
	Vector<AIModelProviderPreset> providers = get_provider_presets();
	for (int i = 0; i < providers.size(); i++) {
		if (providers[i].id == p_provider_id) {
			return providers[i];
		}
	}
	return AIModelProviderPreset();
}

AIModelProfile AIModelSettings::_profile_from_dictionary(const Dictionary &p_profile) {
	AIModelProfile profile;
	profile.id = String(p_profile.get("id", String()));
	profile.display_name = String(p_profile.get("display_name", String()));
	profile.provider_id = String(p_profile.get("provider_id", String()));
	profile.model = String(p_profile.get("model", String()));
	profile.base_url = String(p_profile.get("base_url", String()));
	profile.api_key = String(p_profile.get("api_key", String()));
	profile.enabled = bool(p_profile.get("enabled", true));
	profile.custom = bool(p_profile.get("custom", false));
	profile.max_input_chars = MAX(256, (int)p_profile.get("max_input_chars", 96000));
	profile.max_context_chars = MAX(128, (int)p_profile.get("max_context_chars", 24000));
	profile.max_history_chars = MAX(128, (int)p_profile.get("max_history_chars", 64000));
	profile.max_tool_result_chars = MAX(64, (int)p_profile.get("max_tool_result_chars", 16000));
	profile.min_recent_messages = MAX(1, (int)p_profile.get("min_recent_messages", 4));
	profile.max_provider_turns = MAX(1, (int)p_profile.get("max_provider_turns", 255));
	profile.max_tool_calls = MAX(1, (int)p_profile.get("max_tool_calls", 60));
	profile.max_output_tokens = MAX(0, (int)p_profile.get("max_output_tokens", 0));
	profile.timeout_seconds = MAX(1, (int)p_profile.get("timeout_seconds", 180));

	AIModelProviderPreset provider = _get_provider_preset(profile.provider_id);
	profile.provider_name = provider.display_name.is_empty() ? profile.provider_id : provider.display_name;
	if (profile.base_url.is_empty()) {
		profile.base_url = get_provider_base_url(profile.provider_id);
	}
	if (profile.api_key.is_empty()) {
		profile.api_key = get_provider_api_key(profile.provider_id);
	}
	if (profile.display_name.is_empty()) {
		profile.display_name = _make_profile_display_name(profile.provider_name, profile.model);
	}
	return profile;
}

Dictionary AIModelSettings::_profile_to_dictionary(const AIModelProfile &p_profile) {
	Dictionary profile;
	profile["id"] = p_profile.id;
	profile["display_name"] = p_profile.display_name;
	profile["provider_id"] = p_profile.provider_id;
	profile["model"] = p_profile.model;
	profile["base_url"] = p_profile.base_url;
	profile["api_key"] = p_profile.api_key;
	profile["enabled"] = p_profile.enabled;
	profile["custom"] = p_profile.custom;
	profile["max_input_chars"] = MAX(256, p_profile.max_input_chars);
	profile["max_context_chars"] = MAX(128, p_profile.max_context_chars);
	profile["max_history_chars"] = MAX(128, p_profile.max_history_chars);
	profile["max_tool_result_chars"] = MAX(64, p_profile.max_tool_result_chars);
	profile["min_recent_messages"] = MAX(1, p_profile.min_recent_messages);
	profile["max_provider_turns"] = MAX(1, p_profile.max_provider_turns);
	profile["max_tool_calls"] = MAX(1, p_profile.max_tool_calls);
	profile["max_output_tokens"] = MAX(0, p_profile.max_output_tokens);
	profile["timeout_seconds"] = MAX(1, p_profile.timeout_seconds);
	return profile;
}

String AIModelSettings::_make_profile_id(const String &p_provider_id, const String &p_model) {
	return "profile:" + p_provider_id + ":" + p_model.validate_node_name().replace(" ", "_") + ":" + String::num_uint64(OS::get_singleton()->get_ticks_usec()) + ":" + itos(Math::rand());
}

String AIModelSettings::_make_profile_display_name(const String &p_provider_name, const String &p_model) {
	if (!p_provider_name.is_empty()) {
		return p_provider_name + " / " + p_model;
	}
	return p_model;
}

Vector<AIModelProviderPreset> AIModelSettings::get_provider_presets() {
	Vector<AIModelProviderPreset> presets;

	AIModelProviderPreset deepseek;
	deepseek.id = "deepseek";
	deepseek.display_name = "DeepSeek";
	deepseek.default_base_url = "https://api.deepseek.com";
	deepseek.preset_models.push_back("deepseek-v4-flash");
	deepseek.preset_models.push_back("deepseek-v4-pro");
	deepseek.preset_models.push_back("deepseek-chat");
	deepseek.preset_models.push_back("deepseek-reasoner");
	presets.push_back(deepseek);

	AIModelProviderPreset openai;
	openai.id = "openai";
	openai.display_name = "OpenAI";
	openai.default_base_url = "https://api.openai.com/v1";
	openai.preset_models.push_back("gpt-5.5");
	openai.preset_models.push_back("gpt-5.4");
	openai.preset_models.push_back("gpt-5.4-mini");
	openai.preset_models.push_back("gpt-5.4-nano");
	openai.preset_models.push_back("gpt-5.1");
	openai.preset_models.push_back("gpt-4.1");
	openai.preset_models.push_back("gpt-4.1-mini");
	presets.push_back(openai);

	AIModelProviderPreset openrouter;
	openrouter.id = "openrouter";
	openrouter.display_name = "OpenRouter";
	openrouter.default_base_url = "https://openrouter.ai/api/v1";
	openrouter.preset_models.push_back("openai/gpt-5.5");
	openrouter.preset_models.push_back("openai/gpt-5.4");
	openrouter.preset_models.push_back("anthropic/claude-sonnet-4.5");
	openrouter.preset_models.push_back("google/gemini-2.5-pro");
	presets.push_back(openrouter);

	AIModelProviderPreset compatible;
	compatible.id = "compatible";
	compatible.display_name = "OpenAI Compatible";
	compatible.default_base_url = "http://localhost:11434/v1";
	compatible.preset_models.push_back("llama3.1");
	compatible.preset_models.push_back("qwen2.5-coder");
	compatible.preset_models.push_back("deepseek-coder");
	presets.push_back(compatible);

	return presets;
}

String AIModelSettings::get_model_id(const String &p_provider_id, const String &p_model) {
	return p_provider_id + ":" + p_model;
}

AIModelDescriptor AIModelSettings::get_model(const String &p_model_id) {
	AIModelProfile profile = get_model_profile(p_model_id);
	if (!profile.id.is_empty()) {
		return profile;
	}

	Vector<AIModelProviderPreset> providers = get_provider_presets();
	for (int i = 0; i < providers.size(); i++) {
		const AIModelProviderPreset &provider = providers[i];
		for (int j = 0; j < provider.preset_models.size(); j++) {
			String model = provider.preset_models[j];
			if (get_model_id(provider.id, model) == p_model_id) {
				AIModelDescriptor descriptor;
				descriptor.id = p_model_id;
				descriptor.display_name = _make_profile_display_name(provider.display_name, model);
				descriptor.provider_id = provider.id;
				descriptor.provider_name = provider.display_name;
				descriptor.model = model;
				descriptor.base_url = get_provider_base_url(provider.id);
				descriptor.api_key = get_provider_api_key(provider.id);
				descriptor.enabled = is_model_enabled(provider.id, model);
				return descriptor;
			}
		}

		PackedStringArray custom_models = _split_custom_models(get_custom_models(provider.id));
		for (int j = 0; j < custom_models.size(); j++) {
			String model = custom_models[j];
			if (get_model_id(provider.id, model) == p_model_id) {
				AIModelDescriptor descriptor;
				descriptor.id = p_model_id;
				descriptor.display_name = _make_profile_display_name(provider.display_name, model);
				descriptor.provider_id = provider.id;
				descriptor.provider_name = provider.display_name;
				descriptor.model = model;
				descriptor.base_url = get_provider_base_url(provider.id);
				descriptor.api_key = get_provider_api_key(provider.id);
				descriptor.enabled = is_model_enabled(provider.id, model);
				descriptor.custom = true;
				return descriptor;
			}
		}
	}

	return AIModelDescriptor();
}

Vector<AIModelDescriptor> AIModelSettings::get_enabled_models() {
	const bool has_profile_storage = _has_profile_storage();
	Vector<AIModelDescriptor> enabled_models;
	Vector<AIModelProfile> profiles = get_model_profiles(true);
	for (int i = 0; i < profiles.size(); i++) {
		enabled_models.push_back(profiles[i]);
	}
	if (has_profile_storage || !enabled_models.is_empty()) {
		return enabled_models;
	}

	Vector<AIModelProviderPreset> providers = get_provider_presets();
	for (int i = 0; i < providers.size(); i++) {
		const AIModelProviderPreset &provider = providers[i];
		for (int j = 0; j < provider.preset_models.size(); j++) {
			AIModelDescriptor descriptor = get_model(get_model_id(provider.id, provider.preset_models[j]));
			if (descriptor.enabled) {
				enabled_models.push_back(descriptor);
			}
		}

		PackedStringArray custom_models = _split_custom_models(get_custom_models(provider.id));
		for (int j = 0; j < custom_models.size(); j++) {
			AIModelDescriptor descriptor = get_model(get_model_id(provider.id, custom_models[j]));
			if (descriptor.enabled) {
				enabled_models.push_back(descriptor);
			}
		}
	}
	return enabled_models;
}

AIProviderConfig AIModelSettings::get_provider_config(const String &p_model_id) {
	AIModelDescriptor descriptor = get_model(p_model_id);
	AIProviderConfig config;
	if (descriptor.id.is_empty()) {
		return config;
	}
	config.provider_name = descriptor.provider_name;
	config.base_url = descriptor.base_url;
	config.api_key = descriptor.api_key;
	config.model = descriptor.model;
	config.timeout_seconds = descriptor.timeout_seconds;
	config.max_input_chars = descriptor.max_input_chars;
	config.max_context_chars = descriptor.max_context_chars;
	config.max_history_chars = descriptor.max_history_chars;
	config.max_tool_result_chars = descriptor.max_tool_result_chars;
	config.min_recent_messages = descriptor.min_recent_messages;
	config.max_provider_turns = descriptor.max_provider_turns;
	config.max_tool_calls = descriptor.max_tool_calls;
	config.max_output_tokens = descriptor.max_output_tokens;
	return config;
}

String AIModelSettings::add_model_profile(const String &p_display_name, const String &p_provider_id, const String &p_model, const String &p_api_key, const String &p_base_url, bool p_custom) {
	const String model = p_model.strip_edges();
	ERR_FAIL_COND_V(model.is_empty(), String());

	AIModelProviderPreset provider = _get_provider_preset(p_provider_id);
	const String provider_name = provider.display_name.is_empty() ? p_provider_id : provider.display_name;

	AIModelProfile profile;
	profile.display_name = p_display_name.strip_edges();
	if (profile.display_name.is_empty()) {
		profile.display_name = _make_profile_display_name(provider_name, model);
	}
	profile.provider_id = p_provider_id;
	profile.provider_name = provider_name;
	profile.model = model;
	profile.base_url = p_base_url.strip_edges();
	if (profile.base_url.is_empty()) {
		profile.base_url = get_provider_base_url(p_provider_id);
	}
	profile.api_key = p_api_key.strip_edges();
	profile.enabled = true;
	profile.custom = p_custom;
	return add_model_profile_config(profile);
}

String AIModelSettings::add_model_profile_config(const AIModelProfile &p_profile) {
	const String model = p_profile.model.strip_edges();
	ERR_FAIL_COND_V(model.is_empty(), String());

	AIModelProviderPreset provider = _get_provider_preset(p_profile.provider_id);
	const String provider_name = provider.display_name.is_empty() ? p_profile.provider_id : provider.display_name;

	AIModelProfile profile = p_profile;
	profile.id = profile.id.is_empty() ? _make_profile_id(profile.provider_id, model) : profile.id;
	profile.display_name = profile.display_name.strip_edges();
	if (profile.display_name.is_empty()) {
		profile.display_name = _make_profile_display_name(provider_name, model);
	}
	profile.provider_name = provider_name;
	profile.model = model;
	profile.base_url = profile.base_url.strip_edges();
	if (profile.base_url.is_empty()) {
		profile.base_url = get_provider_base_url(profile.provider_id);
	}
	profile.api_key = profile.api_key.strip_edges();
	profile.enabled = true;
	Array profiles = _get_profile_storage();
	profiles.push_back(_profile_to_dictionary(profile));
	_set_profile_storage(profiles);
	return profile.id;
}

bool AIModelSettings::update_model_profile(const String &p_profile_id, const String &p_display_name, const String &p_provider_id, const String &p_model, const String &p_api_key, const String &p_base_url, bool p_custom) {
	const String model = p_model.strip_edges();
	ERR_FAIL_COND_V(p_profile_id.is_empty() || model.is_empty(), false);

	Array profiles = _get_profile_storage();
	for (int i = 0; i < profiles.size(); i++) {
		if (profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIModelProfile profile = _profile_from_dictionary(profiles[i]);
		if (profile.id != p_profile_id) {
			continue;
		}

		AIModelProfile updated_profile = profile;
		updated_profile.display_name = p_display_name;
		updated_profile.provider_id = p_provider_id;
		updated_profile.model = model;
		updated_profile.base_url = p_base_url;
		updated_profile.api_key = p_api_key;
		updated_profile.custom = p_custom;
		return update_model_profile_config(updated_profile);
	}

	return false;
}

bool AIModelSettings::update_model_profile_config(const AIModelProfile &p_profile) {
	const String model = p_profile.model.strip_edges();
	ERR_FAIL_COND_V(p_profile.id.is_empty() || model.is_empty(), false);

	Array profiles = _get_profile_storage();
	for (int i = 0; i < profiles.size(); i++) {
		if (profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIModelProfile existing = _profile_from_dictionary(profiles[i]);
		if (existing.id != p_profile.id) {
			continue;
		}

		AIModelProviderPreset provider = _get_provider_preset(p_profile.provider_id);
		const String provider_name = provider.display_name.is_empty() ? p_profile.provider_id : provider.display_name;
		AIModelProfile profile = p_profile;
		profile.display_name = profile.display_name.strip_edges();
		if (profile.display_name.is_empty()) {
			profile.display_name = _make_profile_display_name(provider_name, model);
		}
		profile.provider_name = provider_name;
		profile.model = model;
		profile.base_url = profile.base_url.strip_edges();
		if (profile.base_url.is_empty()) {
			profile.base_url = get_provider_base_url(profile.provider_id);
		}
		profile.api_key = profile.api_key.strip_edges();
		profile.enabled = true;
		profiles[i] = _profile_to_dictionary(profile);
		_set_profile_storage(profiles);
		return true;
	}

	return false;
}

bool AIModelSettings::remove_model_profile(const String &p_profile_id) {
	Array profiles = _get_profile_storage();
	for (int i = profiles.size() - 1; i >= 0; i--) {
		if (profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIModelProfile profile = _profile_from_dictionary(profiles[i]);
		if (profile.id == p_profile_id) {
			profiles.remove_at(i);
			_set_profile_storage(profiles);
			return true;
		}
	}
	return false;
}

AIModelProfile AIModelSettings::get_model_profile(const String &p_profile_id) {
	Array profiles = _get_profile_storage();
	for (int i = 0; i < profiles.size(); i++) {
		if (profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIModelProfile profile = _profile_from_dictionary(profiles[i]);
		if (profile.id == p_profile_id) {
			return profile;
		}
	}
	return AIModelProfile();
}

Vector<AIModelProfile> AIModelSettings::get_model_profiles(bool p_enabled_only) {
	Vector<AIModelProfile> model_profiles;
	Array profiles = _get_profile_storage();
	for (int i = 0; i < profiles.size(); i++) {
		if (profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIModelProfile profile = _profile_from_dictionary(profiles[i]);
		if (profile.id.is_empty() || profile.provider_id.is_empty() || profile.model.is_empty()) {
			continue;
		}
		if (!p_enabled_only || profile.enabled) {
			model_profiles.push_back(profile);
		}
	}
	return model_profiles;
}

Array AIModelSettings::get_model_profile_storage_for_test() {
	return _get_profile_storage();
}

void AIModelSettings::set_model_profile_storage_for_test(const Array &p_profiles) {
	_set_profile_storage(p_profiles);
}

void AIModelSettings::clear_model_profiles_for_test() {
	_set_profile_storage(Array());
}

void AIModelSettings::set_provider_auth(const String &p_provider_id, const String &p_api_key, const String &p_base_url) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);

	settings->set(_get_provider_path(p_provider_id, "api_key"), p_api_key.strip_edges());
	settings->set(_get_provider_path(p_provider_id, "base_url"), p_base_url.strip_edges());
}

String AIModelSettings::get_provider_api_key(const String &p_provider_id) {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return String();
	}
	String path = _get_provider_path(p_provider_id, "api_key");
	if (settings->has_setting(path)) {
		return String(settings->get(path));
	}
	if (p_provider_id == "deepseek") {
		return _get_legacy_deepseek_value("api_key", String());
	}
	return String();
}

String AIModelSettings::get_provider_base_url(const String &p_provider_id) {
	EditorSettings *settings = EditorSettings::get_singleton();
	Vector<AIModelProviderPreset> providers = get_provider_presets();
	for (int i = 0; i < providers.size(); i++) {
		if (providers[i].id != p_provider_id) {
			continue;
		}
		String path = _get_provider_path(p_provider_id, "base_url");
		if (settings && settings->has_setting(path)) {
			return String(settings->get(path));
		}
		if (p_provider_id == "deepseek") {
			return _get_legacy_deepseek_value("url", providers[i].default_base_url);
		}
		return providers[i].default_base_url;
	}
	return String();
}

void AIModelSettings::set_model_enabled(const String &p_provider_id, const String &p_model, bool p_enabled) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	settings->set(_get_model_path(p_provider_id, p_model), p_enabled);
}

void AIModelSettings::set_model_enabled(const String &p_model_id, bool p_enabled) {
	Vector<String> parts = p_model_id.split(":", true, 1);
	ERR_FAIL_COND(parts.size() != 2);
	set_model_enabled(parts[0], parts[1], p_enabled);
}

bool AIModelSettings::is_model_enabled(const String &p_provider_id, const String &p_model) {
	EditorSettings *settings = EditorSettings::get_singleton();
	String path = _get_model_path(p_provider_id, p_model);
	if (settings && settings->has_setting(path)) {
		return bool(settings->get(path));
	}
	return _is_default_model_enabled(p_provider_id, p_model);
}

bool AIModelSettings::is_model_enabled(const String &p_model_id) {
	Vector<String> parts = p_model_id.split(":", true, 1);
	ERR_FAIL_COND_V(parts.size() != 2, false);
	return is_model_enabled(parts[0], parts[1]);
}

void AIModelSettings::set_custom_models(const String &p_provider_id, const String &p_models) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	settings->set(_get_provider_path(p_provider_id, "custom_models"), p_models.strip_edges());
}

String AIModelSettings::get_custom_models(const String &p_provider_id) {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return String();
	}
	String path = _get_provider_path(p_provider_id, "custom_models");
	if (!settings->has_setting(path)) {
		return String();
	}
	return String(settings->get(path));
}
