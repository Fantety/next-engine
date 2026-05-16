/**************************************************************************/
/*  test_ai_model_settings.cpp                                            */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/settings/editor_settings.h"

TEST_FORCE_LINK(test_ai_model_settings);

namespace TestAIModelSettings {

TEST_CASE("[Editor][AI] Model settings expose editable presets") {
	EditorSettings *settings = EditorSettings::get_singleton();
	REQUIRE(settings != nullptr);

	Vector<AIModelProviderPreset> providers = AIModelSettings::get_provider_presets();
	CHECK(providers.size() >= 4);

	Vector<AIModelDescriptor> enabled_models = AIModelSettings::get_enabled_models();
	CHECK(enabled_models.size() >= 2);
	CHECK(enabled_models[0].id != enabled_models[0].model);
	CHECK(enabled_models[0].base_url.begins_with("https://"));

	AIProviderConfig default_config = AIModelSettings::get_provider_config(enabled_models[0].id);
	CHECK(default_config.model == enabled_models[0].model);
	CHECK(default_config.base_url == enabled_models[0].base_url);

	AIModelSettings::set_provider_auth("openai", "test-key", "https://example.test/v1");
	AIModelSettings::set_model_enabled(AIModelSettings::get_model_id("openai", "gpt-5.4"), true);

	AIModelDescriptor openai_model = AIModelSettings::get_model(AIModelSettings::get_model_id("openai", "gpt-5.4"));
	CHECK(openai_model.provider_id == "openai");
	CHECK(openai_model.model == "gpt-5.4");
	CHECK(openai_model.api_key == "test-key");
	CHECK(openai_model.base_url == "https://example.test/v1");

	AIProviderConfig openai_config = AIModelSettings::get_provider_config(openai_model.id);
	CHECK(openai_config.provider_name == "OpenAI");
	CHECK(openai_config.model == "gpt-5.4");
	CHECK(openai_config.api_key == "test-key");
	CHECK(openai_config.base_url == "https://example.test/v1");
}

} // namespace TestAIModelSettings
