/**************************************************************************/
/*  ai_tool_registry_v1.cpp                                                */
/**************************************************************************/

#include "ai_tool_registry_v1.h"

#include "editor/agent_v1/core/base/ai_id.h"
#include "editor/agent_v1/domain/events/ai_event_types.h"
#include "editor/agent_v1/tools/ai_builtin_tools_v1.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

Dictionary AIV1ToolSettlement::to_dictionary() const {
	Dictionary output;
	output["success"] = success && !error.is_error();
	output["executed"] = executed;
	output["stale"] = stale;
	output["pending"] = pending;
	output["provider_executed"] = provider_executed;
	output["needs_continuation"] = needs_continuation;
	output["session_id"] = session_id;
	output["agent_id"] = agent_id;
	output["assistant_message_id"] = assistant_message_id;
	output["call_id"] = call_id;
	output["tool"] = tool_name;
	output["input"] = input;
	output["result"] = result;
	output["content"] = content;
	output["structured"] = structured;
	output["output_paths"] = output_paths;
	output["metadata"] = metadata;
	output["error"] = error.to_dictionary();
	return output;
}

void AIV1ToolMaterialization::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_definitions"), &AIV1ToolMaterialization::get_definitions);
	ClassDB::bind_method(D_METHOD("has_tool", "name"), &AIV1ToolMaterialization::has_tool);
	ClassDB::bind_method(D_METHOD("settle", "input"), &AIV1ToolMaterialization::settle);
}

void AIV1ToolMaterialization::setup(const Ref<AIV1ToolRegistry> &p_registry, const HashMap<String, MaterializedEntry> &p_entries, const String &p_root_dir) {
	registry = p_registry;
	entries = p_entries;
	root_dir = p_root_dir.strip_edges();
	definition_cache.clear();
	for (const KeyValue<String, MaterializedEntry> &kv : entries) {
		definition_cache.push_back(kv.value.definition.to_dictionary());
	}
}

Array AIV1ToolMaterialization::get_definitions() const {
	return definition_cache.duplicate(true);
}

bool AIV1ToolMaterialization::has_tool(const String &p_name) const {
	return entries.has(p_name.strip_edges());
}

Dictionary AIV1ToolMaterialization::get_tool_identity(const String &p_name) const {
	const String name = p_name.strip_edges();
	if (!entries.has(name)) {
		return Dictionary();
	}
	return entries[name].identity.to_dictionary();
}

bool AIV1ToolMaterialization::settle_struct(const Dictionary &p_input, AIV1ToolSettlement &r_settlement, AIError &r_error) {
	const Dictionary call = p_input.get("call", p_input);
	const String tool_name = String(call.get("name", call.get("tool", String()))).strip_edges();
	r_settlement.tool_name = tool_name;
	r_settlement.session_id = p_input.get("session_id", p_input.get("sessionID", String()));
	r_settlement.agent_id = p_input.get("agent", p_input.get("agent_id", String()));
	r_settlement.assistant_message_id = p_input.get("assistant_message_id", p_input.get("assistantMessageID", String()));
	r_settlement.call_id = call.get("id", call.get("call_id", call.get("callID", String())));
	r_settlement.input = call.get("input", Variant());
	r_settlement.provider_executed = bool(call.get("provider_executed", call.get("providerExecuted", false)));

	if (registry.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Tool materialization has no registry.");
		r_settlement.error = r_error;
		return false;
	}
	if (tool_name.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Tool call name is required.");
		r_settlement.error = r_error;
		return registry->settle_materialized_tool(Ref<AIV1Tool>(), AIRegistrationIdentity(), p_input, r_settlement, r_error);
	}
	if (!entries.has(tool_name)) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Tool was not materialized for this provider turn: " + tool_name);
		r_settlement.error = r_error;
		return registry->settle_materialized_tool(Ref<AIV1Tool>(), AIRegistrationIdentity(), p_input, r_settlement, r_error);
	}

	const MaterializedEntry entry = entries[tool_name];
	AIRegistrationIdentity identity = entry.identity;
	Variant identity_value = call.get("registration_identity", p_input.get("registration_identity", Variant()));
	if (identity_value.get_type() == Variant::DICTIONARY) {
		const AIRegistrationIdentity recorded_identity = AIRegistrationIdentity::from_dictionary(identity_value);
		if (recorded_identity.is_valid()) {
			identity = recorded_identity;
		}
	}
	Dictionary settle_input = p_input.duplicate(true);
	if (!root_dir.is_empty() && !settle_input.has("root_dir")) {
		settle_input["root_dir"] = root_dir;
	}
	return registry->settle_materialized_tool(entry.tool, identity, settle_input, r_settlement, r_error);
}

