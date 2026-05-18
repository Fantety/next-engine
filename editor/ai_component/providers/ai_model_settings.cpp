/**************************************************************************/
/*  ai_model_settings.cpp                                                 */
/**************************************************************************/

#include "ai_model_settings.h"

#include "editor/settings/editor_settings.h"

String AIModelSettings::_get_provider_path(const String &p_provider_id, const String &p_property) {
	return "ai_agent/providers/" + p_provider_id + "/" + p_property;
}

String AIModelSettings::_get_model_path(const String &p_provider_id, const String &p_model) {
	return "ai_agent/models/" + get_model_id(p_provider_id, p_model) + "/enabled";
}

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
	Vector<AIModelProviderPreset> providers = get_provider_presets();
	for (int i = 0; i < providers.size(); i++) {
		const AIModelProviderPreset &provider = providers[i];
		for (int j = 0; j < provider.preset_models.size(); j++) {
			String model = provider.preset_models[j];
			if (get_model_id(provider.id, model) == p_model_id) {
				AIModelDescriptor descriptor;
				descriptor.id = p_model_id;
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
	Vector<AIModelDescriptor> enabled_models;
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
	return config;
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
