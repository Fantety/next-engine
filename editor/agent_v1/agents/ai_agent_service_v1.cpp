/**************************************************************************/
/*  ai_agent_service_v1.cpp                                                */
/**************************************************************************/

#include "ai_agent_service_v1.h"

#include "editor/agent_v1/core/base/ai_id.h"
#include "editor/agent_v1/domain/projection/ai_session_projector.h"
#include "editor/agent_v1/session/service/ai_session_service.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

static String _ai_agent_service_string_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME || p_value.get_type() == Variant::NODE_PATH) {
		return String(p_value).strip_edges();
	}
	return String();
}

static bool _ai_agent_service_update_subagent_metadata(const Ref<AISessionStore> &p_store, const String &p_child_session_id, const String &p_status, const String &p_result, AIError &r_error) {
	if (p_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionStore is required to update subagent metadata.");
		return false;
	}

	AISessionRow child_session;
	if (!p_store->get_session_struct(p_child_session_id, child_session)) {
		Dictionary details;
		details["session_id"] = p_child_session_id;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Child session not found.", details);
		return false;
	}

	Dictionary metadata = child_session.metadata.duplicate(true);
	metadata["subagent_status"] = p_status;
	metadata["subagentStatus"] = p_status;
	if (!p_result.is_empty()) {
		metadata["subagent_result"] = p_result;
		metadata["subagentResult"] = p_result;
	}

	String store_error;
	AISessionRow updated;
	if (!p_store->update_metadata_struct(p_child_session_id, metadata, updated, store_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, store_error);
		return false;
	}

	r_error = AIError::none();
	return true;
}

Dictionary AIAgentConfig::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["name"] = name;
	result["description"] = description;
	result["provider"] = provider;
	result["model"] = model;
	result["system"] = system.duplicate(true);
	result["tools"] = tools.duplicate(true);
	result["permissions"] = permissions.duplicate(true);
	result["context"] = context.duplicate(true);
	result["skills"] = skills.duplicate(true);
	result["subagents"] = subagents.duplicate(true);
	result["metadata"] = metadata.duplicate(true);
	result["success"] = true;
	return result;
}

