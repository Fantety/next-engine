/**************************************************************************/
/*  test_ai_custom_instructions.cpp                                       */
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

#include "editor/agent_ui/ai_settings_custom_instructions_page.h"
#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"
#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_config_adapter.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_ai_custom_instructions);

namespace TestAICustomInstructions {

Ref<AIConfigService> make_config_with_base_prompt_and_rules() {
	Ref<AIConfigService> config;
	config.instantiate();

	Array main_system;
	main_system.push_back("Base system prompt.");
	main_system.push_back("Base project workflow.");
	Dictionary main_agent;
	main_agent["system"] = main_system;
	main_agent["custom_instructions"] = "Use concise project-specific answers.\n\nPrefer GDScript examples.";

	Dictionary agents;
	agents["main"] = main_agent;

	Dictionary allow_rule;
	allow_rule["action"] = "project.read";
	allow_rule["resource"] = "*";
	allow_rule["effect"] = "allow";
	Array rules;
	rules.push_back(allow_rule);
	Dictionary permissions;
	permissions["rules"] = rules;

	Dictionary override_config;
	override_config["agents"] = agents;
	override_config["permissions"] = permissions;
	config->set_runtime_override(override_config);
	return config;
}

void check_base_prompt_with_custom_instructions(const Ref<AIConfigService> &p_config, const String &p_custom_instructions) {
	const Array system_prompt = p_config->get_system_prompt("main");
	REQUIRE(system_prompt.size() >= 3);
	CHECK(String(system_prompt[0]) == "Base system prompt.");
	CHECK(String(system_prompt[1]) == "Base project workflow.");
	CHECK(String(system_prompt[2]) == p_custom_instructions);
}

TEST_CASE("[Editor][AgentV1][UIAdapter] Custom instructions are exposed separately from permission rules") {
	Ref<AIConfigService> config = make_config_with_base_prompt_and_rules();

	Ref<AIAgentV1UIConfigAdapter> adapter;
	adapter.instantiate();
	adapter->set_config_service(config);

	const Dictionary snapshot = adapter->get_settings_snapshot();
	CHECK(String(snapshot.get("custom_instructions", String())) == "Use concise project-specific answers.\n\nPrefer GDScript examples.");
	CHECK(Array(snapshot.get("rules", Array())).size() == 1);

	Dictionary main_patch;
	main_patch["custom_instructions"] = "Always ask before changing scenes.";
	Dictionary agents_patch;
	agents_patch["main"] = main_patch;
	Dictionary patch;
	patch["agents"] = agents_patch;
	const Dictionary patched = adapter->patch_settings(patch, "runtime");
	REQUIRE(bool(patched.get("success", false)));

	const Dictionary updated_snapshot = adapter->get_settings_snapshot();
	CHECK(String(updated_snapshot.get("custom_instructions", String())) == "Always ask before changing scenes.");
	check_base_prompt_with_custom_instructions(config, "Always ask before changing scenes.");
	const Array updated_rules = updated_snapshot.get("rules", Array());
	REQUIRE(updated_rules.size() == 1);
	CHECK(String(Dictionary(updated_rules[0]).get("action", String())) == "project.read");
}

TEST_CASE("[Editor][AgentV1] Custom instructions settings page writes append-only instructions") {
	AIAgentV1UIBridge::clear_singleton_for_test();
	Ref<AIConfigService> config = make_config_with_base_prompt_and_rules();
	Ref<AIAgentV1UIBridge> bridge = AIAgentV1UIBridge::get_singleton();
	bridge->set_config_service(config);

	AISettingsCustomInstructionsPage *page = memnew(AISettingsCustomInstructionsPage);
	page->build_for_test();
	page->set_custom_instructions_for_test("Use short bullet lists.");

	CHECK(page->get_custom_instructions_for_test() == "Use short bullet lists.");
	check_base_prompt_with_custom_instructions(config, "Use short bullet lists.");
	const Array rules = bridge->get_settings_snapshot().get("rules", Array());
	REQUIRE(rules.size() == 1);
	CHECK(String(Dictionary(rules[0]).get("action", String())) == "project.read");

	memdelete(page);
	AIAgentV1UIBridge::clear_singleton_for_test();
}

} // namespace TestAICustomInstructions
