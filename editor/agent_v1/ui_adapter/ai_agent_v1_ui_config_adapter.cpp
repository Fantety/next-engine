/**************************************************************************/
/*  ai_agent_v1_ui_config_adapter.cpp                                     */
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

#include "ai_agent_v1_ui_config_adapter.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/agent_v1/core/base/ai_id.h"
#include "editor/agent_v1/domain/model/ai_model_catalog.h"

namespace {

const int DEFAULT_MAX_OUTPUT_TOKENS = 0;
const int DEFAULT_TIMEOUT_MSEC = 120000;

String _make_profile_display_name(const String &p_provider_name, const String &p_model) {
	if (!p_provider_name.is_empty()) {
		return p_provider_name + " / " + p_model;
	}
	return p_model;
}

Dictionary _make_error_result(const String &p_message, const Dictionary &p_details = Dictionary()) {
	Dictionary error;
	error["kind"] = "validation";
	error["message"] = p_message;
	error["details"] = p_details.duplicate(true);

	Dictionary result;
	result["success"] = false;
	result["error"] = error;
	return result;
}

int _model_profile_int(const Dictionary &p_profile, const String &p_key, int p_default_value, int p_min_value) {
	return MAX(p_min_value, int(p_profile.get(p_key, p_default_value)));
}

int _model_profile_timeout_msec(const Dictionary &p_profile) {
	if (p_profile.has("timeout_msec")) {
		return _model_profile_int(p_profile, "timeout_msec", DEFAULT_TIMEOUT_MSEC, 1);
	}
	if (p_profile.has("timeout_seconds")) {
		return MAX(1, int(p_profile.get("timeout_seconds", DEFAULT_TIMEOUT_MSEC / 1000))) * 1000;
	}
	return DEFAULT_TIMEOUT_MSEC;
}

void _copy_runtime_options_to_profile(Dictionary &r_profile, const Dictionary &p_provider) {
	r_profile["supports_multimodal"] = bool(p_provider.get("supports_multimodal", false));
	r_profile["max_output_tokens"] = _model_profile_int(p_provider, "max_output_tokens", DEFAULT_MAX_OUTPUT_TOKENS, 0);
	r_profile["timeout_msec"] = _model_profile_timeout_msec(p_provider);
}

void _copy_runtime_options_to_provider(Dictionary &r_provider, const Dictionary &p_profile) {
	r_provider["supports_multimodal"] = bool(p_profile.get("supports_multimodal", false));
	r_provider["max_output_tokens"] = _model_profile_int(p_profile, "max_output_tokens", DEFAULT_MAX_OUTPUT_TOKENS, 0);
	r_provider["timeout_msec"] = _model_profile_timeout_msec(p_profile);
}

Dictionary _model_profile_from_provider_config(const String &p_provider_key, const Dictionary &p_provider_config) {
	const String provider_id = String(p_provider_config.get("provider_id", p_provider_config.get("provider", p_provider_key))).strip_edges();
	const Dictionary preset = AIModelCatalog::get_provider_preset(provider_id);
	const String provider_name = String(p_provider_config.get("provider_name", preset.get("display_name", provider_id))).strip_edges();
	const String model = String(p_provider_config.get("model", String())).strip_edges();
	const String display_name = String(p_provider_config.get("display_name", String())).strip_edges();

	Dictionary profile;
	profile["id"] = String(p_provider_config.get("id", p_provider_key)).strip_edges();
	profile["provider_key"] = p_provider_key;
	profile["provider"] = p_provider_key;
	profile["provider_id"] = provider_id;
	profile["provider_name"] = provider_name;
	profile["display_name"] = display_name.is_empty() ? _make_profile_display_name(provider_name, model) : display_name;
	profile["model"] = model;
	profile["type"] = String(p_provider_config.get("type", preset.get("type", "openai-compatible")));
	profile["base_url"] = String(p_provider_config.get("base_url", p_provider_config.get("url", preset.get("default_base_url", String())))).strip_edges();
	profile["api_key"] = String(p_provider_config.get("api_key", String())).strip_edges();
	profile["enabled"] = bool(p_provider_config.get("enabled", true));
	profile["custom"] = bool(p_provider_config.get("custom", false));
	_copy_runtime_options_to_profile(profile, p_provider_config);
	return profile;
}

Dictionary _provider_config_from_model_profile(const Dictionary &p_profile) {
	const String provider_id = String(p_profile.get("provider_id", p_profile.get("provider", String()))).strip_edges();
	const Dictionary preset = AIModelCatalog::get_provider_preset(provider_id);
	const String provider_name = String(p_profile.get("provider_name", preset.get("display_name", provider_id))).strip_edges();
	const String model = String(p_profile.get("model", String())).strip_edges();
	const String display_name = String(p_profile.get("display_name", String())).strip_edges();

	Dictionary provider;
	provider["id"] = String(p_profile.get("id", String())).strip_edges();
	provider["type"] = String(p_profile.get("type", preset.get("type", "openai-compatible")));
	provider["provider_id"] = provider_id;
	provider["provider_name"] = provider_name;
	provider["display_name"] = display_name.is_empty() ? _make_profile_display_name(provider_name, model) : display_name;
	provider["model"] = model;
	provider["base_url"] = String(p_profile.get("base_url", preset.get("default_base_url", String()))).strip_edges();
	provider["api_key"] = String(p_profile.get("api_key", String())).strip_edges();
	provider["enabled"] = bool(p_profile.get("enabled", true));
	provider["custom"] = bool(p_profile.get("custom", false));
	provider["ui_model_profile"] = true;
	_copy_runtime_options_to_provider(provider, p_profile);
	return provider;
}

void _mark_legacy_model_profile_fields_for_deletion(Dictionary &r_provider_patch) {
	r_provider_patch["api_format"] = Variant();
	r_provider_patch["max_input_chars"] = Variant();
	r_provider_patch["max_context_chars"] = Variant();
	r_provider_patch["max_history_chars"] = Variant();
	r_provider_patch["max_tool_result_chars"] = Variant();
	r_provider_patch["max_multimodal_files"] = Variant();
	r_provider_patch["max_multimodal_file_bytes"] = Variant();
	r_provider_patch["min_recent_messages"] = Variant();
	r_provider_patch["max_provider_turns"] = Variant();
	r_provider_patch["max_tool_calls"] = Variant();
	r_provider_patch["timeout_seconds"] = Variant();
}

Dictionary _merge_model_profile(const Dictionary &p_existing, const Dictionary &p_patch) {
	Dictionary merged = p_existing.duplicate(true);
	for (const KeyValue<Variant, Variant> &kv : p_patch) {
		merged[kv.key] = kv.value;
	}
	return merged;
}

Dictionary _find_replacement_model_profile(const Array &p_profiles, const String &p_excluded_profile_id) {
	for (int i = 0; i < p_profiles.size(); i++) {
		if (p_profiles[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary candidate = p_profiles[i];
		if (!bool(candidate.get("enabled", true))) {
			continue;
		}
		if (String(candidate.get("id", String())) != p_excluded_profile_id) {
			return candidate;
		}
	}
	return Dictionary();
}

void _apply_replacement_model_profile(Dictionary &r_patch, bool p_replace_default, bool p_replace_main, const Dictionary &p_replacement) {
	const String replacement_provider = p_replacement.is_empty() ? String("fake") : String(p_replacement.get("provider_key", p_replacement.get("id", String("fake")))).strip_edges();
	const String replacement_model = p_replacement.is_empty() ? String("fake-model") : String(p_replacement.get("model", String("fake-model"))).strip_edges();
	if (p_replace_default) {
		r_patch["default_provider"] = replacement_provider;
		r_patch["default_model"] = replacement_model;
	}
	if (p_replace_main) {
		Dictionary main_patch;
		main_patch["provider"] = replacement_provider;
		main_patch["model"] = replacement_model;
		Dictionary agents_patch;
		agents_patch["main"] = main_patch;
		r_patch["agents"] = agents_patch;
	}
}

} // namespace

void AIAgentV1UIConfigAdapter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_config_service", "service"), &AIAgentV1UIConfigAdapter::set_config_service);
	ClassDB::bind_method(D_METHOD("get_config_service"), &AIAgentV1UIConfigAdapter::get_config_service);
	ClassDB::bind_method(D_METHOD("set_mcp_service", "service"), &AIAgentV1UIConfigAdapter::set_mcp_service);
	ClassDB::bind_method(D_METHOD("get_mcp_service"), &AIAgentV1UIConfigAdapter::get_mcp_service);
	ClassDB::bind_method(D_METHOD("set_skill_service", "service"), &AIAgentV1UIConfigAdapter::set_skill_service);
	ClassDB::bind_method(D_METHOD("get_skill_service"), &AIAgentV1UIConfigAdapter::get_skill_service);
	ClassDB::bind_method(D_METHOD("set_agent_service", "service"), &AIAgentV1UIConfigAdapter::set_agent_service);
	ClassDB::bind_method(D_METHOD("get_agent_service"), &AIAgentV1UIConfigAdapter::get_agent_service);
	ClassDB::bind_method(D_METHOD("get_settings_snapshot"), &AIAgentV1UIConfigAdapter::get_settings_snapshot);
	ClassDB::bind_method(D_METHOD("list_models"), &AIAgentV1UIConfigAdapter::list_models);
	ClassDB::bind_method(D_METHOD("list_model_provider_presets"), &AIAgentV1UIConfigAdapter::list_model_provider_presets);
	ClassDB::bind_method(D_METHOD("list_model_profiles", "enabled_only"), &AIAgentV1UIConfigAdapter::list_model_profiles, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("get_model_profile", "profile_id"), &AIAgentV1UIConfigAdapter::get_model_profile);
	ClassDB::bind_method(D_METHOD("add_model_profile", "profile", "scope"), &AIAgentV1UIConfigAdapter::add_model_profile, DEFVAL("project"));
	ClassDB::bind_method(D_METHOD("update_model_profile", "profile_id", "profile", "scope"), &AIAgentV1UIConfigAdapter::update_model_profile, DEFVAL("project"));
	ClassDB::bind_method(D_METHOD("remove_model_profile", "profile_id", "scope"), &AIAgentV1UIConfigAdapter::remove_model_profile, DEFVAL("project"));
	ClassDB::bind_method(D_METHOD("list_agents"), &AIAgentV1UIConfigAdapter::list_agents);
	ClassDB::bind_method(D_METHOD("patch_settings", "patch", "scope"), &AIAgentV1UIConfigAdapter::patch_settings, DEFVAL("project"));

	ADD_SIGNAL(MethodInfo("config_changed", PropertyInfo(Variant::STRING, "scope"), PropertyInfo(Variant::DICTIONARY, "config")));
	ADD_SIGNAL(MethodInfo("models_changed", PropertyInfo(Variant::ARRAY, "models")));
	ADD_SIGNAL(MethodInfo("mcp_status_changed", PropertyInfo(Variant::ARRAY, "statuses"), PropertyInfo(Variant::DICTIONARY, "summary")));
	ADD_SIGNAL(MethodInfo("skill_status_changed", PropertyInfo(Variant::ARRAY, "statuses"), PropertyInfo(Variant::DICTIONARY, "summary")));
	ADD_SIGNAL(MethodInfo("rules_changed", PropertyInfo(Variant::ARRAY, "rules")));
	ADD_SIGNAL(MethodInfo("marquee_changed", PropertyInfo(Variant::ARRAY, "marquees"), PropertyInfo(Variant::STRING, "active_id")));
	ADD_SIGNAL(MethodInfo("error_reported", PropertyInfo(Variant::DICTIONARY, "error")));
}

AIAgentV1UIConfigAdapter::AIAgentV1UIConfigAdapter() {
	_ensure_defaults();
}

void AIAgentV1UIConfigAdapter::_ensure_defaults() {
	if (config_service.is_null()) {
		config_service.instantiate();
	}
	if (agent_service.is_null()) {
		agent_service.instantiate();
	}
	if (mcp_service.is_null()) {
		mcp_service.instantiate();
	}
	if (skill_service.is_null()) {
		skill_service.instantiate();
	}
	if (agent_service.is_valid()) {
		agent_service->set_config_service(config_service);
	}
	_wire_service_signals();
}

void AIAgentV1UIConfigAdapter::_wire_service_signals() {
	if (config_service.is_valid()) {
		const Callable config_changed = callable_mp(this, &AIAgentV1UIConfigAdapter::_config_changed);
		if (!config_service->is_connected(SNAME("config_changed"), config_changed)) {
			config_service->connect(SNAME("config_changed"), config_changed);
		}
	}
	if (mcp_service.is_valid()) {
		const Callable status_changed = callable_mp(this, &AIAgentV1UIConfigAdapter::_mcp_status_changed);
		if (!mcp_service->is_connected(SNAME("status_changed"), status_changed)) {
			mcp_service->connect(SNAME("status_changed"), status_changed);
		}
	}
	if (skill_service.is_valid()) {
		const Callable skills_changed = callable_mp(this, &AIAgentV1UIConfigAdapter::_skill_tools_changed);
		if (!skill_service->is_connected(SNAME("skills_changed"), skills_changed)) {
			skill_service->connect(SNAME("skills_changed"), skills_changed);
		}
	}
}

Dictionary AIAgentV1UIConfigAdapter::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Array AIAgentV1UIConfigAdapter::_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

Array AIAgentV1UIConfigAdapter::_models_from_config(const Dictionary &p_config) const {
	Array models;
	const Dictionary providers = _dictionary_from_variant(p_config.get("providers", Dictionary()));
	const String default_provider = String(p_config.get("default_provider", String()));
	const String default_model = String(p_config.get("default_model", String()));
	for (const Variant *key = providers.next(nullptr); key; key = providers.next(key)) {
		const String provider_id = String(*key);
		const Dictionary provider = _dictionary_from_variant(providers[*key]);
		Dictionary item;
		item["id"] = provider_id;
		item["provider"] = provider_id;
		item["type"] = provider.get("type", String());
		item["model"] = provider.get("model", String());
		item["label"] = provider_id + "/" + String(item["model"]);
		item["default"] = provider_id == default_provider && String(item["model"]) == default_model;
		item["metadata"] = provider.duplicate(true);
		models.push_back(item);
	}
	return models;
}

Array AIAgentV1UIConfigAdapter::_model_profiles_from_config(const Dictionary &p_config, bool p_enabled_only) const {
	Array profiles;
	const Dictionary providers = _dictionary_from_variant(p_config.get("providers", Dictionary()));
	for (const Variant *key = providers.next(nullptr); key; key = providers.next(key)) {
		const String provider_key = String(*key);
		const Dictionary provider = _dictionary_from_variant(providers[*key]);
		if (!bool(provider.get("ui_model_profile", false))) {
			continue;
		}

		Dictionary profile = _model_profile_from_provider_config(provider_key, provider);
		if (String(profile.get("id", String())).is_empty() || String(profile.get("provider_id", String())).is_empty() || String(profile.get("model", String())).is_empty()) {
			continue;
		}
		if (!p_enabled_only || bool(profile.get("enabled", true))) {
			profiles.push_back(profile);
		}
	}
	return profiles;
}

Array AIAgentV1UIConfigAdapter::_mcp_servers_from_config(const Dictionary &p_config) const {
	Array servers;
	const Dictionary mcp = _dictionary_from_variant(p_config.get("mcp", Dictionary()));
	const Variant servers_value = mcp.get("servers", Variant());
	if (servers_value.get_type() == Variant::ARRAY) {
		return Array(servers_value).duplicate(true);
	}

	const Dictionary server_dict = _dictionary_from_variant(servers_value);
	for (const Variant *key = server_dict.next(nullptr); key; key = server_dict.next(key)) {
		Dictionary item = _dictionary_from_variant(server_dict[*key]);
		if (!item.has("id")) {
			item["id"] = String(*key);
		}
		servers.push_back(item);
	}
	return servers;
}

Array AIAgentV1UIConfigAdapter::_skills_from_config(const Dictionary &p_config) const {
	Array result;
	if (skill_service.is_valid()) {
		const Array manifests = skill_service->list_manifests();
		for (int i = 0; i < manifests.size(); i++) {
			if (manifests[i].get_type() == Variant::DICTIONARY) {
				result.push_back(Dictionary(manifests[i]).duplicate(true));
			}
		}
	}

	const Dictionary skills = _dictionary_from_variant(p_config.get("skills", Dictionary()));
	const Array sources = _array_from_variant(skills.get("sources", Array()));
	for (int i = 0; i < sources.size(); i++) {
		Dictionary source;
		source["id"] = String(sources[i]);
		source["source"] = String(sources[i]);
		source["kind"] = "source";
		result.push_back(source);
	}
	return result;
}

Array AIAgentV1UIConfigAdapter::_rules_from_config(const Dictionary &p_config) const {
	const Dictionary permissions = _dictionary_from_variant(p_config.get("permissions", Dictionary()));
	return _array_from_variant(permissions.get("rules", Array()));
}

String AIAgentV1UIConfigAdapter::_custom_instructions_from_config(const Dictionary &p_config) const {
	const Dictionary agents = _dictionary_from_variant(p_config.get("agents", Dictionary()));
	const Dictionary main_agent = _dictionary_from_variant(agents.get("main", Dictionary()));
	return String(main_agent.get("custom_instructions", main_agent.get("customInstructions", String()))).strip_edges();
}

Array AIAgentV1UIConfigAdapter::_marquees_from_config(const Dictionary &p_config) const {
	const Dictionary ui = _dictionary_from_variant(p_config.get("ui", Dictionary()));
	const Dictionary marquee = _dictionary_from_variant(ui.get("marquee", Dictionary()));
	return _array_from_variant(marquee.get("presets", Array()));
}

void AIAgentV1UIConfigAdapter::_config_changed(const Dictionary &p_config) {
	const Dictionary config = p_config.duplicate(true);
	emit_signal(SNAME("config_changed"), String("config"), config);
	emit_signal(SNAME("models_changed"), _model_profiles_from_config(config, true));
	emit_signal(SNAME("rules_changed"), _rules_from_config(config));
	const Dictionary ui = _dictionary_from_variant(config.get("ui", Dictionary()));
	const Dictionary marquee = _dictionary_from_variant(ui.get("marquee", Dictionary()));
	emit_signal(SNAME("marquee_changed"), _marquees_from_config(config), String(marquee.get("active_id", String())));
}

void AIAgentV1UIConfigAdapter::_mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary) {
	emit_signal(SNAME("mcp_status_changed"), p_statuses.duplicate(true), p_summary.duplicate(true));
}

