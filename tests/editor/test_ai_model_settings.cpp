/**************************************************************************/
/*  test_ai_model_settings.cpp                                            */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/markdown/markdown_parser.h"
#include "editor/ai_component/providers/ai_mcp_settings.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/providers/ai_openai_compatible_codec.h"
#include "editor/ai_component/skills/ai_skill_settings.h"
#include "editor/ai_component/ui/ai_agent_settings_dialog.h"
#include "editor/ai_component/ui/ai_markdown_label.h"
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

	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();

	AIModelSettings::clear_model_profiles_for_test();
	const String profile_id = AIModelSettings::add_model_profile("OpenAI Test", "openai", "gpt-5.4", "test-key", "https://example.test/v1", false);

	AIModelDescriptor openai_model = AIModelSettings::get_model(profile_id);
	CHECK(openai_model.id != openai_model.model);
	CHECK(openai_model.display_name == "OpenAI Test");
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

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
}

TEST_CASE("[Editor][AI] MCP settings manage server enable states") {
	Array original_servers = AIMCPSettings::get_server_storage_for_test();
	AIMCPSettings::clear_servers_for_test();

	const String filesystem_id = AIMCPSettings::add_server("Filesystem", "npx", "-y @modelcontextprotocol/server-filesystem .", "res://", "NODE_ENV=development", true);
	const String disabled_id = AIMCPSettings::add_server("Disabled Server", "python", "server.py", "res://addons/mcp", String(), false);

	CHECK(!filesystem_id.is_empty());
	CHECK(!disabled_id.is_empty());
	CHECK(filesystem_id != disabled_id);

	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(false);
	REQUIRE(servers.size() == 2);
	CHECK(servers[0].display_name == "Filesystem");
	CHECK(servers[0].transport == "stdio");
	CHECK(servers[0].command == "npx");
	CHECK(servers[0].arguments == "-y @modelcontextprotocol/server-filesystem .");
	CHECK(servers[0].working_directory == "res://");
	CHECK(servers[0].environment == "NODE_ENV=development");
	CHECK(servers[0].url.is_empty());
	CHECK(servers[0].headers.is_empty());
	CHECK(servers[0].enabled);
	CHECK_FALSE(servers[1].enabled);

	Vector<AIMCPServerConfig> enabled_servers = AIMCPSettings::get_servers(true);
	REQUIRE(enabled_servers.size() == 1);
	CHECK(enabled_servers[0].id == filesystem_id);

	CHECK(AIMCPSettings::set_server_enabled(disabled_id, true));
	enabled_servers = AIMCPSettings::get_servers(true);
	REQUIRE(enabled_servers.size() == 2);

	CHECK(AIMCPSettings::update_server(filesystem_id, "Filesystem Local", "node", "server.js", "res://tools", "FOO=bar", false));
	AIMCPServerConfig updated = AIMCPSettings::get_server(filesystem_id);
	CHECK(updated.display_name == "Filesystem Local");
	CHECK(updated.transport == "stdio");
	CHECK(updated.command == "node");
	CHECK(updated.arguments == "server.js");
	CHECK(updated.working_directory == "res://tools");
	CHECK(updated.environment == "FOO=bar");
	CHECK_FALSE(updated.enabled);

	CHECK(AIMCPSettings::remove_server(disabled_id));
	servers = AIMCPSettings::get_servers(false);
	REQUIRE(servers.size() == 1);
	CHECK(servers[0].id == filesystem_id);

	AIMCPSettings::set_server_storage_for_test(original_servers);
}

