/**************************************************************************/
/*  test_ai_model_settings.cpp                                            */
/**************************************************************************/

#include "tests/test_macros.h"

#include "core/markdown/markdown_parser.h"
#include "editor/ai_component/providers/ai_mcp_settings.h"
#include "editor/ai_component/providers/ai_model_settings.h"
#include "editor/ai_component/providers/ai_openai_compatible_codec.h"
#include "editor/ai_component/rules/ai_rule_settings.h"
#include "editor/ai_component/skills/ai_skill_settings.h"
#include "editor/ai_component/ui/ai_next_marquee_settings.h"
#include "editor/ai_component/ui/ai_agent_settings_dialog.h"
#include "editor/ai_component/ui/ai_markdown_label.h"
#include "editor/ai_component/ui/ai_message_bubble.h"
#include "editor/ai_component/ui/ai_message_list.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/label.h"
#include "scene/gui/link_button.h"
#include "scene/gui/markdown_viewer.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/text_edit.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/style_box_flat.h"

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

template <typename T>
T *find_last_child_of_type(Node *p_node) {
	if (!p_node) {
		return nullptr;
	}

	T *last = Object::cast_to<T>(p_node);
	for (int i = 0; i < p_node->get_child_count(); i++) {
		T *child = find_last_child_of_type<T>(p_node->get_child(i));
		if (child) {
			last = child;
		}
	}

	return last;
}

void process_scene_frames(int p_count) {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);
	for (int i = 0; i < p_count; i++) {
		tree->process(0.0);
	}
}

real_t get_scroll_viewport_bottom(AIMessageList *p_list) {
	REQUIRE(p_list != nullptr);
	VScrollBar *scroll_bar = p_list->get_v_scroll_bar();
	REQUIRE(scroll_bar != nullptr);
	VBoxContainer *message_box = find_child_of_type<VBoxContainer>(p_list);
	REQUIRE(message_box != nullptr);
	return message_box->get_global_position().y + scroll_bar->get_value() + scroll_bar->get_page();
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
	CHECK_FALSE(openai_config.supports_multimodal);

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

TEST_CASE("[Editor][AI] Model profiles persist multimodal capability and API format") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	AIModelSettings::clear_model_profiles_for_test();

	AIModelProfile profile;
	profile.id = "profile:fixture:vision";
	profile.display_name = "Vision Fixture";
	profile.provider_id = "compatible";
	profile.provider_name = "OpenAI Compatible";
	profile.model = "fixture-vision-model";
	profile.base_url = "https://fixture.example.test/v1";
	profile.api_key = "fixture-key";
	profile.enabled = true;
	profile.custom = true;
	profile.supports_multimodal = true;
	profile.api_format = "openai_chat_completions";

	const String profile_id = AIModelSettings::add_model_profile_config(profile);
	AIModelProfile stored_profile = AIModelSettings::get_model_profile(profile_id);
	CHECK(stored_profile.supports_multimodal);
	CHECK(stored_profile.api_format == "openai_chat_completions");

	AIProviderConfig config = AIModelSettings::get_provider_config(profile_id);
	CHECK(config.supports_multimodal);
	CHECK(config.api_format == "openai_chat_completions");

	stored_profile.supports_multimodal = false;
	stored_profile.api_format = "openai_responses";
	CHECK(AIModelSettings::update_model_profile_config(stored_profile));

	AIModelProfile updated_profile = AIModelSettings::get_model_profile(profile_id);
	CHECK_FALSE(updated_profile.supports_multimodal);
	CHECK(updated_profile.api_format == "openai_responses");

	AIProviderConfig updated_config = AIModelSettings::get_provider_config(profile_id);
	CHECK_FALSE(updated_config.supports_multimodal);
	CHECK(updated_config.api_format == "openai_responses");

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
}