void AIAgentV1UIConfigAdapter::_skill_tools_changed() {
	Array statuses;
	const Dictionary config = config_service.is_valid() ? config_service->get_config() : Dictionary();
	const Array skills = _skills_from_config(config);
	for (int i = 0; i < skills.size(); i++) {
		if (skills[i].get_type() == Variant::DICTIONARY) {
			statuses.push_back(Dictionary(skills[i]).duplicate(true));
		}
	}
	Dictionary summary;
	summary["count"] = statuses.size();
	emit_signal(SNAME("skill_status_changed"), statuses, summary);
}

void AIAgentV1UIConfigAdapter::set_config_service(const Ref<AIConfigService> &p_service) {
	config_service = p_service;
	_ensure_defaults();
}

Ref<AIConfigService> AIAgentV1UIConfigAdapter::get_config_service() const {
	return config_service;
}

void AIAgentV1UIConfigAdapter::set_mcp_service(const Ref<AIV1MCPService> &p_service) {
	mcp_service = p_service;
	_ensure_defaults();
}

Ref<AIV1MCPService> AIAgentV1UIConfigAdapter::get_mcp_service() const {
	return mcp_service;
}

void AIAgentV1UIConfigAdapter::set_skill_service(const Ref<AIV1SkillService> &p_service) {
	skill_service = p_service;
	_ensure_defaults();
}

