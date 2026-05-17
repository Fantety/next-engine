/**************************************************************************/
/*  test_ai_model_settings.cpp                                            */
/**************************************************************************/

#include "tests/test_macros.h"

#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/providers/ai_openai_compatible_provider.h"
#include "editor/ai_component/ui/ai_message_bubble.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/label.h"
#include "scene/gui/link_button.h"
#include "scene/gui/rich_text_label.h"

TEST_FORCE_LINK(test_ai_model_settings);

namespace TestAIModelSettings {

template <typename T>
T *find_child_of_type(Node *p_node) {
	if (!p_node) {
		return nullptr;
	}

	T *typed_node = Object::cast_to<T>(p_node);
	if (typed_node) {
		return typed_node;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		T *child = find_child_of_type<T>(p_node->get_child(i));
		if (child) {
			return child;
		}
	}

	return nullptr;
}

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

	RichTextLabel *label = find_child_of_type<RichTextLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	CHECK(title_label->get_text() == "You");
	CHECK(label->get_parsed_text() == "hello [b]plain[/b]");

	memdelete(bubble);
}

TEST_CASE("[Editor][AI] Message bubbles ignore null content") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "assistant";
	message["content"] = Variant();

	bubble->set_message(message);

	RichTextLabel *label = find_child_of_type<RichTextLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	CHECK(title_label->get_text() == "Assistant");
	CHECK(label->get_parsed_text() == "");

	message["content"] = "Ready.";
	bubble->set_message(message);
	CHECK(title_label->get_text() == "Assistant");
	CHECK(label->get_parsed_text() == "Ready.");

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

	RichTextLabel *label = find_child_of_type<RichTextLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	CHECK(title_label->get_text() == "Tool: project.read_file (completed)");
	CHECK(label->get_parsed_text() == "res://player.gd");

	memdelete(bubble);
}

TEST_CASE("[Editor][AI] Message bubbles render assistant tool call requests") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "assistant";
	message["content"] = "";

	Dictionary metadata;
	Array tool_calls;
	Dictionary call;
	call["id"] = "call_read";
	call["tool_name"] = "project.read_file";
	Dictionary arguments;
	arguments["path"] = "res://player.gd";
	arguments["reason"] = "Inspect a long script before suggesting edits so the dock can keep tool calls compact by default.";
	call["arguments"] = arguments;
	tool_calls.push_back(call);
	metadata["tool_calls"] = tool_calls;
	message["metadata"] = metadata;

	bubble->set_message(message);

	RichTextLabel *label = find_child_of_type<RichTextLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	const String parsed_text = label->get_parsed_text();
	CHECK(title_label->get_text() == "Tool Call");
	CHECK(parsed_text.contains("project.read_file"));
	CHECK(parsed_text.contains("res://player.gd"));
	CHECK(!parsed_text.contains("Inspect a long script before suggesting edits"));

	LinkButton *details_button = find_child_of_type<LinkButton>(bubble);
	REQUIRE(details_button != nullptr);
	CHECK(details_button->is_visible());
	CHECK(bubble->get_h_size_flags() == Control::SIZE_SHRINK_BEGIN);

	details_button->emit_signal(SceneStringName(pressed));
	CHECK(label->get_parsed_text().contains("Inspect a long script before suggesting edits"));
	CHECK(bubble->get_h_size_flags() == Control::SIZE_EXPAND_FILL);

	memdelete(bubble);
}

TEST_CASE("[Editor][AI] Message bubbles collapse long tool results") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "tool";
	message["content"] = "res://player.gd\nline 1: extends CharacterBody2D\nline 2: const SPEED = 320\nline 3: func _physics_process(delta): pass";

	Dictionary metadata;
	metadata["tool_name"] = "project.read_file";
	metadata["status"] = "completed";
	message["metadata"] = metadata;

	bubble->set_message(message);

	RichTextLabel *label = find_child_of_type<RichTextLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	const String collapsed_text = label->get_parsed_text();
	CHECK(title_label->get_text() == "Tool: project.read_file (completed)");
	CHECK(collapsed_text.contains("res://player.gd line 1: extends CharacterBody2D"));
	CHECK(!collapsed_text.contains("line 3: func _physics_process"));

	LinkButton *details_button = find_child_of_type<LinkButton>(bubble);
	REQUIRE(details_button != nullptr);
	CHECK(details_button->is_visible());
	CHECK(bubble->get_h_size_flags() == Control::SIZE_SHRINK_BEGIN);

	details_button->emit_signal(SceneStringName(pressed));
	CHECK(label->get_parsed_text().contains("line 3: func _physics_process"));
	CHECK(bubble->get_h_size_flags() == Control::SIZE_EXPAND_FILL);

	memdelete(bubble);
}

} // namespace TestAIModelSettings