TEST_CASE("[Editor][AI] Settings ignore stale bundled sample configurations") {
	Array original_profiles = AIModelSettings::get_model_profile_storage_for_test();
	Array original_servers = AIMCPSettings::get_server_storage_for_test();
	Array original_skills = AISkillSettings::get_skill_storage_for_test();
	Array original_rules = AIRuleSettings::get_rule_storage_for_test();

	Dictionary stale_profile;
	stale_profile["id"] = "profile:test:vision";
	stale_profile["display_name"] = "Vision Test";
	stale_profile["provider_id"] = "compatible";
	stale_profile["model"] = "vision-model";
	stale_profile["base_url"] = "https://example.test/v1";
	stale_profile["api_key"] = "test-key";
	stale_profile["enabled"] = true;
	stale_profile["custom"] = true;

	Dictionary kept_profile = stale_profile.duplicate(true);
	kept_profile["id"] = "profile:user:vision";
	kept_profile["display_name"] = "User Vision";
	kept_profile["model"] = "user-vision-model";

	Array profiles;
	profiles.push_back(stale_profile);
	profiles.push_back(kept_profile);
	AIModelSettings::set_model_profile_storage_for_test(profiles);

	Dictionary stale_stdio_server;
	stale_stdio_server["id"] = "mcp:filesystem";
	stale_stdio_server["display_name"] = "filesystem";
	stale_stdio_server["transport"] = "stdio";
	stale_stdio_server["command"] = "npx";
	stale_stdio_server["arguments"] = "-y @modelcontextprotocol/server-filesystem .";
	stale_stdio_server["working_directory"] = "res://";
	stale_stdio_server["environment"] = "NODE_ENV=development";
	stale_stdio_server["enabled"] = true;

	Dictionary stale_http_server;
	stale_http_server["id"] = "mcp:remote-docs";
	stale_http_server["display_name"] = "remote-docs";
	stale_http_server["transport"] = "streamable_http";
	stale_http_server["url"] = "https://mcp.example.test/mcp";
	stale_http_server["headers"] = "Authorization=Bearer test-token";
	stale_http_server["enabled"] = true;

	Dictionary stale_sse_server;
	stale_sse_server["id"] = "mcp:legacy-events";
	stale_sse_server["display_name"] = "legacy-events";
	stale_sse_server["transport"] = "sse";
	stale_sse_server["url"] = "https://mcp.example.test/sse";
	stale_sse_server["enabled"] = true;

	Dictionary kept_server = stale_http_server.duplicate(true);
	kept_server["id"] = "mcp:user-docs";
	kept_server["display_name"] = "user-docs";
	kept_server["url"] = "https://docs.example.com/mcp";

	Array servers;
	servers.push_back(stale_stdio_server);
	servers.push_back(stale_http_server);
	servers.push_back(stale_sse_server);
	servers.push_back(kept_server);
	AIMCPSettings::set_server_storage_for_test(servers);

	Dictionary stale_skill;
	stale_skill["id"] = "skill:tdd";
	stale_skill["display_name"] = "TDD";
	stale_skill["description"] = "Use when changing behavior.";
	stale_skill["content"] = "Write tests first.";
	stale_skill["kind"] = "prompt_context";
	stale_skill["enabled"] = true;

	Dictionary kept_skill = stale_skill.duplicate(true);
	kept_skill["id"] = "skill:user";
	kept_skill["display_name"] = "User Skill";
	kept_skill["content"] = "User content.";

	Array skills;
	skills.push_back(stale_skill);
	skills.push_back(kept_skill);
	AISkillSettings::set_skill_storage_for_test(skills);

	Dictionary stale_enabled_rule;
	stale_enabled_rule["id"] = "rule:stale-enabled";
	stale_enabled_rule["content"] = "Prefer concise answers.";
	stale_enabled_rule["enabled"] = true;

	Dictionary stale_disabled_rule;
	stale_disabled_rule["id"] = "rule:stale-disabled";
	stale_disabled_rule["content"] = "Disabled rule should not appear.";
	stale_disabled_rule["enabled"] = false;

	Dictionary kept_rule;
	kept_rule["id"] = "rule:user";
	kept_rule["content"] = "Use project terminology.";
	kept_rule["enabled"] = true;

	Array rules;
	rules.push_back(stale_enabled_rule);
	rules.push_back(stale_disabled_rule);
	rules.push_back(kept_rule);
	AIRuleSettings::set_rule_storage_for_test(rules);

	Vector<AIModelProfile> filtered_profiles = AIModelSettings::get_model_profiles(false);
	REQUIRE(filtered_profiles.size() == 1);
	CHECK(filtered_profiles[0].display_name == "User Vision");

	Vector<AIMCPServerConfig> filtered_servers = AIMCPSettings::get_servers(false);
	REQUIRE(filtered_servers.size() == 1);
	CHECK(filtered_servers[0].display_name == "user-docs");

	Vector<AISkillConfig> filtered_skills = AISkillSettings::get_skills(false);
	REQUIRE(filtered_skills.size() == 1);
	CHECK(filtered_skills[0].display_name == "User Skill");

	Vector<AIRuleConfig> filtered_rules = AIRuleSettings::get_rules(false);
	REQUIRE(filtered_rules.size() == 1);
	CHECK(filtered_rules[0].content == "Use project terminology.");

	AIModelSettings::set_model_profile_storage_for_test(original_profiles);
	AIMCPSettings::set_server_storage_for_test(original_servers);
	AISkillSettings::set_skill_storage_for_test(original_skills);
	AIRuleSettings::set_rule_storage_for_test(original_rules);
}