Dictionary AIV1ToolMaterialization::settle(const Dictionary &p_input) {
	AIV1ToolSettlement settlement;
	AIError error;
	settle_struct(p_input, settlement, error);
	return settlement.to_dictionary();
}

void AIV1ToolRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_event_store", "event_store"), &AIV1ToolRegistry::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AIV1ToolRegistry::get_event_store);
	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &AIV1ToolRegistry::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &AIV1ToolRegistry::get_projector);
	ClassDB::bind_method(D_METHOD("set_permission_service", "permission_service"), &AIV1ToolRegistry::set_permission_service);
	ClassDB::bind_method(D_METHOD("get_permission_service"), &AIV1ToolRegistry::get_permission_service);
	ClassDB::bind_method(D_METHOD("set_root_dir", "root_dir"), &AIV1ToolRegistry::set_root_dir);
	ClassDB::bind_method(D_METHOD("get_root_dir"), &AIV1ToolRegistry::get_root_dir);
	ClassDB::bind_method(D_METHOD("register_tool", "name", "tool"), &AIV1ToolRegistry::register_tool);
	ClassDB::bind_method(D_METHOD("register_tool_scope", "name", "tool"), &AIV1ToolRegistry::register_tool_scope);
	ClassDB::bind_method(D_METHOD("register_builtin_tools"), &AIV1ToolRegistry::register_builtin_tools);
	ClassDB::bind_method(D_METHOD("has_tool", "name"), &AIV1ToolRegistry::has_tool);
	ClassDB::bind_method(D_METHOD("get_tool_identity", "name"), &AIV1ToolRegistry::get_tool_identity);
	ClassDB::bind_method(D_METHOD("materialize_for_context", "root_dir", "permission_rules"), &AIV1ToolRegistry::materialize_for_context);
	ClassDB::bind_method(D_METHOD("materialize"), &AIV1ToolRegistry::materialize);
	ClassDB::bind_method(D_METHOD("get_tool_names"), &AIV1ToolRegistry::get_tool_names);
	ClassDB::bind_method(D_METHOD("clear"), &AIV1ToolRegistry::clear);
}

AIV1ToolRegistry::AIV1ToolRegistry() {
	permission_service.instantiate();
}

bool AIV1ToolRegistry::_is_valid_tool_name(const String &p_name) {
	return AIId::is_valid_name(p_name, 128);
}

bool AIV1ToolRegistry::_validate_schema_type(const Variant &p_value, const String &p_type) {
	const String type = p_type.strip_edges().to_lower();
	if (type == "string") {
		return p_value.get_type() == Variant::STRING;
	}
	if (type == "boolean" || type == "bool") {
		return p_value.get_type() == Variant::BOOL;
	}
	if (type == "integer") {
		return p_value.get_type() == Variant::INT;
	}
	if (type == "number") {
		return p_value.get_type() == Variant::INT || p_value.get_type() == Variant::FLOAT;
	}
	if (type == "object") {
		return p_value.get_type() == Variant::DICTIONARY;
	}
	if (type == "array") {
		return p_value.get_type() == Variant::ARRAY;
	}
	return true;
}