TEST_CASE("[Editor][AI] Agent settings dialog exposes skill rows") {
	EditorSettings *settings = EditorSettings::get_singleton();
	REQUIRE(settings != nullptr);

	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	AISkillSettings::clear_skills_for_test();

	AIAgentSettingsDialog dialog;
	dialog.build_for_test();
	CHECK(dialog.get_skill_table_row_count_for_test() == 0);
	dialog.add_skill_for_test("TDD", "Use when changing behavior.", "Write tests first.", true);
	CHECK(dialog.get_skill_table_row_count_for_test() == 1);
	dialog.save_settings_for_test();

	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] MCP settings import stdio and HTTP servers from JSON") {
	Array original_servers = AIMCPSettings::get_server_storage_for_test();
	AIMCPSettings::clear_servers_for_test();

	const String json = R"({
		"mcpServers": {
			"filesystem": {
				"command": "npx",
				"args": ["-y", "@modelcontextprotocol/server-filesystem", "."],
				"env": {"NODE_ENV": "development"}
			},
			"remote-docs": {
				"type": "streamable_http",
				"url": "https://mcp.example.test/mcp",
				"headers": {"Authorization": "Bearer test-token"}
			},
			"legacy-events": {
				"transport": "sse",
				"url": "https://mcp.example.test/sse"
			}
		}
	})";

	String error;
	CHECK(AIMCPSettings::import_servers_from_json(json, error));
	CHECK(error.is_empty());

	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(false);
	REQUIRE(servers.size() == 3);
	CHECK(servers[0].display_name == "filesystem");
	CHECK(servers[0].transport == "stdio");
	CHECK(servers[0].command == "npx");
	CHECK(servers[0].arguments == "-y @modelcontextprotocol/server-filesystem .");
	CHECK(servers[0].environment == "NODE_ENV=development");

	CHECK(servers[1].display_name == "remote-docs");
	CHECK(servers[1].transport == "streamable_http");
	CHECK(servers[1].url == "https://mcp.example.test/mcp");
	CHECK(servers[1].headers == "Authorization=Bearer test-token");
	CHECK(servers[1].command.is_empty());

	CHECK(servers[2].display_name == "legacy-events");
	CHECK(servers[2].transport == "sse");
	CHECK(servers[2].url == "https://mcp.example.test/sse");

	AIMCPSettings::set_server_storage_for_test(original_servers);
}

TEST_CASE("[Editor][AI] MCP settings import URL-only JSON servers as streamable HTTP") {
	Array original_servers = AIMCPSettings::get_server_storage_for_test();
	AIMCPSettings::clear_servers_for_test();

	const String json = R"({
		"mcpServers": {
			"remote-docs": {
				"url": "https://mcp.example.test/mcp"
			}
		}
	})";

	String error;
	CHECK(AIMCPSettings::import_servers_from_json(json, error));
	CHECK(error.is_empty());

	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(false);
	REQUIRE(servers.size() == 1);
	CHECK(servers[0].display_name == "remote-docs");
	CHECK(servers[0].transport == "streamable_http");
	CHECK(servers[0].url == "https://mcp.example.test/mcp");

	AIMCPSettings::set_server_storage_for_test(original_servers);
}

TEST_CASE("[Editor][AI] MCP settings expose invalid configs for status reporting") {
	Array original_servers = AIMCPSettings::get_server_storage_for_test();
	AIMCPSettings::clear_servers_for_test();

	Dictionary invalid_server;
	invalid_server["id"] = "mcp:invalid";
	invalid_server["display_name"] = "Invalid Remote";
	invalid_server["transport"] = "streamable_http";
	invalid_server["url"] = "";
	invalid_server["enabled"] = true;

	Array storage;
	storage.push_back(invalid_server);
	AIMCPSettings::set_server_storage_for_test(storage);

	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(false);
	REQUIRE(servers.size() == 1);
	CHECK(AIMCPSettings::get_servers(true).is_empty());

	Vector<AIMCPServerConfig> status_configs = AIMCPSettings::get_server_status_configs();
	REQUIRE(status_configs.size() == 1);
	CHECK(status_configs[0].display_name == "Invalid Remote");

	String error;
	CHECK_FALSE(AIMCPSettings::is_server_config_usable(status_configs[0], &error));
	CHECK(error.contains("URL"));

	AIMCPSettings::set_server_storage_for_test(original_servers);
}