TEST_CASE("[Editor][AI] Model profiles and provider configs share runtime options") {
	AIModelRuntimeOptions defaults;
	AIModelProfile profile;
	AIProviderConfig config;

	CHECK(defaults.max_context_chars == profile.max_context_chars);
	CHECK(defaults.max_context_chars == config.max_context_chars);

	AIModelRuntimeOptions *profile_options = &profile;
	AIModelRuntimeOptions *config_options = &config;

	REQUIRE(profile_options != nullptr);
	REQUIRE(config_options != nullptr);

	profile_options->supports_multimodal = true;
	config_options->max_tool_calls = 12;

	CHECK(profile.supports_multimodal);
	CHECK(config.max_tool_calls == 12);
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
	dialog.add_skill_for_test("Fixture Skill", "Use in settings tests.", "Fixture skill body.", true);
	CHECK(dialog.get_skill_table_row_count_for_test() == 1);
	dialog.save_settings_for_test();

	AISkillSettings::set_skill_storage_for_test(original_skills);
}

TEST_CASE("[Editor][AI] Agent settings dialog exposes rule rows") {
	EditorSettings *settings = EditorSettings::get_singleton();
	REQUIRE(settings != nullptr);

	Array original_rules = AIRuleSettings::get_rule_storage_for_test();
	AIRuleSettings::clear_rules_for_test();

	AIAgentSettingsDialog dialog;
	dialog.build_for_test();
	CHECK(dialog.get_rule_table_row_count_for_test() == 0);
	dialog.add_rule_for_test("Always inspect errors before changing code.", true);
	CHECK(dialog.get_rule_table_row_count_for_test() == 1);
	dialog.save_settings_for_test();

	AIRuleSettings::set_rule_storage_for_test(original_rules);
}

TEST_CASE("[Editor][AI] NEXT marquee settings provide read-only presets and multiple custom shaders") {
	EditorSettings *settings = EditorSettings::get_singleton();
	REQUIRE(settings != nullptr);

	const String original_preset = AINextMarqueeSettings::get_current_preset_id();
	Array original_custom_marquees = AINextMarqueeSettings::get_custom_marquee_storage_for_test();
	AINextMarqueeSettings::clear_custom_marquees_for_test();

	Vector<AINextMarqueePreset> presets = AINextMarqueeSettings::get_presets();
	CHECK(presets.size() >= 4);
	REQUIRE(!presets.is_empty());
	CHECK_FALSE(presets[0].custom);
	CHECK(presets[0].shader_code.contains("shader_type canvas_item"));
	CHECK(AINextMarqueeSettings::get_preset("caution").shader_code.contains("stripe_count"));

	CHECK(AINextMarqueeSettings::set_current_preset_id(presets[0].id));
	CHECK(AINextMarqueeSettings::get_effective_shader_code() == presets[0].shader_code);

	const String custom_shader_a = "shader_type canvas_item;\nvoid fragment() { COLOR = vec4(UV.x, UV.y, 1.0, 1.0); }\n";
	const String custom_shader_b = "shader_type canvas_item;\nvoid fragment() { COLOR = vec4(1.0, UV.x, UV.y, 1.0); }\n";
	const String custom_a_id = AINextMarqueeSettings::add_custom_marquee("Blue Custom", custom_shader_a);
	const String custom_b_id = AINextMarqueeSettings::add_custom_marquee("Warm Custom", custom_shader_b);
	CHECK(!custom_a_id.is_empty());
	CHECK(!custom_b_id.is_empty());
	CHECK(custom_a_id != custom_b_id);

	Vector<AINextMarqueePreset> all_marquees = AINextMarqueeSettings::get_presets();
	CHECK(all_marquees.size() >= presets.size() + 2);

	CHECK(AINextMarqueeSettings::set_current_preset_id(custom_b_id));
	CHECK(AINextMarqueeSettings::get_effective_shader_code() == custom_shader_b);
	CHECK_FALSE(AINextMarqueeSettings::set_current_preset_id("missing_marquee"));

	AINextMarqueeSettings::set_custom_marquee_storage_for_test(original_custom_marquees);
	AINextMarqueeSettings::set_current_preset_id(original_preset);
}