Ref<AIV1SkillService> AIAgentV1UIConfigAdapter::get_skill_service() const {
	return skill_service;
}

void AIAgentV1UIConfigAdapter::set_agent_service(const Ref<AIAgentService> &p_service) {
	agent_service = p_service;
	_ensure_defaults();
}

Ref<AIAgentService> AIAgentV1UIConfigAdapter::get_agent_service() const {
	return agent_service;
}

Dictionary AIAgentV1UIConfigAdapter::get_settings_snapshot() {
	_ensure_defaults();
	const Dictionary config = config_service->get_config();

	Dictionary snapshot;
	snapshot["success"] = true;
	snapshot["default_provider"] = config.get("default_provider", String());
	snapshot["default_model"] = config.get("default_model", String());
	snapshot["models"] = _models_from_config(config);
	snapshot["agents"] = list_agents();
	snapshot["mcp_servers"] = _mcp_servers_from_config(config);
	snapshot["mcp_statuses"] = mcp_service.is_valid() ? mcp_service->get_statuses() : Array();
	snapshot["mcp_summary"] = mcp_service.is_valid() ? mcp_service->get_status_summary() : Dictionary();
	snapshot["skills"] = _skills_from_config(config);
	snapshot["rules"] = _rules_from_config(config);
	snapshot["custom_instructions"] = _custom_instructions_from_config(config);
	snapshot["marquees"] = _marquees_from_config(config);

	const Dictionary ui = _dictionary_from_variant(config.get("ui", Dictionary()));
	const Dictionary marquee = _dictionary_from_variant(ui.get("marquee", Dictionary()));
	snapshot["active_marquee_id"] = marquee.get("active_id", String());
	snapshot["metadata"] = _dictionary_from_variant(config.get("metadata", Dictionary()));
	return snapshot;
}

