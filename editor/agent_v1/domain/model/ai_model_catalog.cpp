/**************************************************************************/
/*  ai_model_catalog.cpp                                                  */
/**************************************************************************/

#include "ai_model_catalog.h"

#include "core/variant/variant.h"

namespace {

Array _make_model_array(const char *const *p_models, int p_count) {
	Array result;
	for (int i = 0; i < p_count; i++) {
		result.push_back(String(p_models[i]));
	}
	return result;
}

Dictionary _make_provider_preset(const String &p_id, const String &p_display_name, const String &p_default_base_url, const char *const *p_models, int p_model_count) {
	Dictionary preset;
	preset["id"] = p_id;
	preset["display_name"] = p_display_name;
	preset["default_base_url"] = p_default_base_url;
	preset["type"] = "openai-compatible";
	preset["preset_models"] = _make_model_array(p_models, p_model_count);
	return preset;
}

} // namespace

Array AIModelCatalog::list_provider_presets() {
	static const char *deepseek_models[] = {
		"deepseek-v4-flash",
		"deepseek-v4-pro",
		"deepseek-chat",
		"deepseek-reasoner",
	};
	static const char *openai_models[] = {
		"gpt-5.5",
		"gpt-5.4",
		"gpt-5.4-mini",
		"gpt-5.4-nano",
		"gpt-5.1",
		"gpt-4.1",
		"gpt-4.1-mini",
	};
	static const char *openrouter_models[] = {
		"openai/gpt-5.5",
		"openai/gpt-5.4",
		"anthropic/claude-sonnet-4.5",
		"google/gemini-2.5-pro",
	};
	static const char *compatible_models[] = {
		"llama3.1",
		"qwen2.5-coder",
		"deepseek-coder",
	};

	Array presets;
	presets.push_back(_make_provider_preset("deepseek", "DeepSeek", "https://api.deepseek.com", deepseek_models, sizeof(deepseek_models) / sizeof(deepseek_models[0])));
	presets.push_back(_make_provider_preset("openai", "OpenAI", "https://api.openai.com/v1", openai_models, sizeof(openai_models) / sizeof(openai_models[0])));
	presets.push_back(_make_provider_preset("openrouter", "OpenRouter", "https://openrouter.ai/api/v1", openrouter_models, sizeof(openrouter_models) / sizeof(openrouter_models[0])));
	presets.push_back(_make_provider_preset("compatible", "OpenAI Compatible", "http://localhost:11434/v1", compatible_models, sizeof(compatible_models) / sizeof(compatible_models[0])));
	return presets;
}

Dictionary AIModelCatalog::get_provider_preset(const String &p_provider_id) {
	const Array presets = list_provider_presets();
	for (int i = 0; i < presets.size(); i++) {
		if (presets[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary preset = presets[i];
		if (String(preset.get("id", String())) == p_provider_id) {
			return preset;
		}
	}
	return Dictionary();
}