TEST_CASE("[Editor][AI] NEXT marquee settings dialog lists built-ins and added custom marquees") {
	EditorSettings *settings = EditorSettings::get_singleton();
	REQUIRE(settings != nullptr);

	const String original_preset = AINextMarqueeSettings::get_current_preset_id();
	Array original_custom_marquees = AINextMarqueeSettings::get_custom_marquee_storage_for_test();
	AINextMarqueeSettings::clear_custom_marquees_for_test();

	AIAgentSettingsDialog dialog;
	dialog.build_for_test();
	const int initial_marquee_count = dialog.get_next_marquee_preset_count_for_test();
	CHECK(initial_marquee_count >= 4);

	dialog.select_next_marquee_preset_for_test("aurora");
	CHECK(AINextMarqueeSettings::get_current_preset_id() == "aurora");

	const String custom_id = dialog.add_next_marquee_for_test("Dialog Custom", "shader_type canvas_item;\nvoid fragment() { COLOR = vec4(0.2, 0.4, 1.0, 1.0); }\n");
	CHECK(!custom_id.is_empty());
	CHECK(dialog.get_next_marquee_preset_count_for_test() == initial_marquee_count + 1);

	dialog.select_next_marquee_preset_for_test(custom_id);
	CHECK(AINextMarqueeSettings::get_current_preset_id() == custom_id);

	AINextMarqueeSettings::set_custom_marquee_storage_for_test(original_custom_marquees);
	AINextMarqueeSettings::set_current_preset_id(original_preset);
}

TEST_CASE("[Editor][AI] MCP settings import stdio and HTTP servers from JSON") {
	Array original_servers = AIMCPSettings::get_server_storage_for_test();
	AIMCPSettings::clear_servers_for_test();

	const String json = R"({
		"mcpServers": {
			"fixture-files": {
				"command": "npx",
				"args": ["-y", "@modelcontextprotocol/server-filesystem", "."],
				"env": {"NODE_ENV": "development"}
			},
			"fixture-docs": {
				"type": "streamable_http",
				"url": "https://mcp.example.test/mcp",
				"headers": {"Authorization": "Bearer test-token"}
			},
			"fixture-events": {
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
	CHECK(servers[0].display_name == "fixture-files");
	CHECK(servers[0].transport == "stdio");
	CHECK(servers[0].command == "npx");
	CHECK(servers[0].arguments == "-y @modelcontextprotocol/server-filesystem .");
	CHECK(servers[0].environment == "NODE_ENV=development");

	CHECK(servers[1].display_name == "fixture-docs");
	CHECK(servers[1].transport == "streamable_http");
	CHECK(servers[1].url == "https://mcp.example.test/mcp");
	CHECK(servers[1].headers == "Authorization=Bearer test-token");
	CHECK(servers[1].command.is_empty());

	CHECK(servers[2].display_name == "fixture-events");
	CHECK(servers[2].transport == "sse");
	CHECK(servers[2].url == "https://mcp.example.test/sse");

	AIMCPSettings::set_server_storage_for_test(original_servers);
}

TEST_CASE("[Editor][AI] MCP settings import URL-only JSON servers as streamable HTTP") {
	Array original_servers = AIMCPSettings::get_server_storage_for_test();
	AIMCPSettings::clear_servers_for_test();

	const String json = R"({
		"mcpServers": {
			"fixture-docs": {
				"url": "https://mcp.example.test/mcp"
			}
		}
	})";

	String error;
	CHECK(AIMCPSettings::import_servers_from_json(json, error));
	CHECK(error.is_empty());

	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_servers(false);
	REQUIRE(servers.size() == 1);
	CHECK(servers[0].display_name == "fixture-docs");
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

	AIMarkdownLabel *label = find_child_of_type<AIMarkdownLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	CHECK(find_child_of_type<MarkdownViewer>(bubble) != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(bubble) == nullptr);
	CHECK(title_label->get_text() == "You");
	CHECK(label->get_parsed_text() == "hello [b]plain[/b]");

	memdelete(bubble);
}