TEST_CASE("[Editor][AI] Model profiles allow duplicate provider and model with user names") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	AIModelSettings::clear_model_profiles_for_test();

	const String primary_id = AIModelSettings::add_model_profile("OpenAI Primary", "openai", "gpt-5.4", "primary-key", "https://primary.example.test/v1", false);
	const String backup_id = AIModelSettings::add_model_profile("OpenAI Backup", "openai", "gpt-5.4", "backup-key", "https://backup.example.test/v1", false);

	CHECK(!primary_id.is_empty());
	CHECK(!backup_id.is_empty());
	CHECK(primary_id != backup_id);

	Vector<AIModelProfile> profiles = AIModelSettings::get_model_profiles();
	CHECK(profiles.size() == 2);
	CHECK(profiles[0].display_name == "OpenAI Primary");
	CHECK(profiles[1].display_name == "OpenAI Backup");
	CHECK(profiles[0].provider_id == "openai");
	CHECK(profiles[1].provider_id == "openai");
	CHECK(profiles[0].model == "gpt-5.4");
	CHECK(profiles[1].model == "gpt-5.4");

	AIProviderConfig primary_config = AIModelSettings::get_provider_config(primary_id);
	AIProviderConfig backup_config = AIModelSettings::get_provider_config(backup_id);
	CHECK(primary_config.model == "gpt-5.4");
	CHECK(primary_config.api_key == "primary-key");
	CHECK(primary_config.base_url == "https://primary.example.test/v1");
	CHECK(backup_config.model == "gpt-5.4");
	CHECK(backup_config.api_key == "backup-key");
	CHECK(backup_config.base_url == "https://backup.example.test/v1");

	CHECK(AIModelSettings::remove_model_profile(primary_id));
	profiles = AIModelSettings::get_model_profiles();
	REQUIRE(profiles.size() == 1);
	CHECK(profiles[0].id == backup_id);
	CHECK(profiles[0].display_name == "OpenAI Backup");
	CHECK(profiles[0].model == "gpt-5.4");

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
}

TEST_CASE("[Editor][AI] Model profiles preserve advanced agent configuration") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	AIModelSettings::clear_model_profiles_for_test();

	AIModelProfile profile;
	profile.display_name = "OpenAI Advanced";
	profile.provider_id = "openai";
	profile.model = "gpt-5.4";
	profile.base_url = "https://advanced.example.test/v1";
	profile.api_key = "advanced-key";
	profile.custom = false;
	profile.max_input_chars = 123456;
	profile.max_context_chars = 34567;
	profile.max_history_chars = 78901;
	profile.max_tool_result_chars = 2345;
	profile.min_recent_messages = 6;
	profile.max_provider_turns = 17;
	profile.max_tool_calls = 8;
	profile.max_output_tokens = 4096;
	profile.timeout_seconds = 45;

	const String profile_id = AIModelSettings::add_model_profile_config(profile);
	REQUIRE(!profile_id.is_empty());

	AIModelProfile stored = AIModelSettings::get_model_profile(profile_id);
	CHECK(stored.max_input_chars == 123456);
	CHECK(stored.max_context_chars == 34567);
	CHECK(stored.max_history_chars == 78901);
	CHECK(stored.max_tool_result_chars == 2345);
	CHECK(stored.min_recent_messages == 6);
	CHECK(stored.max_provider_turns == 17);
	CHECK(stored.max_tool_calls == 8);
	CHECK(stored.max_output_tokens == 4096);
	CHECK(stored.timeout_seconds == 45);

	AIProviderConfig config = AIModelSettings::get_provider_config(profile_id);
	CHECK(config.max_input_chars == 123456);
	CHECK(config.max_context_chars == 34567);
	CHECK(config.max_history_chars == 78901);
	CHECK(config.max_tool_result_chars == 2345);
	CHECK(config.min_recent_messages == 6);
	CHECK(config.max_provider_turns == 17);
	CHECK(config.max_tool_calls == 8);
	CHECK(config.max_output_tokens == 4096);
	CHECK(config.timeout_seconds == 45);

	stored.max_input_chars = 223456;
	stored.max_context_chars = 44567;
	stored.max_history_chars = 88901;
	stored.max_tool_result_chars = 3345;
	stored.min_recent_messages = 7;
	stored.max_provider_turns = 19;
	stored.max_tool_calls = 11;
	stored.max_output_tokens = 0;
	stored.timeout_seconds = 75;
	CHECK(AIModelSettings::update_model_profile_config(stored));

	AIProviderConfig updated_config = AIModelSettings::get_provider_config(profile_id);
	CHECK(updated_config.max_input_chars == 223456);
	CHECK(updated_config.max_context_chars == 44567);
	CHECK(updated_config.max_history_chars == 88901);
	CHECK(updated_config.max_tool_result_chars == 3345);
	CHECK(updated_config.min_recent_messages == 7);
	CHECK(updated_config.max_provider_turns == 19);
	CHECK(updated_config.max_tool_calls == 11);
	CHECK(updated_config.max_output_tokens == 0);
	CHECK(updated_config.timeout_seconds == 75);

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
}