Array AIAgentV1UIConfigAdapter::list_models() {
	_ensure_defaults();
	return _models_from_config(config_service->get_config());
}

Array AIAgentV1UIConfigAdapter::list_model_provider_presets() {
	return AIModelCatalog::list_provider_presets();
}

Array AIAgentV1UIConfigAdapter::list_model_profiles(bool p_enabled_only) {
	_ensure_defaults();
	return _model_profiles_from_config(config_service->get_config(), p_enabled_only);
}

Dictionary AIAgentV1UIConfigAdapter::get_model_profile(const String &p_profile_id) {
	_ensure_defaults();
	const String profile_id = p_profile_id.strip_edges();
	if (profile_id.is_empty()) {
		return Dictionary();
	}

	const Dictionary config = config_service->get_config();
	const Dictionary providers = _dictionary_from_variant(config.get("providers", Dictionary()));
	const Dictionary provider = _dictionary_from_variant(providers.get(profile_id, Dictionary()));
	if (provider.is_empty() || !bool(provider.get("ui_model_profile", false))) {
		return Dictionary();
	}
	return _model_profile_from_provider_config(profile_id, provider);
}

Dictionary AIAgentV1UIConfigAdapter::add_model_profile(const Dictionary &p_profile, const String &p_scope) {
	_ensure_defaults();

	Dictionary profile = p_profile.duplicate(true);
	const String provider_id = String(profile.get("provider_id", profile.get("provider", String()))).strip_edges();
	const String model = String(profile.get("model", String())).strip_edges();
	if (provider_id.is_empty() || model.is_empty()) {
		Dictionary details;
		details["provider_id"] = provider_id;
		details["model"] = model;
		return _make_error_result("Model profile provider_id and model are required.", details);
	}

	String profile_id = String(profile.get("id", profile.get("provider_key", String()))).strip_edges();
	if (profile_id.is_empty()) {
		profile_id = AIId::make("model_profile");
	}
	profile["id"] = profile_id;
	profile["provider_id"] = provider_id;
	profile["model"] = model;
	const bool enabled = bool(profile.get("enabled", true));
	profile["enabled"] = enabled;

	Dictionary providers_patch;
	providers_patch[profile_id] = _provider_config_from_model_profile(profile);

	Dictionary patch;
	patch["providers"] = providers_patch;

	const Dictionary config = config_service->get_config();
	if (enabled && _model_profiles_from_config(config, true).is_empty()) {
		patch["default_provider"] = profile_id;
		patch["default_model"] = model;

		Dictionary main_agent;
		main_agent["provider"] = profile_id;
		main_agent["model"] = model;
		Dictionary agents_patch;
		agents_patch["main"] = main_agent;
		patch["agents"] = agents_patch;
	}

	Dictionary result = patch_settings(patch, p_scope);
	if (bool(result.get("success", true))) {
		result["success"] = true;
		result["id"] = profile_id;
		result["profile"] = get_model_profile(profile_id);
	}
	return result;
}