TEST_CASE("[Editor][AI] Markdown labels render common AI response markdown") {
	AIMarkdownLabel *label = memnew(AIMarkdownLabel);

	label->set_markdown("# Plan\n\nUse **bold**, *italic*, and `code`.\n\n- inspect project\n- apply patch\n\n```gdscript\nextends Node\n```");

	CHECK(find_child_of_type<MarkdownViewer>(label) != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(label) == nullptr);
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

	AIMarkdownLabel *assistant_label = find_child_of_type<AIMarkdownLabel>(assistant_bubble);
	REQUIRE(assistant_label != nullptr);
	CHECK(find_child_of_type<MarkdownViewer>(assistant_bubble) != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(assistant_bubble) == nullptr);
	CHECK(assistant_label->get_parsed_text().strip_edges() == "Use bold and code.");
	CHECK(assistant_bubble->get_h_size_flags() == Control::SIZE_EXPAND_FILL);
	CHECK(assistant_bubble->has_theme_stylebox_override(SceneStringName(panel)));
	Ref<StyleBoxFlat> assistant_style = assistant_bubble->get_theme_stylebox(SceneStringName(panel));
	REQUIRE(assistant_style.is_valid());
	CHECK_FALSE(assistant_style->is_anti_aliased());
	CHECK(assistant_style->get_border_width(SIDE_BOTTOM) >= 2);

	AIMessageBubble *user_bubble = memnew(AIMessageBubble);
	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = "Literal **stars**";

	user_bubble->set_message(user_message);

	AIMarkdownLabel *user_label = find_child_of_type<AIMarkdownLabel>(user_bubble);
	REQUIRE(user_label != nullptr);
	CHECK(find_child_of_type<MarkdownViewer>(user_bubble) != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(user_bubble) == nullptr);
	CHECK(user_label->get_parsed_text().strip_edges() == "Literal **stars**");
	CHECK(user_bubble->get_h_size_flags() == Control::SIZE_EXPAND_FILL);
	CHECK(user_bubble->has_theme_stylebox_override(SceneStringName(panel)));
	Ref<StyleBoxFlat> user_style = user_bubble->get_theme_stylebox(SceneStringName(panel));
	REQUIRE(user_style.is_valid());
	CHECK_FALSE(user_style->is_anti_aliased());
	CHECK(user_style->get_border_width(SIDE_BOTTOM) >= 2);

	memdelete(assistant_bubble);
	memdelete(user_bubble);
}

TEST_CASE("[Editor][AI] Message bubbles ignore null content") {
	AIMessageBubble *bubble = memnew(AIMessageBubble);
	Dictionary message;
	message["role"] = "assistant";
	message["content"] = Variant();

	bubble->set_message(message);

	AIMarkdownLabel *label = find_child_of_type<AIMarkdownLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	CHECK(find_child_of_type<MarkdownViewer>(bubble) != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(bubble) == nullptr);
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

	AIMarkdownLabel *label = find_child_of_type<AIMarkdownLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	CHECK(find_child_of_type<MarkdownViewer>(bubble) != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(bubble) == nullptr);
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

	AIMarkdownLabel *label = find_child_of_type<AIMarkdownLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	LinkButton *details_button = find_child_of_type<LinkButton>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	REQUIRE(details_button != nullptr);
	CHECK(find_child_of_type<MarkdownViewer>(bubble) != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(bubble) == nullptr);

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

	AIMarkdownLabel *label = find_child_of_type<AIMarkdownLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	MarkdownViewer *viewer = find_child_of_type<MarkdownViewer>(bubble);
	REQUIRE(viewer != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(bubble) == nullptr);
	const String parsed_text = label->get_parsed_text();
	CHECK(title_label->get_text() == "Tool Call");
	CHECK(parsed_text.contains("project.read_file"));
	CHECK(parsed_text.contains("res://player.gd"));
	CHECK(!parsed_text.contains("Inspect a long script before suggesting edits"));

	LinkButton *details_button = find_child_of_type<LinkButton>(bubble);
	REQUIRE(details_button != nullptr);
	CHECK(details_button->is_visible());
	CHECK(bubble->get_h_size_flags() == Control::SIZE_EXPAND_FILL);
	CHECK(label->get_h_size_flags() == Control::SIZE_EXPAND_FILL);
	CHECK(viewer->get_h_size_flags() == Control::SIZE_EXPAND_FILL);
	CHECK(bubble->has_theme_stylebox_override(SceneStringName(panel)));

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

	AIMarkdownLabel *label = find_child_of_type<AIMarkdownLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	CHECK(find_child_of_type<MarkdownViewer>(bubble) != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(bubble) == nullptr);
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

	AIMarkdownLabel *label = find_child_of_type<AIMarkdownLabel>(bubble);
	Label *title_label = find_child_of_type<Label>(bubble);
	REQUIRE(label != nullptr);
	REQUIRE(title_label != nullptr);
	MarkdownViewer *viewer = find_child_of_type<MarkdownViewer>(bubble);
	REQUIRE(viewer != nullptr);
	CHECK(find_child_of_type<RichTextLabel>(bubble) == nullptr);
	const String collapsed_text = label->get_parsed_text();
	CHECK(title_label->get_text() == "Tool: project.read_file (completed)");
	CHECK(collapsed_text.contains("res://player.gd line 1: extends CharacterBody2D"));
	CHECK(!collapsed_text.contains("line 3: func _physics_process"));

	LinkButton *details_button = find_child_of_type<LinkButton>(bubble);
	REQUIRE(details_button != nullptr);
	CHECK(details_button->is_visible());
	CHECK(bubble->get_h_size_flags() == Control::SIZE_EXPAND_FILL);
	CHECK(label->get_h_size_flags() == Control::SIZE_EXPAND_FILL);
	CHECK(viewer->get_h_size_flags() == Control::SIZE_EXPAND_FILL);
	CHECK(bubble->has_theme_stylebox_override(SceneStringName(panel)));

	details_button->emit_signal(SceneStringName(pressed));
	CHECK(label->get_parsed_text().contains("line 3: func _physics_process"));
	CHECK(bubble->get_h_size_flags() == Control::SIZE_EXPAND_FILL);

	memdelete(bubble);
}