TEST_CASE("[Editor][AI] OpenAI compatible provider builds valid request paths") {
	CHECK(AIOpenAICompatibleCodec::build_request_path("") == "/chat/completions");
	CHECK(AIOpenAICompatibleCodec::build_request_path("/") == "/chat/completions");
	CHECK(AIOpenAICompatibleCodec::build_request_path("/v1") == "/v1/chat/completions");
	CHECK(AIOpenAICompatibleCodec::build_request_path("v1") == "/v1/chat/completions");
	CHECK(AIOpenAICompatibleCodec::build_request_path("/v1/") == "/v1/chat/completions");
	CHECK(AIOpenAICompatibleCodec::build_request_path("/v1/chat/completions") == "/v1/chat/completions");
}

TEST_CASE("[Editor][AI] Preset models are opt-in by default") {
	EditorSettings *settings = EditorSettings::get_singleton();
	REQUIRE(settings != nullptr);

	const String chat_model_path = "ai_agent/models/" + AIModelSettings::get_model_id("deepseek", "deepseek-chat") + "/enabled";
	const String reasoner_model_path = "ai_agent/models/" + AIModelSettings::get_model_id("deepseek", "deepseek-reasoner") + "/enabled";
	const String legacy_chat_model_path = "deepseek/models/deepseek-chat";
	const String legacy_reasoner_model_path = "deepseek/models/deepseek-reasoner";

	const bool had_chat_model = settings->has_setting(chat_model_path);
	const bool had_reasoner_model = settings->has_setting(reasoner_model_path);
	const bool had_legacy_chat_model = settings->has_setting(legacy_chat_model_path);
	const bool had_legacy_reasoner_model = settings->has_setting(legacy_reasoner_model_path);
	const Variant original_chat_model = had_chat_model ? settings->get(chat_model_path) : Variant();
	const Variant original_reasoner_model = had_reasoner_model ? settings->get(reasoner_model_path) : Variant();
	const Variant original_legacy_chat_model = had_legacy_chat_model ? settings->get(legacy_chat_model_path) : Variant();
	const Variant original_legacy_reasoner_model = had_legacy_reasoner_model ? settings->get(legacy_reasoner_model_path) : Variant();

	settings->erase(chat_model_path);
	settings->erase(reasoner_model_path);
	settings->erase(legacy_chat_model_path);
	settings->erase(legacy_reasoner_model_path);

	CHECK_FALSE(AIModelSettings::is_model_enabled("deepseek", "deepseek-chat"));
	CHECK_FALSE(AIModelSettings::is_model_enabled("deepseek", "deepseek-reasoner"));

	if (had_chat_model) {
		settings->set(chat_model_path, original_chat_model);
	}
	if (had_reasoner_model) {
		settings->set(reasoner_model_path, original_reasoner_model);
	}
	if (had_legacy_chat_model) {
		settings->set(legacy_chat_model_path, original_legacy_chat_model);
	}
	if (had_legacy_reasoner_model) {
		settings->set(legacy_reasoner_model_path, original_legacy_reasoner_model);
	}
}