Dictionary AIAgentV1UIConfigAdapter::update_model_profile(const String &p_profile_id, const Dictionary &p_profile, const String &p_scope) {
	_ensure_defaults();

	const String profile_id = p_profile_id.strip_edges();
	Dictionary existing = get_model_profile(profile_id);
	if (profile_id.is_empty() || existing.is_empty()) {
		Dictionary details;
		details["profile_id"] = profile_id;
		return _make_error_result("Model profile was not found.", details);
	}

	Dictionary profile = _merge_model_profile(existing, p_profile);
	const String provider_id = String(profile.get("provider_id", profile.get("provider", String()))).strip_edges();
	const String model = String(profile.get("model", String())).strip_edges();
	if (provider_id.is_empty() || model.is_empty()) {
		Dictionary details;
		details["profile_id"] = profile_id;
		details["provider_id"] = provider_id;
		details["model"] = model;
		return _make_error_result("Model profile provider_id and model are required.", details);
	}

	profile["id"] = profile_id;
	profile["provider_key"] = profile_id;
	profile["provider_id"] = provider_id;
	profile["model"] = model;
	profile["enabled"] = bool(profile.get("enabled", true));

	Dictionary provider_patch = _provider_config_from_model_profile(profile);
	_mark_legacy_model_profile_fields_for_deletion(provider_patch);

	Dictionary providers_patch;
	providers_patch[profile_id] = provider_patch;

	Dictionary patch;
	patch["providers"] = providers_patch;

	const Dictionary config = config_service->get_config();
	const bool enabled = bool(profile.get("enabled", true));
	const bool updating_default = String(config.get("default_provider", String())).strip_edges() == profile_id;
	const Dictionary agents = _dictionary_from_variant(config.get("agents", Dictionary()));
	const Dictionary main_agent = _dictionary_from_variant(agents.get("main", Dictionary()));
	const bool updating_main_model = String(main_agent.get("provider", String())).strip_edges() == profile_id;
	if (enabled && updating_default) {
		patch["default_model"] = model;
	}
	if (enabled && updating_main_model) {
		Dictionary main_patch;
		main_patch["model"] = model;
		Dictionary agents_patch;
		agents_patch["main"] = main_patch;
		patch["agents"] = agents_patch;
	}
	if (!enabled && (updating_default || updating_main_model)) {
		const Dictionary replacement = _find_replacement_model_profile(_model_profiles_from_config(config, true), profile_id);
		_apply_replacement_model_profile(patch, updating_default, updating_main_model, replacement);
	}

	Dictionary result = patch_settings(patch, p_scope);
	if (bool(result.get("success", true))) {
		result["success"] = true;
		result["id"] = profile_id;
		result["profile"] = get_model_profile(profile_id);
	}
	return result;
}