TEST_CASE("[Editor][AI] Message list keeps the last bubble visible after markdown relayout") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	AIMessageList *list = memnew(AIMessageList);
	list->set_size(Size2(420, 150));

	for (int i = 0; i < 3; i++) {
		Dictionary filler;
		filler["role"] = "assistant";
		filler["content"] = vformat("Filler message %d\n\n- one\n- two\n- three", i);
		list->add_message(filler);
	}

	Dictionary last_message;
	last_message["role"] = "assistant";
	last_message["content"] = "### Final answer\n\nThis line intentionally contains enough words to wrap several times once the AI dock becomes narrow, so the MarkdownViewer content height changes after the message list has already followed the bottom.\n\n```gdscript\nfunc _ready():\n\tprint(\"layout should remain visible\")\n```\n\n- final item one\n- final item two\n- final item three";
	list->add_message(last_message);
	list->scroll_to_bottom();

	root->add_child(list);
	process_scene_frames(6);

	list->set_size(Size2(220, 150));
	process_scene_frames(8);

	VScrollBar *scroll_bar = list->get_v_scroll_bar();
	REQUIRE(scroll_bar != nullptr);
	CHECK(scroll_bar->get_value() + scroll_bar->get_page() >= scroll_bar->get_max() - 2.0);

	AIMessageBubble *last_bubble = find_last_child_of_type<AIMessageBubble>(list);
	REQUIRE(last_bubble != nullptr);
	const real_t visible_bottom = get_scroll_viewport_bottom(list);
	const real_t bubble_bottom = last_bubble->get_global_position().y + last_bubble->get_size().y;
	CHECK(bubble_bottom <= doctest::Approx(visible_bottom).epsilon(0.01));
	CHECK(list->get_horizontal_scroll_mode() == ScrollContainer::SCROLL_MODE_DISABLED);

	root->remove_child(list);
	memdelete(list);
}

TEST_CASE("[Editor][AI] Message list keeps the last error bubble above the bottom edge") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	AIMessageList *list = memnew(AIMessageList);
	list->set_size(Size2(420, 150));

	for (int i = 0; i < 3; i++) {
		Dictionary filler;
		filler["role"] = "assistant";
		filler["content"] = vformat("Filler message %d\n\nThis makes the message list scroll before the final error arrives.", i);
		list->add_message(filler);
	}

	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = "Hello";
	list->add_message(user_message);

	Dictionary error_message;
	error_message["role"] = "error";
	error_message["content"] = "Provider request failed.";
	list->add_message(error_message);
	list->scroll_to_bottom();

	root->add_child(list);
	process_scene_frames(8);

	AIMessageBubble *last_bubble = find_last_child_of_type<AIMessageBubble>(list);
	REQUIRE(last_bubble != nullptr);
	const real_t visible_bottom = get_scroll_viewport_bottom(list);
	const real_t bubble_bottom = last_bubble->get_global_position().y + last_bubble->get_size().y;
	CHECK(bubble_bottom <= visible_bottom - 6.0);

	root->remove_child(list);
	memdelete(list);
}