AIAgentConfig AIAgentConfig::from_dictionary(const String &p_id, const Dictionary &p_dict, const String &p_default_provider, const String &p_default_model) {
	AIAgentConfig result;
	result.id = p_id.strip_edges();
	result.name = _ai_agent_service_string_from_variant(p_dict.get("name", result.id));
	result.description = _ai_agent_service_string_from_variant(p_dict.get("description", String()));

	Dictionary model_ref;
	if (p_dict.get("model", Variant()).get_type() == Variant::DICTIONARY) {
		model_ref = Dictionary(p_dict["model"]).duplicate(true);
	} else if (p_dict.get("model_ref", p_dict.get("modelRef", Variant())).get_type() == Variant::DICTIONARY) {
		model_ref = Dictionary(p_dict.get("model_ref", p_dict.get("modelRef", Dictionary()))).duplicate(true);
	}

	result.provider = _ai_agent_service_string_from_variant(model_ref.get("provider", p_dict.get("provider", p_default_provider)));
	result.model = _ai_agent_service_string_from_variant(model_ref.get("model", p_dict.get("model", p_default_model)));
	if (result.provider.is_empty()) {
		result.provider = p_default_provider.strip_edges().is_empty() ? String("fake") : p_default_provider.strip_edges();
	}
	if (result.model.is_empty()) {
		result.model = p_default_model.strip_edges().is_empty() ? String("fake-model") : p_default_model.strip_edges();
	}

	const Variant system_value = p_dict.get("system", p_dict.get("system_prompt", p_dict.get("systemPrompt", Variant())));
	if (system_value.get_type() == Variant::ARRAY) {
		result.system = Array(system_value).duplicate(true);
	} else if (system_value.get_type() == Variant::STRING || system_value.get_type() == Variant::STRING_NAME) {
		result.system.push_back(String(system_value));
	}
	if (result.system.is_empty()) {
		result.system = AIConfigService::get_fixed_system_prompt();
	}

	if (p_dict.get("tools", Variant()).get_type() == Variant::DICTIONARY) {
		result.tools = Dictionary(p_dict["tools"]).duplicate(true);
	}
	if (p_dict.get("permissions", Variant()).get_type() == Variant::DICTIONARY) {
		result.permissions = Dictionary(p_dict["permissions"]).duplicate(true);
	}
	result.permission_inherit_explicit = result.permissions.has("inherit_from_parent") || result.permissions.has("inheritFromParent");
	result.permission_default_effect_explicit = result.permissions.has("default_effect") || result.permissions.has("defaultEffect");
	if (!result.permissions.has("inherit_from_parent") && !result.permissions.has("inheritFromParent")) {
		result.permissions["inherit_from_parent"] = false;
	}
	if (!result.permissions.has("default_effect") && !result.permissions.has("defaultEffect")) {
		result.permissions["default_effect"] = "ask";
	}
	if (p_dict.get("context", Variant()).get_type() == Variant::DICTIONARY) {
		result.context = Dictionary(p_dict["context"]).duplicate(true);
	}
	if (!result.context.has("inherit_summary_from_parent") && !result.context.has("inheritSummaryFromParent")) {
		result.context["inherit_summary_from_parent"] = true;
	}
	if (!result.context.has("include_workspace_context") && !result.context.has("includeWorkspaceContext")) {
		result.context["include_workspace_context"] = true;
	}
	if (p_dict.get("skills", Variant()).get_type() == Variant::DICTIONARY) {
		result.skills = Dictionary(p_dict["skills"]).duplicate(true);
	}
	if (p_dict.get("subagents", Variant()).get_type() == Variant::DICTIONARY) {
		result.subagents = Dictionary(p_dict["subagents"]).duplicate(true);
	}
	if (!result.subagents.has("allowed_agent_ids") && result.subagents.has("allowedAgentIDs")) {
		result.subagents["allowed_agent_ids"] = result.subagents["allowedAgentIDs"];
	}
	if (!result.subagents.has("max_depth") && result.subagents.has("maxDepth")) {
		result.subagents["max_depth"] = result.subagents["maxDepth"];
	}
	if (!result.subagents.has("max_concurrent") && result.subagents.has("maxConcurrent")) {
		result.subagents["max_concurrent"] = result.subagents["maxConcurrent"];
	}
	if (!result.subagents.has("timeout_ms") && result.subagents.has("timeoutMs")) {
		result.subagents["timeout_ms"] = result.subagents["timeoutMs"];
	}
	if (!result.subagents.has("max_depth")) {
		result.subagents["max_depth"] = 1;
	}
	if (!result.subagents.has("max_concurrent")) {
		result.subagents["max_concurrent"] = 1;
	}
	if (!result.subagents.has("timeout_ms")) {
		result.subagents["timeout_ms"] = 300000;
	}
	if (p_dict.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
		result.metadata = Dictionary(p_dict["metadata"]).duplicate(true);
	}
	return result;
}

static Dictionary _ai_agent_service_model_ref_from_session_metadata(const Dictionary &p_metadata) {
	const Variant model_ref_value = p_metadata.get("model_ref", p_metadata.get("modelRef", Variant()));
	if (model_ref_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(model_ref_value).duplicate(true);
	}

	const String profile_id = _ai_agent_service_string_from_variant(p_metadata.get("selected_model_profile_id", p_metadata.get("selectedModelProfileID", String())));
	if (profile_id.is_empty()) {
		return Dictionary();
	}

	Dictionary model_ref;
	model_ref["provider"] = profile_id;
	model_ref["profile_id"] = profile_id;
	model_ref["source"] = "session_metadata";
	return model_ref;
}

static bool _ai_agent_service_apply_session_model_ref(const AISessionRow &p_session, const Ref<AIConfigService> &p_config_service, AIAgentConfig &r_agent, AIError &r_error) {
	const Dictionary model_ref = _ai_agent_service_model_ref_from_session_metadata(p_session.metadata);
	if (model_ref.is_empty()) {
		r_error = AIError::none();
		return true;
	}
	if (p_config_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "AgentService has no ConfigService.");
		return false;
	}

	String provider = _ai_agent_service_string_from_variant(model_ref.get("provider", model_ref.get("profile_id", model_ref.get("profileID", String()))));
	if (provider.is_empty()) {
		provider = r_agent.provider.strip_edges();
	}
	if (provider.is_empty()) {
		Dictionary details;
		details["session_id"] = p_session.id;
		details["model_ref"] = model_ref.duplicate(true);
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session model reference has no provider.", details);
		return false;
	}

	const Dictionary provider_config = p_config_service->get_provider_config(provider);
	if (provider_config.is_empty()) {
		Dictionary details;
		details["session_id"] = p_session.id;
		details["provider"] = provider;
		details["model_ref"] = model_ref.duplicate(true);
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Session model provider is not configured: " + provider, details);
		return false;
	}
	if (!bool(provider_config.get("enabled", true))) {
		Dictionary details;
		details["session_id"] = p_session.id;
		details["provider"] = provider;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session model provider is disabled: " + provider, details);
		return false;
	}

	String model = _ai_agent_service_string_from_variant(model_ref.get("model", String()));
	if (model.is_empty()) {
		model = _ai_agent_service_string_from_variant(provider_config.get("model", p_config_service->get_default_model()));
	}
	if (model.is_empty()) {
		Dictionary details;
		details["session_id"] = p_session.id;
		details["provider"] = provider;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session model reference has no model.", details);
		return false;
	}

	r_agent.provider = provider;
	r_agent.model = model;
	r_agent.metadata["session_model_ref"] = model_ref.duplicate(true);
	r_error = AIError::none();
	return true;
}