Dictionary AIAgentV1UIConfigAdapter::remove_model_profile(const String &p_profile_id, const String &p_scope) {
	_ensure_defaults();

	const String profile_id = p_profile_id.strip_edges();
	Dictionary existing = get_model_profile(profile_id);
	if (profile_id.is_empty() || existing.is_empty()) {
		Dictionary details;
		details["profile_id"] = profile_id;
		return _make_error_result("Model profile was not found.", details);
	}

	Dictionary providers_patch;
	providers_patch[profile_id] = Variant();

	Dictionary patch;
	patch["providers"] = providers_patch;

	const Dictionary config = config_service->get_config();
	const bool removing_default = String(config.get("default_provider", String())).strip_edges() == profile_id;
	const Dictionary agents = _dictionary_from_variant(config.get("agents", Dictionary()));
	const Dictionary main_agent = _dictionary_from_variant(agents.get("main", Dictionary()));
	const bool removing_main_model = String(main_agent.get("provider", String())).strip_edges() == profile_id;
	if (removing_default || removing_main_model) {
		const Dictionary replacement = _find_replacement_model_profile(_model_profiles_from_config(config, true), profile_id);
		_apply_replacement_model_profile(patch, removing_default, removing_main_model, replacement);
	}

	Dictionary result = patch_settings(patch, p_scope);
	if (bool(result.get("success", true))) {
		result["success"] = true;
		result["id"] = profile_id;
	}
	return result;
}

Array AIAgentV1UIConfigAdapter::list_agents() {
	_ensure_defaults();
	if (agent_service.is_null()) {
		return Array();
	}
	return agent_service->list();
}

Dictionary AIAgentV1UIConfigAdapter::patch_settings(const Dictionary &p_patch, const String &p_scope) {
	_ensure_defaults();
	Dictionary result = config_service->patch_config(p_patch, p_scope);
	if (!bool(result.get("success", true))) {
		Dictionary error = result.get("error", Dictionary());
		emit_signal(SNAME("error_reported"), error);
		return result;
	}
	result["success"] = true;
	if (mcp_service.is_valid()) {
		mcp_service->import_config(result);
	}
	if (skill_service.is_valid()) {
		skill_service->import_config(result);
	}
	emit_signal(SNAME("config_changed"), p_scope, result.duplicate(true));
	emit_signal(SNAME("models_changed"), _model_profiles_from_config(result, true));
	emit_signal(SNAME("rules_changed"), _rules_from_config(result));
	return result;
}