TEST_CASE("[Editor][AI] Settings dialog starts with an empty model table when no models are enabled") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	Vector<AIModelProviderPreset> providers = AIModelSettings::get_provider_presets();
	Vector<String> original_custom_models;
	Vector<Vector<bool>> original_preset_enabled;
	AIModelSettings::clear_model_profiles_for_test();
	for (int i = 0; i < providers.size(); i++) {
		original_custom_models.push_back(AIModelSettings::get_custom_models(providers[i].id));
		AIModelSettings::set_custom_models(providers[i].id, String());
		Vector<bool> enabled_states;
		for (int j = 0; j < providers[i].preset_models.size(); j++) {
			enabled_states.push_back(AIModelSettings::is_model_enabled(providers[i].id, providers[i].preset_models[j]));
			AIModelSettings::set_model_enabled(providers[i].id, providers[i].preset_models[j], false);
		}
		original_preset_enabled.push_back(enabled_states);
	}

	AIAgentSettingsDialog *dialog = memnew(AIAgentSettingsDialog);
	dialog->build_for_test();

	CHECK(dialog->get_model_table_row_count_for_test() == 0);
	CHECK(dialog->get_custom_model_table_row_count_for_test() == 0);

	memdelete(dialog);

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
	for (int i = 0; i < providers.size(); i++) {
		AIModelSettings::set_custom_models(providers[i].id, original_custom_models[i]);
		for (int j = 0; j < providers[i].preset_models.size(); j++) {
			AIModelSettings::set_model_enabled(providers[i].id, providers[i].preset_models[j], original_preset_enabled[i][j]);
		}
	}
}

TEST_CASE("[Editor][AI] Settings dialog adds enabled provider and custom models") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	const String original_custom_models = AIModelSettings::get_custom_models("compatible");

	AIModelSettings::clear_model_profiles_for_test();
	AIModelSettings::set_custom_models("compatible", String());

	AIAgentSettingsDialog *dialog = memnew(AIAgentSettingsDialog);
	dialog->build_for_test();

	dialog->add_provider_model_for_test("openai", "gpt-5.4", "settings-test-key");
	CHECK(dialog->get_model_table_row_count_for_test() == 1);
	Vector<AIModelProfile> profiles = AIModelSettings::get_model_profiles();
	REQUIRE(profiles.size() == 1);
	CHECK(profiles[0].provider_id == "openai");
	CHECK(profiles[0].model == "gpt-5.4");
	CHECK(profiles[0].api_key == "settings-test-key");

	dialog->add_custom_model_for_test("next-test-model", "https://example.test/v1", "compatible-test-key");
	CHECK(dialog->get_model_table_row_count_for_test() == 2);
	CHECK(dialog->get_custom_model_table_row_count_for_test() == 1);
	CHECK(AIModelSettings::get_custom_models("compatible").contains("next-test-model"));
	profiles = AIModelSettings::get_model_profiles();
	REQUIRE(profiles.size() == 2);
	CHECK(profiles[1].custom);
	CHECK(profiles[1].provider_id == "compatible");
	CHECK(profiles[1].model == "next-test-model");
	CHECK(profiles[1].api_key == "compatible-test-key");
	CHECK(profiles[1].base_url == "https://example.test/v1");

	dialog->remove_custom_model_for_test("compatible", "next-test-model");
	CHECK(dialog->get_custom_model_table_row_count_for_test() == 0);
	CHECK_FALSE(AIModelSettings::get_custom_models("compatible").contains("next-test-model"));
	CHECK(AIModelSettings::get_model_profiles().size() == 1);

	memdelete(dialog);

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
	AIModelSettings::set_custom_models("compatible", original_custom_models);
}

TEST_CASE("[Editor][AI] Settings dialog adds MCP servers") {
	Array original_servers = AIMCPSettings::get_server_storage_for_test();
	AIMCPSettings::clear_servers_for_test();

	AIAgentSettingsDialog *dialog = memnew(AIAgentSettingsDialog);
	dialog->build_for_test();

	CHECK(dialog->get_mcp_server_table_row_count_for_test() == 0);
	dialog->add_mcp_server_for_test("Filesystem", "npx", true);
	CHECK(dialog->get_mcp_server_table_row_count_for_test() == 1);

	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(false);
	REQUIRE(servers.size() == 1);
	CHECK(servers[0].display_name == "Filesystem");
	CHECK(servers[0].command == "npx");
	CHECK(servers[0].enabled);

	memdelete(dialog);

	AIMCPSettings::set_server_storage_for_test(original_servers);
}

TEST_CASE("[Editor][AI] Settings dialog edits added provider model credentials") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	AIModelSettings::clear_model_profiles_for_test();

	AIAgentSettingsDialog *dialog = memnew(AIAgentSettingsDialog);
	dialog->build_for_test();

	dialog->add_provider_model_for_test("openai", "gpt-5.4", "initial-key");
	dialog->edit_provider_model_for_test("openai", "gpt-5.4", "edited-key");

	Vector<AIModelProfile> profiles = AIModelSettings::get_model_profiles();
	REQUIRE(profiles.size() == 1);
	CHECK(profiles[0].api_key == "edited-key");
	CHECK(profiles[0].model == "gpt-5.4");
	CHECK(dialog->get_model_table_row_count_for_test() == 1);

	memdelete(dialog);

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
}