void AIAgentService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_config_service", "config_service"), &AIAgentService::set_config_service);
	ClassDB::bind_method(D_METHOD("get_config_service"), &AIAgentService::get_config_service);
	ClassDB::bind_method(D_METHOD("set_session_store", "session_store"), &AIAgentService::set_session_store);
	ClassDB::bind_method(D_METHOD("get_session_store"), &AIAgentService::get_session_store);
	ClassDB::bind_method(D_METHOD("resolve", "agent_id"), &AIAgentService::resolve, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("resolve_for_session", "session_id"), &AIAgentService::resolve_for_session);
	ClassDB::bind_method(D_METHOD("list"), &AIAgentService::list);
	ClassDB::bind_method(D_METHOD("assert_can_spawn", "parent_session_id", "child_agent_id"), &AIAgentService::assert_can_spawn);
}

AIAgentService::AIAgentService() {
	config_service.instantiate();
}

Dictionary AIAgentService::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Array AIAgentService::_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

Array AIAgentService::_string_array_from_variant(const Variant &p_value) {
	Array result;
	if (p_value.get_type() == Variant::ARRAY) {
		const Array source = p_value;
		for (int i = 0; i < source.size(); i++) {
			const String item = _ai_agent_service_string_from_variant(source[i]);
			if (!item.is_empty()) {
				result.push_back(item);
			}
		}
	} else {
		const String item = _ai_agent_service_string_from_variant(p_value);
		if (!item.is_empty()) {
			result.push_back(item);
		}
	}
	return result;
}

Dictionary AIAgentService::_agents_from_config(const Dictionary &p_config) {
	return _dictionary_from_variant(p_config.get("agents", Dictionary()));
}

String AIAgentService::_default_agent_id_from_config(const Dictionary &p_config) {
	const String configured = _ai_agent_service_string_from_variant(p_config.get("default_agent", p_config.get("defaultAgent", p_config.get("default_agent_id", p_config.get("defaultAgentID", String())))));
	return configured.is_empty() ? String("main") : configured;
}

bool AIAgentService::_array_contains_string(const Array &p_array, const String &p_value) {
	const String value = p_value.strip_edges();
	for (int i = 0; i < p_array.size(); i++) {
		if (_ai_agent_service_string_from_variant(p_array[i]) == value) {
			return true;
		}
	}
	return false;
}

int AIAgentService::_subagent_depth_for_session(const String &p_session_id) const {
	if (session_store.is_null()) {
		return 0;
	}

	int depth = 0;
	String cursor = p_session_id.strip_edges();
	for (int guard = 0; guard < 64 && !cursor.is_empty(); guard++) {
		AISessionRow session;
		if (!session_store->get_session_struct(cursor, session)) {
			break;
		}
		const String parent = _ai_agent_service_string_from_variant(session.metadata.get("parent_session_id", session.metadata.get("parentSessionID", String())));
		if (parent.is_empty()) {
			break;
		}
		depth++;
		cursor = parent;
	}
	return depth;
}