TEST_CASE("[Editor][AI] Dock-like message list gives markdown viewer its full wrapped height") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	VBoxContainer *dock_body = memnew(VBoxContainer);
	dock_body->set_size(Size2(360, 480));
	dock_body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dock_body->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	dock_body->add_theme_constant_override("separation", 8);
	root->add_child(dock_body);

	AIMessageList *list = memnew(AIMessageList);
	dock_body->add_child(list);

	Label *tokens = memnew(Label);
	tokens->set_text("Tokens  In 389.7k  Out 12.5k  Total 402.2k  Context ~399.4k");
	tokens->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dock_body->add_child(tokens);

	TextEdit *input = memnew(TextEdit);
	input->set_custom_minimum_size(Size2(0, 118));
	input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dock_body->add_child(input);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dock_body->add_child(toolbar);
	Control *toolbar_fill = memnew(Control);
	toolbar_fill->set_custom_minimum_size(Size2(0, 44));
	toolbar_fill->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	toolbar->add_child(toolbar_fill);

	Dictionary message;
	message["role"] = "assistant";
	message["content"] = "Hello! I am here. The previous connection issue should not affect this conversation.\n\nWe already made a complete snake game. If you want:\n\n- add more features to snake, such as speed changes, sound effects, levels, score rankings\n- build a 2D or 3D scene in Godot from the art reference\n- or make any other game, tool, or asset-related development";
	list->add_message(message);
	list->scroll_to_bottom();
	process_scene_frames(8);

	AIMessageBubble *last_bubble = find_last_child_of_type<AIMessageBubble>(list);
	REQUIRE(last_bubble != nullptr);
	MarkdownViewer *viewer = find_last_child_of_type<MarkdownViewer>(last_bubble);
	REQUIRE(viewer != nullptr);

	CHECK(viewer->get_content_height() <= doctest::Approx(viewer->get_size().y).epsilon(0.01));

	const real_t visible_bottom = get_scroll_viewport_bottom(list);
	const real_t bubble_bottom = last_bubble->get_global_position().y + last_bubble->get_size().y;
	CHECK(bubble_bottom <= doctest::Approx(visible_bottom).epsilon(0.01));

	root->remove_child(dock_body);
	memdelete(dock_body);
}

TEST_CASE("[Editor][AI] Message list leaves visual clearance below the last bubble") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	AIMessageList *list = memnew(AIMessageList);
	list->set_size(Size2(360, 180));
	root->add_child(list);

	for (int i = 0; i < 4; i++) {
		Dictionary filler;
		filler["role"] = "assistant";
		filler["content"] = vformat("Filler message %d\n\nThis creates enough history for the list to scroll.", i);
		list->add_message(filler);
	}

	Dictionary message;
	message["role"] = "assistant";
	message["content"] = "Final answer\n\n- The bottom border should remain visibly separated from the message list clipping edge.\n- This reproduces the narrow AI dock layout after markdown wrapping.";
	list->add_message(message);
	list->scroll_to_bottom();
	process_scene_frames(12);

	AIMessageBubble *last_bubble = find_last_child_of_type<AIMessageBubble>(list);
	REQUIRE(last_bubble != nullptr);
	const real_t visible_bottom = get_scroll_viewport_bottom(list);
	const real_t bubble_bottom = last_bubble->get_global_position().y + last_bubble->get_size().y;
	CHECK(bubble_bottom <= visible_bottom - 8.0);

	root->remove_child(list);
	memdelete(list);
}