TEST_CASE("[Editor][AI] Settings dialog edits custom model identity and provider auth") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	const String original_custom_models = AIModelSettings::get_custom_models("compatible");

	AIModelSettings::clear_model_profiles_for_test();
	AIModelSettings::set_custom_models("compatible", String());

	AIAgentSettingsDialog *dialog = memnew(AIAgentSettingsDialog);
	dialog->build_for_test();

	dialog->add_custom_model_for_test("next-old-model", "https://old.example.test/v1", "old-key");
	dialog->edit_custom_model_for_test("next-old-model", "next-new-model", "https://new.example.test/v1", "new-key");

	CHECK(AIModelSettings::get_custom_models("compatible").contains("next-new-model"));
	Vector<AIModelProfile> profiles = AIModelSettings::get_model_profiles();
	REQUIRE(profiles.size() == 1);
	CHECK(profiles[0].custom);
	CHECK(profiles[0].model == "next-new-model");
	CHECK(profiles[0].base_url == "https://new.example.test/v1");
	CHECK(profiles[0].api_key == "new-key");
	CHECK(dialog->get_custom_model_table_row_count_for_test() == 1);

	memdelete(dialog);

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
	AIModelSettings::set_custom_models("compatible", original_custom_models);
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

TEST_CASE("[Editor][AI] Markdown labels render common AI response markdown") {
	AIMarkdownLabel *label = memnew(AIMarkdownLabel);

	label->set_markdown("# Plan\n\nUse **bold**, *italic*, and `code`.\n\n- inspect project\n- apply patch\n\n```gdscript\nextends Node\n```");

	const String parsed_text = label->get_parsed_text();
	CHECK(parsed_text.contains("Plan"));
	CHECK(parsed_text.contains("Use bold, italic, and code."));
	CHECK(parsed_text.contains("inspect project"));
	CHECK(parsed_text.contains("apply patch"));
	CHECK(parsed_text.contains("extends Node"));

	memdelete(label);
}

TEST_CASE("[Editor][AI] Markdown labels render list items without blank rows") {
	AIMarkdownLabel *label = memnew(AIMarkdownLabel);

	label->set_markdown("1. First\n2. Second\n3. Third\n\n- Alpha\n- Beta");

	const String parsed_text = label->get_parsed_text();
	CHECK(parsed_text.contains("First\nSecond\nThird"));
	CHECK(parsed_text.contains("Alpha\nBeta"));
	CHECK_FALSE(parsed_text.contains("First\n\nSecond"));
	CHECK_FALSE(parsed_text.contains("Alpha\n\nBeta"));

	memdelete(label);
}

TEST_CASE("[Editor][AI] Markdown labels render pipe tables") {
	AIMarkdownLabel *label = memnew(AIMarkdownLabel);

	label->set_markdown("Before\n\n| Name | Value |\n| --- | --- |\n| **HP** | `100` |\n| MP | 50 |\n\nAfter");

	const String parsed_text = label->get_parsed_text();
	CHECK(parsed_text.contains("Before"));
	CHECK(parsed_text.contains("Name"));
	CHECK(parsed_text.contains("Value"));
	CHECK(parsed_text.contains("HP"));
	CHECK(parsed_text.contains("100"));
	CHECK(parsed_text.contains("MP"));
	CHECK(parsed_text.contains("50"));
	CHECK(parsed_text.contains("After"));
	CHECK_FALSE(parsed_text.contains("---"));

	memdelete(label);
}

