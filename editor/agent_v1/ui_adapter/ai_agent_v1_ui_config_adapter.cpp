/**************************************************************************/
/*  ai_agent_v1_ui_config_adapter.cpp                                     */
/**************************************************************************/

#include "ai_agent_v1_ui_config_adapter.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

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

Array AIAgentV1UIConfigAdapter::_marquees_from_config(const Dictionary &p_config) const {
	const Dictionary ui = _dictionary_from_variant(p_config.get("ui", Dictionary()));
	const Dictionary marquee = _dictionary_from_variant(ui.get("marquee", Dictionary()));
	return _array_from_variant(marquee.get("presets", Array()));
}

void AIAgentV1UIConfigAdapter::_config_changed(const Dictionary &p_config) {
	const Dictionary config = p_config.duplicate(true);
	emit_signal(SNAME("config_changed"), String("config"), config);
	emit_signal(SNAME("models_changed"), _models_from_config(config));
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
	emit_signal(SNAME("models_changed"), _models_from_config(result));
	emit_signal(SNAME("rules_changed"), _rules_from_config(result));
	return result;
}