TEST_CASE("[Editor][AI] Message list scrolls to the bottom of a tall final bubble") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	AIMessageList *list = memnew(AIMessageList);
	list->set_size(Size2(360, 180));
	root->add_child(list);

	Dictionary message;
	message["role"] = "assistant";
	message["content"] = "Assistant response\n\nThis final message is intentionally taller than the message list viewport after wrapping.\n\n- The first item explains a longer idea that wraps across multiple visual lines in a narrow AI dock.\n- The second item also wraps and keeps adding vertical height to the same final bubble.\n- The third item makes sure the bottom of this final bubble is the important visible edge.\n- The fourth item is here to force the final bubble below the viewport.\n- The fifth item should still be reachable when the list follows the bottom.\n\nThe visible region should end at the bottom border of this bubble, not at its title.";
	list->add_message(message);
	list->scroll_to_bottom();
	process_scene_frames(10);

	AIMessageBubble *last_bubble = find_last_child_of_type<AIMessageBubble>(list);
	REQUIRE(last_bubble != nullptr);
	REQUIRE(last_bubble->get_size().y > list->get_size().y);
	const real_t visible_bottom = get_scroll_viewport_bottom(list);
	const real_t bubble_bottom = last_bubble->get_global_position().y + last_bubble->get_size().y;
	CHECK(bubble_bottom <= doctest::Approx(visible_bottom).epsilon(0.01));
	CHECK(last_bubble->get_global_position().y < list->get_global_position().y);

	root->remove_child(list);
	memdelete(list);
}

TEST_CASE("[Editor][AI] Markdown label minimum height follows its assigned width") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	AIMarkdownLabel *label = memnew(AIMarkdownLabel);
	label->set_markdown("你好呀！我在这里。之前我们已经做好了一个完整的贪吃蛇游戏，如果你想：\n\n- 给贪吃蛇加更多功能（比如加速/减速音效、关卡、得分排行榜）\n- 基于刚才看到的那幅艺术家阁楼插画，在 Godot 里搭建一个 2D/3D 场景\n- 或者做其他任何游戏、工具、资源相关的开发");
	root->add_child(label);

	label->set_size(Size2(760, 120));
	process_scene_frames(4);
	const real_t wide_height = label->get_combined_minimum_size().y;

	label->set_size(Size2(260, 120));
	process_scene_frames(4);
	const real_t narrow_height = label->get_combined_minimum_size().y;

	CHECK(narrow_height > wide_height);

	root->remove_child(label);
	memdelete(label);
}

TEST_CASE("[Editor][AI] Dock-like message list keeps bottom visible at wide and narrow widths") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	const real_t widths[] = { 320.0, 760.0 };
	for (real_t width : widths) {
		VBoxContainer *dock_body = memnew(VBoxContainer);
		dock_body->set_size(Size2(width, 480));
		dock_body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		dock_body->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		dock_body->add_theme_constant_override("separation", 8);
		root->add_child(dock_body);

		AIMessageList *list = memnew(AIMessageList);
		dock_body->add_child(list);

		Label *tokens = memnew(Label);
		tokens->set_text("Tokens  In 389.7k  Out 12.5k  Total 402.2k  Context ~399.4k");
		tokens->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		dock_body->add_child(tokens);

		TextEdit *input = memnew(TextEdit);
		input->set_custom_minimum_size(Size2(0, 118));
		input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		dock_body->add_child(input);

		HBoxContainer *toolbar = memnew(HBoxContainer);
		toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		dock_body->add_child(toolbar);
		Control *toolbar_fill = memnew(Control);
		toolbar_fill->set_custom_minimum_size(Size2(0, 44));
		toolbar_fill->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		toolbar->add_child(toolbar_fill);

		Dictionary user_message;
		user_message["role"] = "user";
		user_message["content"] = "你好";
		list->add_message(user_message);

		Dictionary message;
		message["role"] = "assistant";
		message["content"] = "你好呀！😊 我在这里。刚才的连接小问题不影响我们交流。\n\n之前我们已经做好了一个完整的贪吃蛇游戏，如果你想：\n\n- 给贪吃蛇加更多功能（比如加速/减速音效、关卡、得分排行榜）\n- 基于刚才看到的那幅艺术家阁楼插画，在 Godot 里搭建一个 2D/3D 场景\n- 或者做其他任何游戏、工具、资源相关的开发";
		list->add_message(message);
		list->scroll_to_bottom();
		process_scene_frames(12);

		AIMessageBubble *last_bubble = find_last_child_of_type<AIMessageBubble>(list);
		REQUIRE(last_bubble != nullptr);
		const real_t visible_bottom = get_scroll_viewport_bottom(list);
		const real_t bubble_bottom = last_bubble->get_global_position().y + last_bubble->get_size().y;
		CHECK(bubble_bottom <= visible_bottom - 6.0);

		root->remove_child(dock_body);
		memdelete(dock_body);
	}
}

} // namespace TestAIModelSettings