bool AIV1ToolRegistry::_validate_arguments(const Ref<AIV1Tool> &p_tool, const Dictionary &p_arguments, AIError &r_error) {
	if (p_tool.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Tool is not available.");
		return false;
	}

	const Dictionary schema = p_tool->get_input_schema();
	const Array required = schema.get("required", Array());
	for (int i = 0; i < required.size(); i++) {
		const String key = String(required[i]);
		if (!p_arguments.has(key) || p_arguments[key].get_type() == Variant::NIL) {
			Dictionary details;
			details["argument"] = key;
			r_error = AIError::make(AI_ERROR_VALIDATION, "Missing required tool argument: " + key, details);
			return false;
		}
	}

	const Dictionary properties = schema.get("properties", Dictionary());
	for (const KeyValue<Variant, Variant> &kv : properties) {
		const String key = String(kv.key);
		if (!p_arguments.has(key) || kv.value.get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary property = kv.value;
		if (!_validate_schema_type(p_arguments[key], property.get("type", String()))) {
			Dictionary details;
			details["argument"] = key;
			details["expected"] = property.get("type", String());
			r_error = AIError::make(AI_ERROR_VALIDATION, "Invalid tool argument type: " + key, details);
			return false;
		}
	}
	return true;
}

bool AIV1ToolRegistry::_wildcard_match(const String &p_pattern, const String &p_value) {
	const String pattern = p_pattern.strip_edges();
	if (pattern.is_empty() || pattern == "*") {
		return true;
	}
	if (pattern.ends_with("*")) {
		return p_value.begins_with(pattern.substr(0, pattern.length() - 1));
	}
	return pattern == p_value;
}

bool AIV1ToolRegistry::_is_tool_wholly_disabled(const String &p_name, const Ref<AIV1Tool> &p_tool, const Array &p_rules) {
	if (p_tool.is_null()) {
		return false;
	}

	const Dictionary metadata = p_tool->get_metadata();
	const String action = String(metadata.get("action", p_name)).strip_edges();
	bool saw_rule = false;
	bool disabled = false;
	for (int i = 0; i < p_rules.size(); i++) {
		if (p_rules[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary rule = p_rules[i];
		if (!_wildcard_match(String(rule.get("action", "*")), action)) {
			continue;
		}

		saw_rule = true;
		const String resource = String(rule.get("resource", "*")).strip_edges();
		const String effect = String(rule.get("effect", "ask")).strip_edges().to_lower();
		disabled = (resource == "*" || resource.is_empty()) && effect == "deny";
	}
	return saw_rule && disabled;
}

bool AIV1ToolRegistry::_get_entry_struct(const String &p_name, ToolEntry &r_entry) const {
	MutexLock lock(mutex);
	const String name = p_name.strip_edges();
	if (!tools.has(name) || tools[name].is_empty()) {
		return false;
	}
	r_entry = tools[name][tools[name].size() - 1];
	return true;
}

bool AIV1ToolRegistry::_append_settlement_event(const AIV1ToolSettlement &p_settlement, bool p_success, AIError &r_error) {
	if (event_store.is_null()) {
		return true;
	}

	Dictionary data;
	data["assistant_message_id"] = p_settlement.assistant_message_id;
	data["call_id"] = p_settlement.call_id;
	data["tool"] = p_settlement.tool_name;
	data["name"] = p_settlement.tool_name;
	data["input"] = p_settlement.input;
	data["result"] = p_settlement.result;
	data["content"] = p_settlement.content;
	data["structured"] = p_settlement.structured;
	data["output_paths"] = p_settlement.output_paths;
	data["metadata"] = p_settlement.metadata;
	if (!p_success) {
		data["error"] = p_settlement.error.to_dictionary();
	}

	AIEventRow row;
	String event_error;
	const String event_type = p_success ? AIDomainEventTypes::tool_success() : AIDomainEventTypes::tool_failed();
	if (!event_store->append(p_settlement.session_id, event_type, data, false, row, event_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
		return false;
	}
	if (projector.is_valid()) {
		projector->project(row);
	}
	return true;
}

void AIV1ToolRegistry::_close_registration(const Dictionary &p_identity) {
	const AIRegistrationIdentity identity = AIRegistrationIdentity::from_dictionary(p_identity);
	if (!identity.is_valid()) {
		return;
	}

	MutexLock lock(mutex);
	if (!tools.has(identity.name)) {
		return;
	}

	Vector<ToolEntry> &stack = tools[identity.name];
	for (int i = stack.size() - 1; i >= 0; i--) {
		if (stack[i].identity.matches(identity)) {
			stack.remove_at(i);
			generation++;
			break;
		}
	}
	if (stack.is_empty()) {
		tools.erase(identity.name);
	}
}

void AIV1ToolRegistry::set_event_store(const Ref<AIEventStore> &p_event_store) {
	event_store = p_event_store;
	if (permission_service.is_valid()) {
		permission_service->set_event_store(p_event_store);
	}
}

Ref<AIEventStore> AIV1ToolRegistry::get_event_store() const {
	return event_store;
}

void AIV1ToolRegistry::set_projector(const Ref<AISessionProjector> &p_projector) {
	projector = p_projector;
}

Ref<AISessionProjector> AIV1ToolRegistry::get_projector() const {
	return projector;
}

void AIV1ToolRegistry::set_permission_service(const Ref<AIPermissionService> &p_permission_service) {
	permission_service = p_permission_service;
	if (permission_service.is_valid()) {
		permission_service->set_event_store(event_store);
	}
}

Ref<AIPermissionService> AIV1ToolRegistry::get_permission_service() const {
	return permission_service;
}

void AIV1ToolRegistry::set_root_dir(const String &p_root_dir) {
	MutexLock lock(mutex);
	root_dir = p_root_dir.strip_edges();
}

String AIV1ToolRegistry::get_root_dir() const {
	MutexLock lock(mutex);
	return root_dir;
}

bool AIV1ToolRegistry::register_tool_struct(const String &p_name, const Ref<AIV1Tool> &p_tool, const String &p_source, const Dictionary &p_metadata, AIRegistrationIdentity *r_identity) {
	const String name = p_name.strip_edges();
	if (!_is_valid_tool_name(name) || p_tool.is_null()) {
		return false;
	}

	MutexLock lock(mutex);
	generation++;
	ToolEntry entry;
	entry.tool = p_tool;
	entry.identity.id = AIId::make("tool");
	entry.identity.name = name;
	entry.identity.owner = owner;
	entry.identity.generation = generation;
	entry.identity.metadata = p_metadata.duplicate(true);
	if (!p_source.is_empty()) {
		entry.identity.metadata["source"] = p_source;
	}
	tools[name].push_back(entry);
	if (r_identity) {
		*r_identity = entry.identity;
	}
	return true;
}

bool AIV1ToolRegistry::register_tool(const String &p_name, const Ref<AIV1Tool> &p_tool) {
	return register_tool_struct(p_name, p_tool);
}

Ref<AIScopedRegistration> AIV1ToolRegistry::register_tool_scope_struct(const String &p_name, const Ref<AIV1Tool> &p_tool, const String &p_source, const Dictionary &p_metadata, AIError *r_error) {
	AIRegistrationIdentity identity;
	if (!register_tool_struct(p_name, p_tool, p_source, p_metadata, &identity)) {
		if (r_error) {
			*r_error = AIError::make(AI_ERROR_VALIDATION, "Failed to register tool: " + p_name);
		}
		return Ref<AIScopedRegistration>();
	}

	Ref<AIScopedRegistration> scope;
	scope.instantiate();
	scope->setup(identity, callable_mp(this, &AIV1ToolRegistry::_close_registration));
	if (r_error) {
		*r_error = AIError::none();
	}
	return scope;
}

Ref<AIScopedRegistration> AIV1ToolRegistry::register_tool_scope(const String &p_name, const Ref<AIV1Tool> &p_tool) {
	return register_tool_scope_struct(p_name, p_tool);
}

void AIV1ToolRegistry::register_builtin_tools() {
	if (!has_tool("file_read")) {
		Ref<AIV1ReadFileTool> read_tool;
		read_tool.instantiate();
		register_tool_struct("file_read", read_tool, "builtin");
	}
	if (!has_tool("file_write")) {
		Ref<AIV1WriteFileTool> write_tool;
		write_tool.instantiate();
		register_tool_struct("file_write", write_tool, "builtin");
	}
	if (!has_tool("shell_run")) {
		Ref<AIV1ShellTool> shell_tool;
		shell_tool.instantiate();
		register_tool_struct("shell_run", shell_tool, "builtin");
	}
}

bool AIV1ToolRegistry::has_tool(const String &p_name) const {
	MutexLock lock(mutex);
	const String name = p_name.strip_edges();
	return tools.has(name) && !tools[name].is_empty();
}

Dictionary AIV1ToolRegistry::get_tool_identity(const String &p_name) const {
	ToolEntry entry;
	if (!_get_entry_struct(p_name, entry)) {
		return Dictionary();
	}
	return entry.identity.to_dictionary();
}

bool AIV1ToolRegistry::is_identity_current(const String &p_name, const AIRegistrationIdentity &p_identity) const {
	ToolEntry entry;
	if (!_get_entry_struct(p_name, entry)) {
		return false;
	}
	return entry.identity.matches(p_identity);
}

Ref<AIV1ToolMaterialization> AIV1ToolRegistry::materialize_struct(const String &p_root_dir, const Array &p_permission_rules) {
	HashMap<String, AIV1ToolMaterialization::MaterializedEntry> entries;
	{
		MutexLock lock(mutex);
		for (const KeyValue<String, Vector<ToolEntry>> &kv : tools) {
			if (kv.value.is_empty()) {
				continue;
			}
			const ToolEntry entry = kv.value[kv.value.size() - 1];
			if (entry.tool.is_null() || _is_tool_wholly_disabled(kv.key, entry.tool, p_permission_rules)) {
				continue;
			}
			AIV1ToolMaterialization::MaterializedEntry materialized;
			materialized.tool = entry.tool;
			materialized.identity = entry.identity;
			materialized.definition = entry.tool->to_model_definition(kv.key, entry.identity.to_dictionary());
			entries[kv.key] = materialized;
		}
	}

	Ref<AIV1ToolMaterialization> materialization;
	materialization.instantiate();
	materialization->setup(Ref<AIV1ToolRegistry>(this), entries, p_root_dir.strip_edges());
	return materialization;
}

Ref<AIV1ToolMaterialization> AIV1ToolRegistry::materialize_struct() {
	return materialize_struct(get_root_dir(), Array());
}

Ref<AIV1ToolMaterialization> AIV1ToolRegistry::materialize_for_context(const String &p_root_dir, const Array &p_permission_rules) {
	return materialize_struct(p_root_dir, p_permission_rules);
}

Ref<AIV1ToolMaterialization> AIV1ToolRegistry::materialize() {
	return materialize_struct();
}

Array AIV1ToolRegistry::get_tool_names() const {
	MutexLock lock(mutex);
	Array names;
	for (const KeyValue<String, Vector<ToolEntry>> &kv : tools) {
		if (!kv.value.is_empty()) {
			names.push_back(kv.key);
		}
	}
	return names;
}

void AIV1ToolRegistry::clear() {
	MutexLock lock(mutex);
	tools.clear();
	generation++;
}

bool AIV1ToolRegistry::settle_materialized_tool(const Ref<AIV1Tool> &p_tool, const AIRegistrationIdentity &p_identity, const Dictionary &p_input, AIV1ToolSettlement &r_settlement, AIError &r_error) {
	const Dictionary call = p_input.get("call", p_input);
	r_settlement.session_id = p_input.get("session_id", p_input.get("sessionID", String()));
	r_settlement.agent_id = p_input.get("agent", p_input.get("agent_id", String()));
	r_settlement.assistant_message_id = p_input.get("assistant_message_id", p_input.get("assistantMessageID", String()));
	r_settlement.call_id = call.get("id", call.get("call_id", call.get("callID", String())));
	r_settlement.tool_name = call.get("name", call.get("tool", r_settlement.tool_name));
	r_settlement.input = call.get("input", Variant());
	r_settlement.provider_executed = bool(call.get("provider_executed", call.get("providerExecuted", false)));

	if (r_settlement.call_id.is_empty()) {
		r_settlement.call_id = AIId::make("call");
	}

	if (r_settlement.provider_executed) {
		r_settlement.success = true;
		r_settlement.executed = false;
		return true;
	}

	Dictionary arguments;
	if (r_settlement.input.get_type() == Variant::DICTIONARY) {
		arguments = Dictionary(r_settlement.input).duplicate(true);
	} else if (r_settlement.input.get_type() == Variant::NIL) {
		arguments = Dictionary();
	} else {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Tool input must be an object.");
		r_settlement.error = r_error;
		return _append_settlement_event(r_settlement, false, r_error);
	}

	if (p_tool.is_null()) {
		if (!r_settlement.error.is_error()) {
			r_settlement.error = AIError::make(AI_ERROR_UNAVAILABLE, "Tool is not available: " + r_settlement.tool_name);
		}
		return _append_settlement_event(r_settlement, false, r_error);
	}

	if (!is_identity_current(r_settlement.tool_name, p_identity)) {
		r_settlement.stale = true;
		r_settlement.error = AIError::make(AI_ERROR_CONFLICT, "Tool call was made against a stale materialization.");
		return _append_settlement_event(r_settlement, false, r_error);
	}

	if (!_validate_arguments(p_tool, arguments, r_error)) {
		r_settlement.error = r_error;
		return _append_settlement_event(r_settlement, false, r_error);
	}

	AIV1ToolExecutionContext context;
	context.session_id = r_settlement.session_id;
	context.agent_id = r_settlement.agent_id;
	context.assistant_message_id = r_settlement.assistant_message_id;
	context.call_id = r_settlement.call_id;
	context.tool_name = r_settlement.tool_name;
	context.root_dir = String(p_input.get("root_dir", get_root_dir())).strip_edges();
	context.permission_service = permission_service;
	context.source["assistant_message_id"] = r_settlement.assistant_message_id;
	context.source["call_id"] = r_settlement.call_id;
	context.source["tool"] = r_settlement.tool_name;

	AIV1ToolExecutionResult execution_result;
	AIError execution_error;
	const bool executed = p_tool->execute_struct(arguments, context, execution_result, execution_error);
	r_settlement.executed = executed && !execution_result.error.is_error();
	r_settlement.result = execution_result.result;
	r_settlement.content = execution_result.content;
	r_settlement.structured = execution_result.structured;
	r_settlement.output_paths = execution_result.output_paths;
	r_settlement.metadata = execution_result.metadata.duplicate(true);
	if (!executed || execution_result.error.is_error()) {
		r_settlement.error = execution_error.is_error() ? execution_error : execution_result.error;
		r_settlement.pending = r_settlement.error.message.contains("pending") || bool(r_settlement.error.details.get("status", String()) == "pending");
		if (r_settlement.pending) {
			r_settlement.needs_continuation = false;
			return true;
		}
		r_settlement.needs_continuation = true;
		return _append_settlement_event(r_settlement, false, r_error);
	}

	r_settlement.success = true;
	r_settlement.needs_continuation = true;
	return _append_settlement_event(r_settlement, true, r_error);
}
