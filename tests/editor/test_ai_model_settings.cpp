/**************************************************************************/
/*  test_ai_model_settings.cpp                                            */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/providers/ai_openai_compatible_provider.h"
#include "editor/ai_component/ui/ai_message_bubble.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/rich_text_label.h"

TEST_FORCE_LINK(test_ai_model_settings);

namespace TestAIModelSettings {

TEST_CASE("[Editor][AI] Model settings expose editable presets") {
	EditorSettings *settings = EditorSettings::get_singleton();
	REQUIRE(settings != nullptr);

	Vector<AIModelProviderPreset> providers = AIModelSettings::get_provider_presets();
	CHECK(providers.size() >= 4);

	AIModelSettings::set_provider_auth("openai", "test-key", "https://example.test/v1");
	AIModelSettings::set_model_enabled(AIModelSettings::get_model_id("openai", "gpt-5.4"), true);

	AIModelDescriptor openai_model = AIModelSettings::get_model(AIModelSettings::get_model_id("openai", "gpt-5.4"));
	CHECK(openai_model.id != openai_model.model);
	CHECK(openai_model.provider_id == "openai");
	CHECK(openai_model.model == "gpt-5.4");
	CHECK(openai_model.api_key == "test-key");
	CHECK(openai_model.base_url == "https://example.test/v1");
	CHECK(openai_model.enabled);

	AIProviderConfig openai_config = AIModelSettings::get_provider_config(openai_model.id);
	CHECK(openai_config.provider_name == "OpenAI");
	CHECK(openai_config.model == "gpt-5.4");
	CHECK(openai_config.api_key == "test-key");
	CHECK(openai_config.base_url == "https://example.test/v1");

	Vector<AIModelDescriptor> enabled_models = AIModelSettings::get_enabled_models();
	bool found_openai_model = false;
	for (int i = 0; i < enabled_models.size(); i++) {
		if (enabled_models[i].id == openai_model.id) {
			found_openai_model = true;
			break;
		}
	}
	CHECK(found_openai_model);
}

TEST_CASE("[Editor][AI] OpenAI compatible provider builds valid request paths") {
	CHECK(AIOpenAICompatibleProvider::build_request_path("") == "/chat/completions");
	CHECK(AIOpenAICompatibleProvider::build_request_path("/") == "/chat/completions");
	CHECK(AIOpenAICompatibleProvider::build_request_path("/v1") == "/v1/chat/completions");
	CHECK(AIOpenAICompatibleProvider::build_request_path("v1") == "/v1/chat/completions");
	CHECK(AIOpenAICompatibleProvider::build_request_path("/v1/") == "/v1/chat/completions");
	CHECK(AIOpenAICompatibleProvider::build_request_path("/v1/chat/completions") == "/v1/chat/completions");
}

TEST_CASE("[Editor][AI] Message bubbles render labels without BBCode markup") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "user";
	message["content"] = "hello [b]plain[/b]";

	bubble->set_message(message);

	RichTextLabel *label = Object::cast_to<RichTextLabel>(bubble->get_child(0));
	REQUIRE(label != nullptr);
	CHECK(label->get_parsed_text() == "You\nhello [b]plain[/b]");

	memdelete(bubble);
}

TEST_CASE("[Editor][AI] Message bubbles ignore null content") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "assistant";
	message["content"] = Variant();

	bubble->set_message(message);

	RichTextLabel *label = Object::cast_to<RichTextLabel>(bubble->get_child(0));
	REQUIRE(label != nullptr);
	CHECK(label->get_parsed_text() == "Assistant\n");

	message["content"] = "Ready.";
	bubble->set_message(message);
	CHECK(label->get_parsed_text() == "Assistant\nReady.");

	memdelete(bubble);
}

TEST_CASE("[Editor][AI] Message bubbles render tool events with metadata") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "tool";
	message["content"] = "res://player.gd";

	Dictionary metadata;
	metadata["tool_name"] = "project.read_file";
	metadata["status"] = "completed";
	message["metadata"] = metadata;

	bubble->set_message(message);

	RichTextLabel *label = Object::cast_to<RichTextLabel>(bubble->get_child(0));
	REQUIRE(label != nullptr);
	CHECK(label->get_parsed_text() == "Tool: project.read_file (completed)\nres://player.gd");

	memdelete(bubble);
}

} // namespace TestAIModelSettings