int AIAgentService::_child_session_count(const String &p_parent_session_id) const {
	if (session_store.is_null()) {
		return 0;
	}

	int count = 0;
	const Array sessions = session_store->list_sessions();
	for (int i = 0; i < sessions.size(); i++) {
		if (sessions[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary session = sessions[i];
		const Dictionary session_metadata = _dictionary_from_variant(session.get("metadata", Dictionary()));
		const String parent = _ai_agent_service_string_from_variant(session_metadata.get("parent_session_id", session_metadata.get("parentSessionID", String())));
		if (parent == p_parent_session_id) {
			count++;
		}
	}
	return count;
}

int AIAgentService::_active_child_session_count(const String &p_parent_session_id) const {
	if (session_store.is_null()) {
		return 0;
	}

	int count = 0;
	const Array sessions = session_store->list_sessions();
	for (int i = 0; i < sessions.size(); i++) {
		if (sessions[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary session = sessions[i];
		const Dictionary session_metadata = _dictionary_from_variant(session.get("metadata", Dictionary()));
		const String parent = _ai_agent_service_string_from_variant(session_metadata.get("parent_session_id", session_metadata.get("parentSessionID", String())));
		if (parent != p_parent_session_id) {
			continue;
		}

		const String status = _ai_agent_service_string_from_variant(session_metadata.get("subagent_status", session_metadata.get("subagentStatus", String()))).to_lower();
		if (status.is_empty() || status == "created" || status == "queued" || status == "pending" || status == "running") {
			count++;
		}
	}
	return count;
}

void AIAgentService::set_config_service(const Ref<AIConfigService> &p_config_service) {
	config_service = p_config_service;
}

Ref<AIConfigService> AIAgentService::get_config_service() const {
	return config_service;
}

void AIAgentService::set_session_store(const Ref<AISessionStore> &p_session_store) {
	session_store = p_session_store;
}

Ref<AISessionStore> AIAgentService::get_session_store() const {
	return session_store;
}

bool AIAgentService::resolve_struct(const String &p_agent_id, AIAgentConfig &r_agent, AIError &r_error) const {
	if (config_service.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "AgentService has no ConfigService.");
		return false;
	}

	const Dictionary config = config_service->get_config();
	if (!bool(config.get("success", true))) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "ConfigService failed to provide config.", config.get("error", Dictionary()));
		return false;
	}

	const Dictionary agents = _agents_from_config(config);
	String agent_id = p_agent_id.strip_edges();
	if (agent_id.is_empty()) {
		agent_id = _default_agent_id_from_config(config);
	}
	if (!agents.has(agent_id)) {
		Dictionary details;
		details["agent_id"] = agent_id;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Agent is not configured: " + agent_id, details);
		return false;
	}
	if (agents[agent_id].get_type() != Variant::DICTIONARY) {
		Dictionary details;
		details["agent_id"] = agent_id;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Agent config must be an object.", details);
		return false;
	}

	r_agent = AIAgentConfig::from_dictionary(agent_id, agents[agent_id], config_service->get_default_provider(), config_service->get_default_model());
	r_error = AIError::none();
	return true;
}

bool AIAgentService::resolve_for_session_struct(const String &p_session_id, AIAgentConfig &r_agent, AIError &r_error) const {
	String agent_id;
	AISessionRow session;
	bool has_session = false;
	if (session_store.is_valid()) {
		if (!session_store->get_session_struct(p_session_id, session)) {
			Dictionary details;
			details["session_id"] = p_session_id;
			r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Session not found.", details);
			return false;
		}
		has_session = true;
		agent_id = session.agent_id.strip_edges();
	}
	if (!resolve_struct(agent_id, r_agent, r_error)) {
		return false;
	}
	if (has_session) {
		return _ai_agent_service_apply_session_model_ref(session, config_service, r_agent, r_error);
	}
	r_error = AIError::none();
	return true;
}

bool AIAgentService::assert_can_spawn_struct(const String &p_parent_session_id, const String &p_child_agent_id, AIError &r_error) const {
	AIAgentConfig parent_agent;
	if (!resolve_for_session_struct(p_parent_session_id, parent_agent, r_error)) {
		return false;
	}

	AIAgentConfig child_agent;
	if (!resolve_struct(p_child_agent_id, child_agent, r_error)) {
		return false;
	}

	const Array allowed = _string_array_from_variant(parent_agent.subagents.get("allowed_agent_ids", parent_agent.subagents.get("allowedAgentIDs", Array())));
	if (!allowed.is_empty() && !_array_contains_string(allowed, child_agent.id)) {
		Dictionary details;
		details["parent_agent_id"] = parent_agent.id;
		details["child_agent_id"] = child_agent.id;
		r_error = AIError::make(AI_ERROR_PERMISSION, "Subagent is not allowed by parent agent policy.", details);
		return false;
	}

	const int max_depth = int(parent_agent.subagents.get("max_depth", parent_agent.subagents.get("maxDepth", 1)));
	const int current_depth = _subagent_depth_for_session(p_parent_session_id);
	if (max_depth >= 0 && current_depth >= max_depth) {
		Dictionary details;
		details["parent_session_id"] = p_parent_session_id;
		details["child_agent_id"] = child_agent.id;
		details["current_depth"] = current_depth;
		details["max_depth"] = max_depth;
		r_error = AIError::make(AI_ERROR_PERMISSION, "Subagent depth limit reached.", details);
		return false;
	}

	const int max_total = int(parent_agent.subagents.get("max_total_child_sessions", parent_agent.subagents.get("maxTotalChildSessions", 0)));
	if (max_total > 0 && _child_session_count(p_parent_session_id) >= max_total) {
		Dictionary details;
		details["parent_session_id"] = p_parent_session_id;
		details["max_total_child_sessions"] = max_total;
		r_error = AIError::make(AI_ERROR_PERMISSION, "Subagent child session limit reached.", details);
		return false;
	}

	const int max_concurrent = int(parent_agent.subagents.get("max_concurrent", parent_agent.subagents.get("maxConcurrent", 1)));
	if (max_concurrent > 0 && _active_child_session_count(p_parent_session_id) >= max_concurrent) {
		Dictionary details;
		details["parent_session_id"] = p_parent_session_id;
		details["max_concurrent"] = max_concurrent;
		r_error = AIError::make(AI_ERROR_PERMISSION, "Subagent concurrency limit reached.", details);
		return false;
	}

	r_error = AIError::none();
	return true;
}

Array AIAgentService::permission_rules_for_agent(const AIAgentConfig &p_agent, const Array &p_base_rules) const {
	Array result;
	const bool default_inherit = !p_agent.permission_inherit_explicit && !p_agent.permission_default_effect_explicit;
	bool inherit_from_parent = default_inherit;
	if (p_agent.permission_inherit_explicit) {
		inherit_from_parent = bool(p_agent.permissions.get("inherit_from_parent", p_agent.permissions.get("inheritFromParent", false)));
	}
	const String default_effect = _ai_agent_service_string_from_variant(p_agent.permissions.get("default_effect", p_agent.permissions.get("defaultEffect", String()))).to_lower();
	if (p_agent.permission_inherit_explicit || p_agent.permission_default_effect_explicit) {
		Dictionary rule;
		rule["action"] = "*";
		rule["resource"] = "*";
		rule["effect"] = default_effect.is_empty() ? String("ask") : default_effect;
		rule["reason"] = "Agent permission default.";
		result.push_back(rule);
	}
	if (inherit_from_parent) {
		const Array inherited = p_base_rules.duplicate(true);
		for (int i = 0; i < inherited.size(); i++) {
			result.push_back(inherited[i]);
		}
	}
	const Array additional_rules = _array_from_variant(p_agent.permissions.get("additional_rules", p_agent.permissions.get("additionalRules", Array())));
	for (int i = 0; i < additional_rules.size(); i++) {
		result.push_back(additional_rules[i]);
	}

	const Array disabled_tools = _string_array_from_variant(p_agent.tools.get("disabled", p_agent.tools.get("disabled_tools", p_agent.tools.get("disabledTools", Array()))));
	for (int i = 0; i < disabled_tools.size(); i++) {
		Dictionary rule;
		rule["action"] = disabled_tools[i];
		rule["resource"] = "*";
		rule["effect"] = "deny";
		rule["reason"] = "Tool disabled by agent policy.";
		result.push_back(rule);
	}
	return result;
}

Dictionary AIAgentService::resolve(const String &p_agent_id) const {
	AIAgentConfig agent;
	AIError error;
	if (!resolve_struct(p_agent_id, agent, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	return agent.to_dictionary();
}

Dictionary AIAgentService::resolve_for_session(const String &p_session_id) const {
	AIAgentConfig agent;
	AIError error;
	if (!resolve_for_session_struct(p_session_id, agent, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	return agent.to_dictionary();
}

Array AIAgentService::list() const {
	Array result;
	if (config_service.is_null()) {
		return result;
	}

	const Dictionary config = config_service->get_config();
	const Dictionary agents = _agents_from_config(config);
	const String default_provider = config_service->get_default_provider();
	const String default_model = config_service->get_default_model();
	for (const KeyValue<Variant, Variant> &kv : agents) {
		if (kv.value.get_type() != Variant::DICTIONARY) {
			continue;
		}
		result.push_back(AIAgentConfig::from_dictionary(String(kv.key), kv.value, default_provider, default_model).to_dictionary());
	}
	return result;
}

Dictionary AIAgentService::assert_can_spawn(const String &p_parent_session_id, const String &p_child_agent_id) const {
	AIError error;
	const bool ok = assert_can_spawn_struct(p_parent_session_id, p_child_agent_id, error);
	Dictionary result;
	const bool success = ok && !error.is_error();
	result["success"] = success;
	if (!success) {
		result["error"] = error.to_dictionary();
	}
	return result;
}

void AIV1TaskTool::_bind_methods() {
}

Dictionary AIV1TaskTool::_schema_property(const String &p_type, const String &p_description) {
	Dictionary property;
	property["type"] = p_type;
	if (!p_description.is_empty()) {
		property["description"] = p_description;
	}
	return property;
}

Dictionary AIV1TaskTool::_object_schema(const Dictionary &p_properties, const Array &p_required) {
	Dictionary schema;
	schema["type"] = "object";
	schema["properties"] = p_properties;
	schema["required"] = p_required;
	return schema;
}

String AIV1TaskTool::_build_child_prompt(const Dictionary &p_arguments) {
	String prompt;
	const String description = String(p_arguments.get("description", String())).strip_edges();
	const String task_prompt = String(p_arguments.get("prompt", String())).strip_edges();
	const String expected = String(p_arguments.get("expected_output", p_arguments.get("expectedOutput", String()))).strip_edges();

	if (!description.is_empty()) {
		prompt += "[Parent Task]\n" + description + "\n";
	}
	if (!task_prompt.is_empty()) {
		prompt += (prompt.is_empty() ? String() : String("\n")) + "[Prompt]\n" + task_prompt + "\n";
	}
	if (!expected.is_empty()) {
		prompt += "\n[Expected Output]\n" + expected + "\n";
	}
	return prompt.strip_edges();
}

String AIV1TaskTool::_latest_assistant_text(const Ref<AISessionProjector> &p_projector, const String &p_session_id) {
	if (p_projector.is_null()) {
		return String();
	}
	const Vector<AISessionMessage> messages = p_projector->get_messages_struct(p_session_id);
	for (int i = messages.size() - 1; i >= 0; i--) {
		if (messages[i].type != AI_SESSION_MESSAGE_ASSISTANT) {
			continue;
		}
		String text = messages[i].text;
		for (int j = 0; j < messages[i].content.size(); j++) {
			const AIAssistantContent &content = messages[i].content[j];
			if (content.type == "text" || content.type == "reasoning") {
				text += text.is_empty() ? content.text : "\n" + content.text;
			}
		}
		if (!text.strip_edges().is_empty()) {
			return text.strip_edges();
		}
	}
	return String();
}

String AIV1TaskTool::_summary_from_result(const String &p_agent_id, const String &p_result, const String &p_status) {
	const String agent = p_agent_id.strip_edges().is_empty() ? String("subagent") : p_agent_id.strip_edges();
	if (p_status == "completed") {
		return "Subagent " + agent + " completed: " + p_result.substr(0, MIN(240, p_result.length()));
	}
	return "Subagent " + agent + " " + p_status + ".";
}

AIV1TaskTool::AIV1TaskTool() {
	Dictionary properties;
	properties["agent_id"] = _schema_property("string", "Optional target subagent id.");
	properties["description"] = _schema_property("string", "Short delegated task description.");
	properties["prompt"] = _schema_property("string", "Detailed prompt for the subagent.");
	properties["expected_output"] = _schema_property("string", "Optional expected output format.");
	properties["context"] = _schema_property("object", "Optional explicit context references.");
	Array required;
	required.push_back("description");
	required.push_back("prompt");
	Dictionary tool_metadata;
	tool_metadata["action"] = "agent.spawn";
	tool_metadata["tool_origin"] = "builtin";
	tool_metadata["phase"] = "10";
	configure("Run a delegated subagent task and return its result.", _object_schema(properties, required), Callable(), tool_metadata);
}

void AIV1TaskTool::setup(AISessionService *p_session_service, AIAgentService *p_agent_service) {
	session_service_id = p_session_service ? p_session_service->get_instance_id() : ObjectID();
	agent_service_id = p_agent_service ? p_agent_service->get_instance_id() : ObjectID();
}

bool AIV1TaskTool::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	const String task_description = String(p_arguments.get("description", String())).strip_edges();
	const String prompt = String(p_arguments.get("prompt", String())).strip_edges();
	if (task_description.is_empty() || prompt.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Task tool requires description and prompt.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	if (p_context.cancel_token.is_valid() && p_context.cancel_token->is_cancel_requested()) {
		r_error = AIError::make(AI_ERROR_INTERRUPTED, p_context.cancel_token->get_cancel_message("Subagent task interrupted."));
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	AISessionService *session_service = Object::cast_to<AISessionService>(ObjectDB::get_instance(session_service_id));
	AIAgentService *agent_service = Object::cast_to<AIAgentService>(ObjectDB::get_instance(agent_service_id));
	if (!session_service || !agent_service) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Task tool dependencies are not available.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	AIAgentConfig parent_agent;
	if (!agent_service->resolve_for_session_struct(p_context.session_id, parent_agent, r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	String child_agent_id = String(p_arguments.get("agent_id", p_arguments.get("agentID", String()))).strip_edges();
	if (child_agent_id.is_empty()) {
		const Array allowed = AIAgentService::_string_array_from_variant(parent_agent.subagents.get("allowed_agent_ids", parent_agent.subagents.get("allowedAgentIDs", Array())));
		if (!allowed.is_empty()) {
			child_agent_id = String(allowed[0]).strip_edges();
		}
	}

	AIAgentConfig child_agent;
	if (!agent_service->resolve_struct(child_agent_id, child_agent, r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const int64_t timeout_ms = int64_t(p_arguments.get("timeout_ms", p_arguments.get("timeoutMs", parent_agent.subagents.get("timeout_ms", parent_agent.subagents.get("timeoutMs", 300000)))));
	if (timeout_ms <= 0) {
		Dictionary details;
		details["timeout_ms"] = timeout_ms;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Subagent task timeout_ms must be positive.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Ref<AISessionStore> store = session_service->get_session_store();
	AISessionRow parent_session;
	if (store.is_null() || !store->get_session_struct(p_context.session_id, parent_session)) {
		Dictionary details;
		details["session_id"] = p_context.session_id;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Parent session not found.", details);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const String parent_assistant_message_id = p_context.assistant_message_id.strip_edges();
	const String parent_tool_call_id = p_context.call_id.strip_edges();
	const String task_key = p_context.session_id + "|" + parent_assistant_message_id + "|" + parent_tool_call_id;
	const String task_hash = task_key.sha256_text().substr(0, 32);
	const String child_session_id_hint = "sess_task_" + task_hash;
	const String child_prompt_id = "prompt_task_" + task_hash;
	const String child_message_id = "msg_task_" + task_hash;

	bool existing_child_for_call = false;
	AISessionRow existing_child;
	if (store->get_session_struct(child_session_id_hint, existing_child)) {
		const Dictionary existing_metadata = existing_child.metadata;
		existing_child_for_call = _ai_agent_service_string_from_variant(existing_metadata.get("parent_session_id", existing_metadata.get("parentSessionID", String()))) == p_context.session_id &&
				_ai_agent_service_string_from_variant(existing_metadata.get("parent_assistant_message_id", existing_metadata.get("parentAssistantMessageID", String()))) == parent_assistant_message_id &&
				_ai_agent_service_string_from_variant(existing_metadata.get("parent_tool_call_id", existing_metadata.get("parentToolCallID", String()))) == parent_tool_call_id;
	}

	if (!existing_child_for_call && !agent_service->assert_can_spawn_struct(p_context.session_id, child_agent.id, r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	if (p_context.permission_service.is_null()) {
		r_error = AIError::make(AI_ERROR_PERMISSION, "PermissionService is required to spawn subagents.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary permission_input = p_context.make_permission_input("agent.spawn", child_agent.id, "Run delegated subagent task.");
	permission_input["default_effect"] = parent_agent.permissions.get("spawn_default_effect", parent_agent.permissions.get("default_effect", parent_agent.permissions.get("defaultEffect", "ask")));
	AIPermissionDecision decision;
	if (!p_context.permission_service->assert_permission_struct(permission_input, decision, r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary child_metadata;
	child_metadata["parent_session_id"] = p_context.session_id;
	child_metadata["parentSessionID"] = p_context.session_id;
	child_metadata["parent_assistant_message_id"] = parent_assistant_message_id;
	child_metadata["parentAssistantMessageID"] = parent_assistant_message_id;
	child_metadata["parent_tool_call_id"] = parent_tool_call_id;
	child_metadata["parentToolCallID"] = parent_tool_call_id;
	child_metadata["parent_agent_id"] = parent_agent.id;
	child_metadata["task_description"] = task_description;
	child_metadata["task_expected_output"] = p_arguments.get("expected_output", p_arguments.get("expectedOutput", String()));
	child_metadata["task_key"] = task_key;
	child_metadata["subagent_depth"] = agent_service->_subagent_depth_for_session(p_context.session_id) + 1;
	child_metadata["subagent_status"] = "created";

	Dictionary create_input;
	create_input["id"] = child_session_id_hint;
	create_input["agent_id"] = child_agent.id;
	create_input["location"] = parent_session.location.to_dictionary();
	create_input["directory"] = parent_session.location.directory;
	create_input["workspace_id"] = parent_session.location.workspace_id;
	create_input["title"] = task_description.is_empty() ? String("Subagent Task") : task_description;
	create_input["metadata"] = child_metadata;
	Dictionary created = session_service->create(create_input);
	if (!bool(created.get("success", false))) {
		r_error = AIError::make(AIError::string_to_kind(Dictionary(created.get("error", Dictionary())).get("kind", "internal")), Dictionary(created.get("error", Dictionary())).get("message", "Failed to create child session."), Dictionary(created.get("error", Dictionary())).get("details", Dictionary()));
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	const String child_session_id = String(created.get("id", String())).strip_edges();
	if (child_session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Task tool created an empty child session id.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	if (!_ai_agent_service_update_subagent_metadata(store, child_session_id, "queued", String(), r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary prompt_input;
	prompt_input["session_id"] = child_session_id;
	prompt_input["prompt_id"] = child_prompt_id;
	prompt_input["message_id"] = child_message_id;
	prompt_input["text"] = _build_child_prompt(p_arguments);
	prompt_input["delivery"] = "queue";
	prompt_input["resume"] = false;
	prompt_input["idempotency_key"] = "task.prompt:" + task_key.sha256_text();
	Dictionary prompt_result = session_service->prompt(prompt_input);
	if (!bool(prompt_result.get("success", false))) {
		r_error = AIError::make(AIError::string_to_kind(Dictionary(prompt_result.get("error", Dictionary())).get("kind", "internal")), Dictionary(prompt_result.get("error", Dictionary())).get("message", "Failed to prompt child session."), Dictionary(prompt_result.get("error", Dictionary())).get("details", Dictionary()));
		AIError metadata_error;
		(void)_ai_agent_service_update_subagent_metadata(store, child_session_id, "failed", String(r_error.message), metadata_error);
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	String status = "completed";
	String final_text;
	if (!_ai_agent_service_update_subagent_metadata(store, child_session_id, "running", String(), r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Ref<AISessionRunner> child_runner = session_service->get_session_runner();
	const uint64_t started_msec = OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : 0;
	Vector<AISessionInputRecord> child_promoted;
	AIError run_error;
	bool run_ok = false;
	if (child_runner.is_valid()) {
		run_ok = child_runner->drain_struct(child_session_id, p_context.cancel_token, 0, child_promoted, run_error);
	} else {
		run_error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionRunner is required to run subagent tasks.");
	}

	const uint64_t elapsed_msec = OS::get_singleton() && started_msec > 0 ? OS::get_singleton()->get_ticks_msec() - started_msec : 0;
	if (run_ok && elapsed_msec > uint64_t(timeout_ms)) {
		Dictionary details;
		details["timeout_ms"] = timeout_ms;
		details["elapsed_ms"] = int64_t(elapsed_msec);
		run_ok = false;
		run_error = AIError::make(AI_ERROR_TIMEOUT, "Subagent task exceeded timeout_ms.", details);
	}

	if (!run_ok) {
		status = run_error.kind == AI_ERROR_INTERRUPTED ? String("interrupted") : String("failed");
		final_text = run_error.message.is_empty() ? String("Subagent run failed.") : run_error.message;
	} else {
		final_text = _latest_assistant_text(session_service->get_projector(), child_session_id);
		if (final_text.is_empty()) {
			final_text = "Subagent completed without a text result.";
		}
	}
	if (!_ai_agent_service_update_subagent_metadata(store, child_session_id, status, final_text, r_error)) {
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}
	if (!run_ok && run_error.kind == AI_ERROR_TIMEOUT) {
		r_error = run_error;
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary output;
	output["child_session_id"] = child_session_id;
	output["summary"] = _summary_from_result(child_agent.id, final_text, status);
	output["result"] = final_text;
	output["status"] = status;
	output["agent_id"] = child_agent.id;

	r_result = AIV1ToolExecutionResult::ok(output, output["summary"], output);
	r_result.metadata["tool_origin"] = "subagent";
	r_result.metadata["child_session_id"] = child_session_id;
	r_result.metadata["agent_id"] = child_agent.id;
	r_error = AIError::none();
	return true;
}