TEST_CASE("[Editor][AI] Markdown parser preserves links code fences and children") {
	MarkdownParser parser;
	Ref<MarkdownNode> root = parser.parse_markdown("[Godot](https://godotengine.org)\n\n```gdscript\nextends Node\n```");

	REQUIRE(root.is_valid());
	Dictionary document = root->to_dictionary();
	REQUIRE(document.has("children"));
	Array document_children = document["children"];
	REQUIRE(document_children.size() == 2);

	Dictionary paragraph = document_children[0];
	REQUIRE(paragraph.has("children"));
	Array paragraph_children = paragraph["children"];
	REQUIRE(paragraph_children.size() == 1);
	Dictionary link = paragraph_children[0];
	CHECK(String(link["url"]) == "https://godotengine.org");

	Dictionary code_block = document_children[1];
	CHECK(String(code_block["fence_info"]) == "gdscript");
	CHECK(String(code_block["literal"]).contains("extends Node"));
}

TEST_CASE("[Editor][AI] Markdown labels keep BBCode-looking text as plain text") {
	AIMarkdownLabel *label = memnew(AIMarkdownLabel);

	label->set_markdown("Do not parse [b]BBCode[/b] from model output.");

	CHECK(label->get_parsed_text().strip_edges() == "Do not parse [b]BBCode[/b] from model output.");

	memdelete(label);
}

TEST_CASE("[Editor][AI] Assistant message bubbles render markdown while user bubbles stay plain") {
	AIMessageBubble *assistant_bubble = memnew(AIMessageBubble);
	Dictionary assistant_message;
	assistant_message["role"] = "assistant";
	assistant_message["content"] = "Use **bold** and `code`.";

	assistant_bubble->set_message(assistant_message);

	RichTextLabel *assistant_label = find_child_of_type<RichTextLabel>(assistant_bubble);
	REQUIRE(assistant_label != nullptr);
	CHECK(assistant_label->get_parsed_text().strip_edges() == "Use bold and code.");

	AIMessageBubble *user_bubble = memnew(AIMessageBubble);
	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = "Literal **stars**";

	user_bubble->set_message(user_message);

	RichTextLabel *user_label = find_child_of_type<RichTextLabel>(user_bubble);
	REQUIRE(user_label != nullptr);
	CHECK(user_label->get_parsed_text().strip_edges() == "Literal **stars**");

	memdelete(assistant_bubble);
	memdelete(user_bubble);
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

TEST_CASE("[Editor][AI] Message bubbles render MCP tool execution metadata") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "tool";
	message["content"] = "Read 3 files.";

	Dictionary metadata;
	metadata["tool_name"] = "mcp_filesystem_read_file";
	metadata["status"] = "completed";
	metadata["tool_origin"] = "mcp";
	metadata["mcp_server_name"] = "Filesystem";
	metadata["mcp_server_id"] = "mcp:filesystem";
	metadata["mcp_tool_name"] = "read_file";
	metadata["mcp_transport"] = "stdio";
	metadata["mcp_agent_tool_name"] = "mcp_filesystem_read_file";
	message["metadata"] = metadata;

	bubble->set_message(message);

	RichTextLabel *label = find_child_of_type<RichTextLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	LinkButton *details_button = find_child_of_type<LinkButton>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	REQUIRE(details_button != nullptr);

	CHECK(title_label->get_text() == "MCP: Filesystem / read_file (completed)");
	CHECK(label->get_parsed_text().contains("Read 3 files."));
	CHECK(details_button->is_visible());

	details_button->emit_signal(SceneStringName(pressed));
	const String expanded_text = label->get_parsed_text();
	CHECK(expanded_text.contains("MCP Server: Filesystem"));
	CHECK(expanded_text.contains("MCP Tool: read_file"));
	CHECK(expanded_text.contains("Transport: stdio"));
	CHECK(expanded_text.contains("Agent Tool: mcp_filesystem_read_file"));
	CHECK(expanded_text.contains("Read 3 files."));

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

TEST_CASE("[Editor][AI] Message bubbles keep streamed assistant text when tool calls arrive") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "assistant";
	message["content"] = "I will inspect the project before editing.";

	Dictionary metadata;
	Array tool_calls;
	Dictionary call;
	call["id"] = "call_read";
	call["tool_name"] = "project.read_file";
	Dictionary arguments;
	arguments["path"] = "res://player.gd";
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
	CHECK(title_label->get_text() == "Assistant");
	CHECK(parsed_text.contains("I will inspect the project before editing."));
	CHECK(parsed_text.contains("project.read_file"));
	CHECK(parsed_text.contains("res://player.gd"));

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
