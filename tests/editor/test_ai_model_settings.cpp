/**************************************************************************/
/*  test_ai_model_settings.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/version.h"
#include "editor/agent_ui/ai_composer.h"
#include "editor/agent_ui/ai_settings_about_page.h"
#include "editor/agent_ui/ai_settings_mcp_page.h"
#include "editor/agent_ui/ai_settings_models_page.h"
#include "editor/agent_ui/ai_settings_rules_page.h"
#include "editor/agent_ui/ai_settings_skills_page.h"
#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/tools/editor/ai_change_set_store.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"
#include "editor/next_engine_update_checker.h"
#include "scene/main/http_request.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_ai_model_settings);

namespace TestAIAgentV1Settings {

Ref<AIAgentV1UIBridge> setup_bridge_for_test() {
	AIAgentV1UIBridge::clear_singleton_for_test();

	Ref<AIConfigService> config;
	config.instantiate();

	Dictionary override_config;
	override_config["default_provider"] = "fake";
	override_config["default_model"] = "fake-model";
	override_config["providers"] = Dictionary();
	config->set_runtime_override(override_config);

	Ref<AIAgentV1UIBridge> bridge = AIAgentV1UIBridge::get_singleton();
	bridge->set_config_service(config);
	return bridge;
}

Dictionary find_model_profile(const Array &p_profiles, const String &p_provider_id, const String &p_model) {
	for (int i = 0; i < p_profiles.size(); i++) {
		if (p_profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary profile = p_profiles[i];
		if (String(profile.get("provider_id", String())) == p_provider_id && String(profile.get("model", String())) == p_model) {
			return profile;
		}
	}
	return Dictionary();
}

Array array_from_snapshot(const Ref<AIAgentV1UIBridge> &p_bridge, const String &p_key) {
	Dictionary snapshot = p_bridge->get_settings_snapshot();
	Variant value = snapshot.get(p_key, Array());
	return value.get_type() == Variant::ARRAY ? Array(value) : Array();
}

PackedByteArray utf8_body(const String &p_text) {
	return p_text.to_utf8_buffer();
}

String find_repo_file(const String &p_relative_path) {
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	REQUIRE(dir.is_valid());

	String current_dir = dir->get_current_dir();
	for (int i = 0; i < 8; i++) {
		const String candidate = current_dir.path_join(p_relative_path);
		if (FileAccess::exists(candidate)) {
			return candidate;
		}

		const String parent = current_dir.get_base_dir();
		if (parent == current_dir) {
			break;
		}
		current_dir = parent;
	}

	REQUIRE_MESSAGE(false, vformat("Could not locate repository file: %s", p_relative_path));
	return String();
}

String read_repo_file(const String &p_relative_path) {
	const String path = find_repo_file(p_relative_path);
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	REQUIRE_MESSAGE(file.is_valid(), vformat("Could not read repository file: %s", path));
	REQUIRE(err == OK);
	return file->get_as_text();
}

} // namespace TestAIAgentV1Settings

TEST_CASE("[Editor][AgentV1] Settings model page writes model profiles through bridge") {
	using namespace TestAIAgentV1Settings;

	Ref<AIAgentV1UIBridge> bridge = setup_bridge_for_test();
	AISettingsModelsPage *page = memnew(AISettingsModelsPage);
	page->build_for_test();

	page->add_provider_model_for_test("openai", "gpt-5.4", "test-key");

	Array profiles = bridge->list_model_profiles(false);
	Dictionary profile = find_model_profile(profiles, "openai", "gpt-5.4");
	CHECK_FALSE(profile.is_empty());
	CHECK(String(profile.get("api_key", String())) == "test-key");
	CHECK(page->get_model_table_row_count_for_test() == 1);

	memdelete(page);
	AIAgentV1UIBridge::clear_singleton_for_test();
}

TEST_CASE("[Editor][AgentV1] Settings MCP page writes MCP servers through bridge") {
	using namespace TestAIAgentV1Settings;

	Ref<AIAgentV1UIBridge> bridge = setup_bridge_for_test();
	AISettingsMCPPage *page = memnew(AISettingsMCPPage);
	page->build_for_test();

	page->add_server_for_test("Filesystem", "npx", true);

	Array servers = array_from_snapshot(bridge, "mcp_servers");
	REQUIRE(servers.size() == 1);
	Dictionary server = servers[0];
	CHECK(String(server.get("name", String())) == "Filesystem");
	CHECK(String(server.get("command", String())) == "npx");
	CHECK(bool(server.get("enabled", false)));
	CHECK(page->get_server_table_row_count_for_test() == 1);

	memdelete(page);
	AIAgentV1UIBridge::clear_singleton_for_test();
}

TEST_CASE("[Editor][AgentV1] Settings skills and rules pages patch agent_v1 config") {
	using namespace TestAIAgentV1Settings;

	Ref<AIAgentV1UIBridge> bridge = setup_bridge_for_test();
	AISettingsSkillsPage *skills_page = memnew(AISettingsSkillsPage);
	AISettingsRulesPage *rules_page = memnew(AISettingsRulesPage);
	skills_page->build_for_test();
	rules_page->build_for_test();

	skills_page->add_skill_for_test("Local Skills", "F:/agent-skills", "# Skill", true);
	rules_page->add_rule_for_test("project.read", true);

	Array skills = array_from_snapshot(bridge, "skills");
	Array rules = array_from_snapshot(bridge, "rules");
	CHECK(skills_page->get_skill_table_row_count_for_test() == 1);
	CHECK(rules_page->get_rule_table_row_count_for_test() == 1);
	REQUIRE(skills.size() == 1);
	REQUIRE(rules.size() == 1);
	CHECK(String(Dictionary(skills[0]).get("source", String())) == "F:/agent-skills");
	CHECK(String(Dictionary(rules[0]).get("action", String())) == "project.read");

	memdelete(skills_page);
	memdelete(rules_page);
	AIAgentV1UIBridge::clear_singleton_for_test();
}

TEST_CASE("[Editor][AgentV1] NEXT update checker parses GitHub latest release") {
	using namespace TestAIAgentV1Settings;

	NextEngineUpdateCheckResult update = parse_next_engine_update_response(HTTPRequest::RESULT_SUCCESS, 200, utf8_body("{\"tag_name\":\"v9.0.4.7.1-preview\"}"), "0.0.4.7.1-preview");
	CHECK(update.status == NextEngineUpdateCheckStatus::UPDATE_AVAILABLE);
	CHECK(update.latest_version == "v9.0.4.7.1-preview");

	NextEngineUpdateCheckResult current = parse_next_engine_update_response(HTTPRequest::RESULT_SUCCESS, 200, utf8_body("{\"tag_name\":\"v0.0.4.7.1-preview\"}"), "0.0.4.7.1-preview");
	CHECK(current.status == NextEngineUpdateCheckStatus::UP_TO_DATE);

	NextEngineUpdateCheckResult invalid = parse_next_engine_update_response(HTTPRequest::RESULT_SUCCESS, 200, utf8_body("{\"tag_name\":\"not-a-version\"}"), "0.0.4.7.1-preview");
	CHECK(invalid.status == NextEngineUpdateCheckStatus::ERROR);
}

TEST_CASE("[Editor][AgentV1] Settings about page shows manual update results") {
	using namespace TestAIAgentV1Settings;

	AISettingsAboutPage *page = memnew(AISettingsAboutPage);
	page->build_for_test();

	CHECK(page->get_current_version_text_for_test().contains(NEXT_VERSION_FULL_CONFIG));
	CHECK_FALSE(page->is_download_button_visible_for_test());

	page->apply_update_response_for_test(HTTPRequest::RESULT_SUCCESS, 200, "{\"tag_name\":\"v9.0.4.7.1-preview\"}", "0.0.4.7.1-preview");
	CHECK(page->get_update_status_for_test() == NextEngineUpdateCheckStatus::UPDATE_AVAILABLE);
	CHECK(page->get_latest_version_text_for_test().contains("v9.0.4.7.1-preview"));
	CHECK(page->is_download_button_visible_for_test());

	page->apply_update_response_for_test(HTTPRequest::RESULT_SUCCESS, 200, "{\"tag_name\":\"v0.0.4.7.1-preview\"}", "0.0.4.7.1-preview");
	CHECK(page->get_update_status_for_test() == NextEngineUpdateCheckStatus::UP_TO_DATE);
	CHECK_FALSE(page->is_download_button_visible_for_test());

	memdelete(page);
}

TEST_CASE("[Editor][AgentV1] Settings dialog source registers about navigation page") {
	using namespace TestAIAgentV1Settings;

	const String dialog_header = read_repo_file("editor/agent_ui/ai_agent_settings_dialog.h");
	const String dialog_source = read_repo_file("editor/agent_ui/ai_agent_settings_dialog.cpp");

	CHECK(dialog_header.contains("PAGE_ABOUT"));
	CHECK(dialog_header.contains("AISettingsAboutPage *about_page"));
	CHECK(dialog_source.contains("navigation->add_item(TTR(\"About\"), get_editor_theme_icon(SNAME(\"Info\")))"));
	CHECK(dialog_source.contains("navigation->set_item_metadata(PAGE_ABOUT, PAGE_ABOUT)"));
	CHECK(dialog_source.contains("about_page = memnew(AISettingsAboutPage)"));
	CHECK(dialog_source.contains("pages->add_child(about_page)"));
}

TEST_CASE("[Editor][AgentV1] Composer model selector uses agent_v1 model profiles") {
	using namespace TestAIAgentV1Settings;

	Ref<AIAgentV1UIBridge> bridge = setup_bridge_for_test();
	Dictionary profile;
	profile["provider_id"] = "openai";
	profile["model"] = "gpt-5.4";
	profile["api_key"] = "test-key";
	Dictionary result = bridge->add_model_profile(profile, "runtime");
	REQUIRE(bool(result.get("success", false)));
	const String profile_id = String(result.get("id", String()));

	AIComposer *composer = memnew(AIComposer);
	composer->reload_models();

	CHECK(composer->get_selected_model() == profile_id);

	memdelete(composer);
	AIAgentV1UIBridge::clear_singleton_for_test();
}

TEST_CASE("[Editor][AgentV1] UI bridge exposes change review operations without owning agent core") {
	using namespace TestAIAgentV1Settings;

	Ref<AIAgentV1UIBridge> bridge = setup_bridge_for_test();
	Ref<AIChangeSetStore> store = AIChangeSetStore::get_singleton();
	store->clear_for_test();

	Dictionary change;
	change["path"] = "res://codex_test_change.gd";
	change["type"] = "edit";
	change["old_text"] = "print(\"old\")\n";
	change["new_text"] = "print(\"new\")\n";
	change["diff"] = "--- res://codex_test_change.gd\n+++ res://codex_test_change.gd\n@@ change @@\n-print(\"old\")\n+print(\"new\")\n";
	change["added_lines"] = 1;
	change["removed_lines"] = 1;

	Array changes;
	changes.push_back(change);
	const String change_set_id = store->add_change_set("Test change", "session-1", "tool-call-1", changes);
	REQUIRE_FALSE(change_set_id.is_empty());

	CHECK(bridge->get_pending_change_set_count() == 1);
	Array pending = bridge->list_change_sets("pending");
	REQUIRE(pending.size() == 1);
	CHECK(String(Dictionary(pending[0]).get("id", String())) == change_set_id);

	Dictionary keep_result = bridge->keep_change_set(change_set_id);
	CHECK(bool(keep_result.get("success", false)));
	CHECK(bridge->get_pending_change_set_count() == 0);

	store->clear_for_test();
	AIAgentV1UIBridge::clear_singleton_for_test();
}
