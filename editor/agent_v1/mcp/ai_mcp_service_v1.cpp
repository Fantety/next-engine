/**************************************************************************/
/*  ai_mcp_service_v1.cpp                                                 */
/**************************************************************************/

#include "ai_mcp_service_v1.h"

#include "editor/ai_component/providers/ai_mcp_client.h"
#include "editor/agent_v1/core/base/ai_id.h"
#include "editor/agent_v1/core/testing/ai_fake_mcp_server.h"

#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

void AIV1MCPToolAdapter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_server_id"), &AIV1MCPToolAdapter::get_server_id);
	ClassDB::bind_method(D_METHOD("get_tool_name"), &AIV1MCPToolAdapter::get_tool_name);
	ClassDB::bind_method(D_METHOD("get_agent_tool_name"), &AIV1MCPToolAdapter::get_agent_tool_name);
}

void AIV1MCPToolAdapter::setup(AIV1MCPService *p_service, const Dictionary &p_server_config, const Dictionary &p_descriptor) {
	service_id = p_service ? p_service->get_instance_id() : ObjectID();
	server_config = AIV1MCPService::_normalize_server_config(p_server_config);
	server_id = AIV1MCPService::_server_id_from_config(server_config);
	server_name = AIV1MCPService::_server_name_from_config(server_config);
	descriptor = AIV1MCPService::_normalize_tool_descriptor(server_id, server_name, p_descriptor);
	tool_name = String(descriptor.get("name", String())).strip_edges();
	agent_tool_name = AIV1MCPService::make_tool_name(server_id, tool_name);
	permission_default = AIV1MCPService::_permission_default_from_config(server_config);

	Dictionary tool_metadata = AIV1MCPService::_metadata_for_server_tool(server_config, descriptor, agent_tool_name);
	tool_metadata["action"] = "mcp." + server_id + "." + tool_name;
	configure(String(descriptor.get("description", "MCP tool " + tool_name)), Dictionary(descriptor.get("inputSchema", Dictionary())), Callable(), tool_metadata);
}

String AIV1MCPToolAdapter::get_server_id() const {
	return server_id;
}

String AIV1MCPToolAdapter::get_tool_name() const {
	return tool_name;
}

String AIV1MCPToolAdapter::get_agent_tool_name() const {
	return agent_tool_name;
}

bool AIV1MCPToolAdapter::execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) {
	AIV1MCPService *service = Object::cast_to<AIV1MCPService>(ObjectDB::get_instance(service_id));
	if (service == nullptr) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "MCP service is not available.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	const String action = "mcp." + server_id + "." + tool_name;
	const String resource = AIV1MCPService::_permission_resource_for_tool_call(server_id, tool_name, p_arguments);
	Dictionary source = p_context.source.duplicate(true);
	source["external"] = true;
	source["mcp_server_id"] = server_id;
	source["mcp_server_name"] = server_name;
	source["mcp_tool_name"] = tool_name;
	source["mcp_agent_tool_name"] = agent_tool_name;
	source["mcp_transport"] = AIV1MCPService::_transport_type_from_config(server_config);
	source["mcp_trust"] = String(server_config.get("trust", "workspace"));
	source["mcp_arguments"] = p_arguments.duplicate(true);
	source["mcp_arguments_preview"] = AIV1MCPService::_arguments_preview(p_arguments);
	source["mcp_arguments_hash"] = AIV1MCPService::_arguments_hash(p_arguments);

	if (p_context.permission_service.is_null()) {
		r_error = AIError::make(AI_ERROR_PERMISSION, "PermissionService is required for MCP tools.");
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary permission_input;
	permission_input["session_id"] = p_context.session_id;
	permission_input["action"] = action;
	permission_input["resource"] = resource;
	permission_input["reason"] = "Execute MCP tool `" + tool_name + "` from `" + server_name + "`.";
	permission_input["source"] = source;
	if (!permission_default.is_empty()) {
		permission_input["default_effect"] = permission_default;
	}

	AIPermissionDecision decision;
	if (!p_context.permission_service->assert_permission_struct(permission_input, decision, r_error)) {
		if (!r_error.is_error()) {
			r_error = decision.error.is_error() ? decision.error : AIError::make(AI_ERROR_PERMISSION, "MCP permission denied.");
		}
		r_result = AIV1ToolExecutionResult::fail(r_error);
		return false;
	}

	Dictionary call_result;
	if (!service->call_tool_struct(server_id, tool_name, p_arguments, call_result, r_error)) {
		Dictionary tool_metadata = AIV1MCPService::_metadata_for_server_tool(server_config, descriptor, agent_tool_name);
		tool_metadata["mcp_permission_action"] = action;
		tool_metadata["mcp_permission_resource"] = resource;
		r_result = AIV1ToolExecutionResult::fail(r_error);
		r_result.metadata = tool_metadata;
		return false;
	}

	PackedStringArray content_types;
	const String content = AIV1MCPService::_content_from_mcp_result(call_result.get("result", call_result), content_types);

	Dictionary structured;
	structured["output"] = content;
	structured["data"] = call_result.get("result", call_result);

	Dictionary tool_metadata = AIV1MCPService::_metadata_for_server_tool(server_config, descriptor, agent_tool_name);
	tool_metadata["mcp_permission_action"] = action;
	tool_metadata["mcp_permission_resource"] = resource;
	tool_metadata["mcp_original_content_types"] = content_types;
	if (call_result.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
		tool_metadata.merge(Dictionary(call_result["metadata"]), true);
	}
	structured["metadata"] = tool_metadata.duplicate(true);

	r_result = AIV1ToolExecutionResult::ok(structured, content, structured);
	r_result.metadata = tool_metadata;
	return true;
}

void AIV1MCPService::_bind_methods() {
	ClassDB::bind_static_method("AIV1MCPService", D_METHOD("sanitize_name_part", "text"), &AIV1MCPService::sanitize_name_part);
	ClassDB::bind_static_method("AIV1MCPService", D_METHOD("make_tool_name", "server_id", "tool_name"), &AIV1MCPService::make_tool_name);
	ClassDB::bind_method(D_METHOD("set_tool_registry", "tool_registry"), &AIV1MCPService::set_tool_registry);
	ClassDB::bind_method(D_METHOD("get_tool_registry"), &AIV1MCPService::get_tool_registry);
	ClassDB::bind_method(D_METHOD("register_server", "config"), &AIV1MCPService::register_server);
	ClassDB::bind_method(D_METHOD("import_config", "config"), &AIV1MCPService::import_config);
	ClassDB::bind_method(D_METHOD("clear"), &AIV1MCPService::clear);
	ClassDB::bind_method(D_METHOD("refresh"), &AIV1MCPService::refresh);
	ClassDB::bind_method(D_METHOD("get_statuses"), &AIV1MCPService::get_statuses);
	ClassDB::bind_method(D_METHOD("get_status_summary"), &AIV1MCPService::get_status_summary);
	ClassDB::bind_method(D_METHOD("get_discovery_snapshots"), &AIV1MCPService::get_discovery_snapshots);
	ClassDB::bind_method(D_METHOD("list_resources", "server_id"), &AIV1MCPService::list_resources, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("list_prompts", "server_id"), &AIV1MCPService::list_prompts, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("read_resource", "server_id", "uri"), &AIV1MCPService::read_resource);
	ClassDB::bind_method(D_METHOD("make_resource_context_source", "server_id", "uri", "required", "priority"), &AIV1MCPService::make_resource_context_source, DEFVAL(false), DEFVAL(300));
	ClassDB::bind_method(D_METHOD("render_prompt", "server_id", "prompt_name", "arguments"), &AIV1MCPService::render_prompt, DEFVAL(Dictionary()));

	ADD_SIGNAL(MethodInfo("status_changed", PropertyInfo(Variant::ARRAY, "statuses"), PropertyInfo(Variant::DICTIONARY, "summary")));
	ADD_SIGNAL(MethodInfo("tools_changed"));
}

AIV1MCPService::~AIV1MCPService() {
	clear();
}

Dictionary AIV1MCPService::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Array AIV1MCPService::_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

String AIV1MCPService::_server_id_from_config(const Dictionary &p_config) {
	return String(p_config.get("id", String())).strip_edges();
}

String AIV1MCPService::_server_name_from_config(const Dictionary &p_config) {
	const String name = String(p_config.get("name", p_config.get("display_name", p_config.get("displayName", String())))).strip_edges();
	return name.is_empty() ? _server_id_from_config(p_config) : name;
}

String AIV1MCPService::_transport_type_from_config(const Dictionary &p_config) {
	const Variant transport_value = p_config.get("transport", String());
	if (transport_value.get_type() == Variant::DICTIONARY) {
		return String(Dictionary(transport_value).get("type", "stdio")).strip_edges().to_lower();
	}
	return String(transport_value).strip_edges().to_lower();
}

String AIV1MCPService::_permission_default_from_config(const Dictionary &p_config) {
	String effect = String(p_config.get("permission_default", p_config.get("permissionDefault", String()))).strip_edges().to_lower();
	if (effect == "allow" || effect == "ask" || effect == "deny") {
		return effect;
	}
	return "ask";
}

String AIV1MCPService::_startup_permission_default_from_config(const Dictionary &p_config) {
	String effect = String(p_config.get("startup_permission_default", p_config.get("startupPermissionDefault", String()))).strip_edges().to_lower();
	if (effect == "allow" || effect == "ask" || effect == "deny") {
		return effect;
	}
	return "allow";
}

Dictionary AIV1MCPService::_normalize_server_config(const Dictionary &p_config) {
	Dictionary config = p_config.duplicate(true);
	const String server_id = _server_id_from_config(config);
	if (!server_id.is_empty()) {
		config["id"] = server_id;
	}
	config["name"] = _server_name_from_config(config);
	config["transport_type"] = _transport_type_from_config(config);
	config["permission_default"] = _permission_default_from_config(config);
	config["startup_permission_default"] = _startup_permission_default_from_config(config);
	if (!config.has("enabled")) {
		config["enabled"] = true;
	}
	if (!config.has("trust")) {
		config["trust"] = "workspace";
	}
	if (!config.has("tool_visibility")) {
		config["tool_visibility"] = config.get("toolVisibility", "model");
	}
	return config;
}

Dictionary AIV1MCPService::_normalize_tool_descriptor(const String &p_server_id, const String &p_server_name, const Dictionary &p_descriptor) {
	Dictionary descriptor = p_descriptor.duplicate(true);
	const String tool_name = String(descriptor.get("name", String())).strip_edges();
	descriptor["server_id"] = p_server_id;
	descriptor["server_name"] = p_server_name;
	descriptor["name"] = tool_name;
	if (!descriptor.has("description")) {
		descriptor["description"] = "MCP tool `" + tool_name + "` from `" + p_server_name + "`.";
	}
	if (!descriptor.has("inputSchema") || descriptor.get("inputSchema", Variant()).get_type() != Variant::DICTIONARY) {
		Dictionary schema;
		schema["type"] = "object";
		schema["properties"] = Dictionary();
		descriptor["inputSchema"] = schema;
	}
	return descriptor;
}

String AIV1MCPService::_content_from_mcp_result(const Variant &p_value, PackedStringArray &r_content_types) {
	r_content_types.clear();
	if (p_value.get_type() == Variant::NIL) {
		return String();
	}
	if (p_value.get_type() == Variant::STRING) {
		r_content_types.push_back("text");
		return String(p_value);
	}
	if (p_value.get_type() != Variant::DICTIONARY) {
		r_content_types.push_back(Variant::get_type_name(p_value.get_type()));
		return JSON::stringify(p_value);
	}

	const Dictionary result = p_value;
	if (result.has("content") && result["content"].get_type() == Variant::ARRAY) {
		Array content = result["content"];
		Vector<String> parts;
		for (int i = 0; i < content.size(); i++) {
			if (content[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary item = content[i];
			const String type = String(item.get("type", "unknown"));
			r_content_types.push_back(type);
			if (item.has("text")) {
				parts.push_back(String(item["text"]));
			} else {
				parts.push_back(JSON::stringify(item));
			}
		}
		return String("\n").join(parts);
	}
	if (result.has("content") && result["content"].get_type() == Variant::STRING) {
		r_content_types.push_back("text");
		return String(result["content"]);
	}
	if (result.has("text")) {
		r_content_types.push_back("text");
		return String(result["text"]);
	}
	if (result.has("output")) {
		r_content_types.push_back("text");
		return String(result["output"]);
	}

	r_content_types.push_back("json");
	return JSON::stringify(result);
}

Dictionary AIV1MCPService::_metadata_for_server_tool(const Dictionary &p_server_config, const Dictionary &p_descriptor, const String &p_agent_tool_name) {
	Dictionary tool_metadata;
	tool_metadata["tool_origin"] = "mcp";
	tool_metadata["mcp_server_id"] = _server_id_from_config(p_server_config);
	tool_metadata["mcp_server_name"] = _server_name_from_config(p_server_config);
	tool_metadata["mcp_tool_name"] = String(p_descriptor.get("name", String()));
	tool_metadata["mcp_transport"] = _transport_type_from_config(p_server_config);
	tool_metadata["mcp_agent_tool_name"] = p_agent_tool_name;
	tool_metadata["mcp_trust"] = String(p_server_config.get("trust", "workspace"));
	return tool_metadata;
}

String AIV1MCPService::_arguments_hash(const Dictionary &p_arguments) {
	const String json = JSON::stringify(p_arguments);
	const PackedByteArray bytes = json.to_utf8_buffer();
	unsigned char hash[32];
	const uint8_t empty = 0;
	const uint8_t *bytes_ptr = bytes.size() > 0 ? bytes.ptr() : &empty;
	if (CryptoCore::sha256(bytes_ptr, bytes.size(), hash) != OK) {
		return String::num_uint64(json.hash64(), 16);
	}
	return String::hex_encode_buffer(hash, 32);
}

String AIV1MCPService::_arguments_preview(const Dictionary &p_arguments, int p_max_chars) {
	String preview = JSON::stringify(p_arguments);
	if (preview.length() <= p_max_chars) {
		return preview;
	}
	return preview.substr(0, MAX(0, p_max_chars - 3)) + "...";
}

String AIV1MCPService::_permission_resource_for_tool_call(const String &p_server_id, const String &p_tool_name, const Dictionary &p_arguments) {
	return p_server_id + "/" + p_tool_name + "?args_hash=" + _arguments_hash(p_arguments) + "&args=" + _arguments_preview(p_arguments);
}

bool AIV1MCPService::_validate_server_config(const Dictionary &p_config, AIError &r_error) {
	const Dictionary config = _normalize_server_config(p_config);
	const String server_id = _server_id_from_config(config);
	if (server_id.is_empty() || !AIId::is_valid_name(sanitize_name_part(server_id), 128)) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "MCP server id is required.");
		return false;
	}
	r_error = AIError::none();
	return true;
}

String AIV1MCPService::sanitize_name_part(const String &p_text) {
	String sanitized;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
			sanitized += c;
		} else {
			sanitized += "_";
		}
	}
	while (sanitized.contains("__")) {
		sanitized = sanitized.replace("__", "_");
	}
	sanitized = sanitized.strip_edges().trim_prefix("_").trim_suffix("_");
	return sanitized.is_empty() ? String("mcp") : sanitized;
}

String AIV1MCPService::make_tool_name(const String &p_server_id, const String &p_tool_name) {
	return "mcp__" + sanitize_name_part(p_server_id) + "__" + sanitize_name_part(p_tool_name);
}

void AIV1MCPService::set_tool_registry(const Ref<AIV1ToolRegistry> &p_tool_registry) {
	MutexLock lock(mutex);
	tool_registry = p_tool_registry;
}

Ref<AIV1ToolRegistry> AIV1MCPService::get_tool_registry() const {
	MutexLock lock(mutex);
	return tool_registry;
}

bool AIV1MCPService::register_server_struct(const Dictionary &p_config, AIError &r_error) {
	const Dictionary config = _normalize_server_config(p_config);
	if (!_validate_server_config(config, r_error)) {
		return false;
	}
	const String server_id = _server_id_from_config(config);

	Vector<Ref<AIScopedRegistration>> registrations_to_close;
	{
		MutexLock lock(mutex);
		if (servers.has(server_id)) {
			registrations_to_close = servers[server_id].registrations;
		}

		ServerHandle handle;
		handle.config = config;
		handle.fake_server.unref();
		handle.state = bool(config.get("enabled", true)) ? String("stopped") : String("disabled");
		handle.last_error.clear();
		handle.tools.clear();
		handle.resources.clear();
		handle.prompts.clear();
		handle.discovered_at = 0;
		servers[server_id] = handle;
	}
	_close_registration_list(registrations_to_close);
	r_error = AIError::none();
	return true;
}

bool AIV1MCPService::register_fake_server_for_test(const Dictionary &p_config, const Ref<AIFakeMCPServer> &p_server) {
	if (p_server.is_null()) {
		return false;
	}
	AIError error;
	Dictionary config = _normalize_server_config(p_config);
	config["transport"] = "fake";
	config["transport_type"] = "fake";
	if (!register_server_struct(config, error)) {
		return false;
	}
	const String server_id = _server_id_from_config(config);
	MutexLock lock(mutex);
	servers[server_id].fake_server = p_server;
	return true;
}

Dictionary AIV1MCPService::register_server(const Dictionary &p_config) {
	AIError error;
	const bool ok = register_server_struct(p_config, error);
	Dictionary result;
	result["success"] = ok && !error.is_error();
	if (!bool(result["success"])) {
		result["error"] = error.to_dictionary();
	}
	return result;
}

bool AIV1MCPService::import_config_struct(const Dictionary &p_config, AIError &r_error) {
	Dictionary mcp_config = p_config;
	if (p_config.get("mcp", Variant()).get_type() == Variant::DICTIONARY) {
		mcp_config = Dictionary(p_config["mcp"]).duplicate(true);
	}

	const Variant servers_value = mcp_config.get("servers", Variant());
	Array server_configs;
	if (servers_value.get_type() == Variant::DICTIONARY) {
		Dictionary configured_servers = servers_value;
		for (const KeyValue<Variant, Variant> &kv : configured_servers) {
			if (kv.value.get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary server_config = Dictionary(kv.value).duplicate(true);
			if (!server_config.has("id")) {
				server_config["id"] = String(kv.key);
			}
			server_configs.push_back(server_config);
		}
	} else if (servers_value.get_type() == Variant::ARRAY) {
		server_configs = Array(servers_value).duplicate(true);
	} else if (mcp_config.has("id")) {
		server_configs.push_back(mcp_config);
	}

	Vector<Dictionary> normalized_configs;
	for (int i = 0; i < server_configs.size(); i++) {
		if (server_configs[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		const Dictionary normalized = _normalize_server_config(server_configs[i]);
		if (!_validate_server_config(normalized, r_error)) {
			return false;
		}
		normalized_configs.push_back(normalized);
	}

	Vector<Ref<AIScopedRegistration>> registrations_to_close;
	{
		MutexLock lock(mutex);
		for (KeyValue<String, ServerHandle> &kv : servers) {
			for (int i = 0; i < kv.value.registrations.size(); i++) {
				registrations_to_close.push_back(kv.value.registrations[i]);
			}
		}
		servers.clear();
		for (int i = 0; i < normalized_configs.size(); i++) {
			const Dictionary config = normalized_configs[i];
			const String server_id = _server_id_from_config(config);
			ServerHandle handle;
			handle.config = config;
			handle.state = bool(config.get("enabled", true)) ? String("stopped") : String("disabled");
			servers[server_id] = handle;
		}
	}
	_close_registration_list(registrations_to_close);
	r_error = AIError::none();
	return true;
}

Dictionary AIV1MCPService::import_config(const Dictionary &p_config) {
	AIError error;
	const bool ok = import_config_struct(p_config, error);
	Dictionary result;
	result["success"] = ok && !error.is_error();
	if (!bool(result["success"])) {
		result["error"] = error.to_dictionary();
	}
	result["statuses"] = get_statuses();
	return result;
}

void AIV1MCPService::_close_registration_list(Vector<Ref<AIScopedRegistration>> &r_registrations) {
	for (int i = 0; i < r_registrations.size(); i++) {
		if (r_registrations[i].is_valid()) {
			r_registrations.write[i]->close();
		}
	}
	r_registrations.clear();
}

void AIV1MCPService::_close_registrations(ServerHandle &r_handle) const {
	Vector<Ref<AIScopedRegistration>> registrations = r_handle.registrations;
	_close_registration_list(registrations);
	r_handle.registrations.clear();
}

void AIV1MCPService::clear() {
	Vector<Ref<AIScopedRegistration>> registrations_to_close;
	{
		MutexLock lock(mutex);
		for (KeyValue<String, ServerHandle> &kv : servers) {
			for (int i = 0; i < kv.value.registrations.size(); i++) {
				registrations_to_close.push_back(kv.value.registrations[i]);
			}
		}
		servers.clear();
	}
	_close_registration_list(registrations_to_close);
}

bool AIV1MCPService::_register_discovered_tools(const String &p_server_id, ServerHandle &r_handle, AIError &r_error) {
	_close_registrations(r_handle);
	Ref<AIV1ToolRegistry> registry = get_tool_registry();
	if (registry.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ToolRegistry is required for MCP tool registration.");
		return false;
	}
	if (String(r_handle.config.get("tool_visibility", "model")) == "hidden") {
		r_error = AIError::none();
		return true;
	}

	for (int i = 0; i < r_handle.tools.size(); i++) {
		if (r_handle.tools[i].get_type() != Variant::DICTIONARY) {
			continue;
		}

		Dictionary descriptor = _normalize_tool_descriptor(p_server_id, _server_name_from_config(r_handle.config), r_handle.tools[i]);
		const String source_tool_name = String(descriptor.get("name", String())).strip_edges();
		if (source_tool_name.is_empty()) {
			continue;
		}
		const String agent_tool_name = make_tool_name(p_server_id, source_tool_name);

		Ref<AIV1MCPToolAdapter> tool;
		tool.instantiate();
		tool->setup(this, r_handle.config, descriptor);
		Dictionary tool_metadata = _metadata_for_server_tool(r_handle.config, descriptor, agent_tool_name);
		tool_metadata["source"] = "mcp";
		Ref<AIScopedRegistration> scope = registry->register_tool_scope_struct(agent_tool_name, tool, "mcp", tool_metadata, &r_error);
		if (scope.is_null()) {
			_close_registrations(r_handle);
			return false;
		}
		r_handle.registrations.push_back(scope);
	}
	r_error = AIError::none();
	return true;
}

bool AIV1MCPService::_refresh_fake_server(const String &p_server_id, ServerHandle &r_handle, AIError &r_error) {
	if (r_handle.fake_server.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Fake MCP server is not available.");
		return false;
	}
	if (!r_handle.fake_server->is_running()) {
		r_handle.state = "failed";
		r_handle.last_error = "MCP server is not running.";
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, r_handle.last_error);
		return false;
	}

	r_handle.tools = r_handle.fake_server->list_tools();
	r_handle.resources = r_handle.fake_server->list_resources();
	r_handle.prompts = r_handle.fake_server->list_prompts();
	r_handle.discovered_at = OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : 0;
	r_handle.state = "ready";
	r_handle.last_error.clear();
	return _register_discovered_tools(p_server_id, r_handle, r_error);
}

static AIMCPServerConfig _aiv1_legacy_mcp_config_from_dictionary(const Dictionary &p_config) {
	AIMCPServerConfig config;
	config.id = String(p_config.get("id", String())).strip_edges();
	config.display_name = String(p_config.get("name", p_config.get("display_name", p_config.get("displayName", config.id)))).strip_edges();
	if (config.display_name.is_empty()) {
		config.display_name = config.id;
	}
	config.enabled = bool(p_config.get("enabled", true));
	const Variant transport_value = p_config.get("transport", Variant());
	if (transport_value.get_type() == Variant::DICTIONARY) {
		const Dictionary transport = transport_value;
		config.transport = String(transport.get("type", "stdio")).strip_edges().to_lower();
		if (transport.has("command") && transport["command"].get_type() == Variant::ARRAY) {
			Array command = transport["command"];
			if (command.size() > 0) {
				config.command = String(command[0]);
				Vector<String> args;
				for (int i = 1; i < command.size(); i++) {
					args.push_back(String(command[i]));
				}
				config.arguments = String(" ").join(args);
			}
		} else {
			config.command = String(transport.get("command", String()));
			config.arguments = String(transport.get("arguments", String()));
		}
		config.working_directory = String(transport.get("cwd", transport.get("working_directory", String())));
		config.environment = String(transport.get("env", transport.get("environment", String())));
		config.url = String(transport.get("url", String()));
		config.headers = String(transport.get("headers", String()));
	} else {
		config.transport = String(transport_value).strip_edges().to_lower();
		config.command = String(p_config.get("command", String()));
		config.arguments = String(p_config.get("arguments", String()));
		config.working_directory = String(p_config.get("cwd", p_config.get("working_directory", String())));
		config.environment = String(p_config.get("env", p_config.get("environment", String())));
		config.url = String(p_config.get("url", String()));
		config.headers = String(p_config.get("headers", String()));
	}
	if (config.transport == "http") {
		config.transport = "streamable_http";
	}
	return config;
}

static Dictionary _aiv1_dictionary_from_legacy_mcp_tool(const AIMCPToolDescriptor &p_tool) {
	Dictionary descriptor;
	descriptor["server_id"] = p_tool.server_id;
	descriptor["server_name"] = p_tool.server_name;
	descriptor["name"] = p_tool.name;
	descriptor["display_name"] = p_tool.display_name;
	descriptor["description"] = p_tool.description;
	descriptor["inputSchema"] = p_tool.input_schema;
	return descriptor;
}

static Dictionary _aiv1_dictionary_from_legacy_mcp_resource(const AIMCPResourceDescriptor &p_resource) {
	Dictionary descriptor = p_resource.metadata.duplicate(true);
	descriptor["server_id"] = p_resource.server_id;
	descriptor["server_name"] = p_resource.server_name;
	descriptor["uri"] = p_resource.uri;
	descriptor["name"] = p_resource.name;
	descriptor["description"] = p_resource.description;
	if (!p_resource.mime_type.is_empty()) {
		descriptor["mimeType"] = p_resource.mime_type;
		descriptor["mime"] = p_resource.mime_type;
	}
	return descriptor;
}

static Dictionary _aiv1_dictionary_from_legacy_mcp_prompt(const AIMCPPromptDescriptor &p_prompt) {
	Dictionary descriptor = p_prompt.metadata.duplicate(true);
	descriptor["server_id"] = p_prompt.server_id;
	descriptor["server_name"] = p_prompt.server_name;
	descriptor["name"] = p_prompt.name;
	descriptor["description"] = p_prompt.description;
	descriptor["arguments"] = p_prompt.arguments.duplicate(true);
	return descriptor;
}

bool AIV1MCPService::_assert_configured_server_start_permission(const String &p_server_id, ServerHandle &r_handle, AIError &r_error) const {
	Ref<AIV1ToolRegistry> registry = get_tool_registry();
	if (registry.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ToolRegistry is required for MCP server startup permission.");
		return false;
	}
	Ref<AIPermissionService> permission_service = registry->get_permission_service();
	if (permission_service.is_null()) {
		r_error = AIError::make(AI_ERROR_PERMISSION, "PermissionService is required for MCP server startup.");
		return false;
	}

	const String transport = _transport_type_from_config(r_handle.config);
	const String endpoint = transport == "streamable_http" || transport == "sse" ? String(r_handle.config.get("url", String())) : String(r_handle.config.get("command", String()));
	const String resource = p_server_id + "/" + transport + "/" + endpoint;

	Dictionary source;
	source["external"] = true;
	source["mcp_server_id"] = p_server_id;
	source["mcp_server_name"] = _server_name_from_config(r_handle.config);
	source["mcp_transport"] = transport;
	source["mcp_trust"] = String(r_handle.config.get("trust", "workspace"));
	if (!endpoint.is_empty()) {
		source["mcp_endpoint"] = endpoint;
	}

	Dictionary permission_input;
	permission_input["action"] = "mcp.server.start";
	permission_input["resource"] = resource;
	permission_input["reason"] = "Start MCP server `" + _server_name_from_config(r_handle.config) + "`.";
	permission_input["source"] = source;
	permission_input["default_effect"] = _startup_permission_default_from_config(r_handle.config);

	AIPermissionDecision decision;
	if (!permission_service->assert_permission_struct(permission_input, decision, r_error)) {
		r_handle.state = decision.pending ? String("permission_pending") : String("failed");
		r_handle.last_error = r_error.message;
		return false;
	}
	r_error = AIError::none();
	return true;
}

void AIV1MCPService::_commit_server_status_from_handle(const String &p_server_id, const ServerHandle &p_handle) {
	MutexLock lock(mutex);
	if (!servers.has(p_server_id)) {
		return;
	}
	servers[p_server_id].state = p_handle.state;
	servers[p_server_id].last_error = p_handle.last_error;
}

bool AIV1MCPService::_refresh_configured_server(const String &p_server_id, ServerHandle &r_handle, AIError &r_error) {
	if (!_assert_configured_server_start_permission(p_server_id, r_handle, r_error)) {
		return false;
	}

	AIMCPServerConfig legacy_config = _aiv1_legacy_mcp_config_from_dictionary(r_handle.config);
	Ref<AIMCPClient> client = AIMCPClientFactory::create_client(legacy_config);
	if (client.is_null()) {
		r_handle.state = "failed";
		r_handle.last_error = "MCP client transport is not available.";
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, r_handle.last_error);
		return false;
	}
	client->set_timeout_msec(int(r_handle.config.get("timeout_msec", r_handle.config.get("timeoutMsec", 3000))));

	Vector<AIMCPToolDescriptor> legacy_tools;
	String error;
	if (!client->list_tools(legacy_tools, error)) {
		r_handle.state = "failed";
		r_handle.last_error = error;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, error);
		return false;
	}

	Array tools;
	for (int i = 0; i < legacy_tools.size(); i++) {
		tools.push_back(_aiv1_dictionary_from_legacy_mcp_tool(legacy_tools[i]));
	}
	Vector<AIMCPResourceDescriptor> legacy_resources;
	Array resources;
	String resource_error;
	if (client->list_resources(legacy_resources, resource_error)) {
		for (int i = 0; i < legacy_resources.size(); i++) {
			resources.push_back(_aiv1_dictionary_from_legacy_mcp_resource(legacy_resources[i]));
		}
	}
	Vector<AIMCPPromptDescriptor> legacy_prompts;
	Array prompts;
	String prompt_error;
	if (client->list_prompts(legacy_prompts, prompt_error)) {
		for (int i = 0; i < legacy_prompts.size(); i++) {
			prompts.push_back(_aiv1_dictionary_from_legacy_mcp_prompt(legacy_prompts[i]));
		}
	}
	r_handle.tools = tools;
	r_handle.resources = resources;
	r_handle.prompts = prompts;
	r_handle.discovered_at = OS::get_singleton() ? OS::get_singleton()->get_ticks_msec() : 0;
	r_handle.state = "ready";
	r_handle.last_error.clear();
	return _register_discovered_tools(p_server_id, r_handle, r_error);
}

bool AIV1MCPService::refresh_struct(AIError &r_error) {
	bool ok = true;
	Vector<String> server_ids;
	{
		MutexLock lock(mutex);
		for (const KeyValue<String, ServerHandle> &kv : servers) {
			server_ids.push_back(kv.key);
		}
	}

	for (int i = 0; i < server_ids.size(); i++) {
		const String server_id = server_ids[i];
		ServerHandle handle;
		bool found = false;
		bool disabled = false;
		Vector<Ref<AIScopedRegistration>> disabled_registrations;
		{
			MutexLock lock(mutex);
			if (!servers.has(server_id)) {
				continue;
			}
			handle = servers[server_id];
			found = true;
			disabled = !bool(handle.config.get("enabled", true));
			if (disabled) {
				disabled_registrations = handle.registrations;
				servers[server_id].registrations.clear();
				servers[server_id].state = "disabled";
			}
		}
		if (!found) {
			continue;
		}
		if (disabled) {
			_close_registration_list(disabled_registrations);
			continue;
		}

		AIError server_error;
		const bool server_ok = handle.fake_server.is_valid() ? _refresh_fake_server(server_id, handle, server_error) : _refresh_configured_server(server_id, handle, server_error);
		if (!server_ok) {
			ok = false;
			if (handle.state == "ready" || handle.state.is_empty()) {
				handle.state = "failed";
				handle.last_error = server_error.message;
			}
			if (!r_error.is_error()) {
				r_error = server_error;
			}
		}

		bool committed = false;
		{
			MutexLock lock(mutex);
			if (servers.has(server_id)) {
				servers[server_id] = handle;
				committed = true;
			}
		}
		if (!committed) {
			_close_registrations(handle);
		}
	}
	call_deferred("emit_signal", SNAME("status_changed"), get_statuses(), get_status_summary());
	call_deferred("emit_signal", SNAME("tools_changed"));
	if (!ok) {
		return false;
	}
	r_error = AIError::none();
	return true;
}

Dictionary AIV1MCPService::refresh() {
	AIError error;
	const bool ok = refresh_struct(error);
	Dictionary result;
	result["success"] = ok && !error.is_error();
	if (!bool(result["success"])) {
		result["error"] = error.to_dictionary();
	}
	result["statuses"] = get_statuses();
	return result;
}

Array AIV1MCPService::get_statuses() const {
	MutexLock lock(mutex);
	Array statuses;
	for (const KeyValue<String, ServerHandle> &kv : servers) {
		Dictionary status;
		status["server_id"] = kv.key;
		status["server_name"] = _server_name_from_config(kv.value.config);
		status["state"] = kv.value.state;
		status["last_error"] = kv.value.last_error;
		status["tool_count"] = kv.value.tools.size();
		status["resource_count"] = kv.value.resources.size();
		status["prompt_count"] = kv.value.prompts.size();
		statuses.push_back(status);
	}
	return statuses;
}

Dictionary AIV1MCPService::get_status_summary() const {
	Array statuses = get_statuses();
	Dictionary summary;
	summary["total"] = statuses.size();
	int ready = 0;
	int failed = 0;
	int disabled = 0;
	for (int i = 0; i < statuses.size(); i++) {
		const String state = String(Dictionary(statuses[i]).get("state", String()));
		if (state == "ready") {
			ready++;
		} else if (state == "failed") {
			failed++;
		} else if (state == "disabled") {
			disabled++;
		}
	}
	summary["ready"] = ready;
	summary["failed"] = failed;
	summary["disabled"] = disabled;
	return summary;
}

Array AIV1MCPService::get_discovery_snapshots() const {
	MutexLock lock(mutex);
	Array snapshots;
	for (const KeyValue<String, ServerHandle> &kv : servers) {
		Dictionary snapshot;
		snapshot["server_id"] = kv.key;
		snapshot["server_name"] = _server_name_from_config(kv.value.config);
		snapshot["state"] = kv.value.state;
		snapshot["last_error"] = kv.value.last_error;
		snapshot["tools"] = kv.value.tools.duplicate(true);
		snapshot["resources"] = kv.value.resources.duplicate(true);
		snapshot["prompts"] = kv.value.prompts.duplicate(true);
		snapshot["discovered_at"] = static_cast<int64_t>(kv.value.discovered_at);
		snapshots.push_back(snapshot);
	}
	return snapshots;
}

Array AIV1MCPService::list_resources(const String &p_server_id) const {
	MutexLock lock(mutex);
	const String server_id = p_server_id.strip_edges();
	Array resources;
	for (const KeyValue<String, ServerHandle> &kv : servers) {
		if (!server_id.is_empty() && kv.key != server_id) {
			continue;
		}
		for (int i = 0; i < kv.value.resources.size(); i++) {
			if (kv.value.resources[i].get_type() == Variant::DICTIONARY) {
				Dictionary resource = Dictionary(kv.value.resources[i]).duplicate(true);
				resource["server_id"] = kv.key;
				resource["server_name"] = _server_name_from_config(kv.value.config);
				resources.push_back(resource);
			}
		}
	}
	return resources;
}

Array AIV1MCPService::list_prompts(const String &p_server_id) const {
	MutexLock lock(mutex);
	const String server_id = p_server_id.strip_edges();
	Array prompts;
	for (const KeyValue<String, ServerHandle> &kv : servers) {
		if (!server_id.is_empty() && kv.key != server_id) {
			continue;
		}
		for (int i = 0; i < kv.value.prompts.size(); i++) {
			if (kv.value.prompts[i].get_type() == Variant::DICTIONARY) {
				Dictionary prompt = Dictionary(kv.value.prompts[i]).duplicate(true);
				prompt["server_id"] = kv.key;
				prompt["server_name"] = _server_name_from_config(kv.value.config);
				prompts.push_back(prompt);
			}
		}
	}
	return prompts;
}

Dictionary AIV1MCPService::read_resource(const String &p_server_id, const String &p_uri) {
	const String server_id = p_server_id.strip_edges();
	const String uri = p_uri.strip_edges();
	Dictionary result;
	if (server_id.is_empty() || uri.is_empty()) {
		result["success"] = false;
		result["error"] = AIError::make(AI_ERROR_VALIDATION, "MCP resource server_id and uri are required.").to_dictionary();
		return result;
	}

	Ref<AIFakeMCPServer> fake_server;
	ServerHandle handle;
	{
		MutexLock lock(mutex);
		if (!servers.has(server_id)) {
			result["success"] = false;
			result["error"] = AIError::make(AI_ERROR_UNAVAILABLE, "MCP server is not registered: " + server_id).to_dictionary();
			return result;
		}
		handle = servers[server_id];
		fake_server = handle.fake_server;
	}

	if (fake_server.is_valid()) {
		result = fake_server->read_resource(uri);
		if (!bool(result.get("success", false))) {
			AIError error = AIError::make(AI_ERROR_UNAVAILABLE, String(result.get("error", "Failed to read MCP resource.")));
			result["error"] = error.to_dictionary();
			return result;
		}
		result["server_id"] = server_id;
		return result;
	}

	AIError permission_error;
	if (!_assert_configured_server_start_permission(server_id, handle, permission_error)) {
		_commit_server_status_from_handle(server_id, handle);
		result["success"] = false;
		result["error"] = permission_error.to_dictionary();
		return result;
	}

	Dictionary server_config = handle.config.duplicate(true);
	AIMCPServerConfig legacy_config = _aiv1_legacy_mcp_config_from_dictionary(server_config);
	Ref<AIMCPClient> client = AIMCPClientFactory::create_client(legacy_config);
	if (client.is_null()) {
		result["success"] = false;
		result["error"] = AIError::make(AI_ERROR_UNAVAILABLE, "MCP client transport is not available.").to_dictionary();
		return result;
	}
	client->set_timeout_msec(int(server_config.get("timeout_msec", server_config.get("timeoutMsec", 30000))));
	AIMCPResourceReadResult read_result = client->read_resource(uri);
	if (!read_result.success) {
		AIError error = AIError::make(AI_ERROR_UNAVAILABLE, read_result.error.is_empty() ? String("Failed to read MCP resource.") : read_result.error);
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	result["success"] = true;
	result["uri"] = read_result.uri.is_empty() ? uri : read_result.uri;
	result["mime"] = read_result.mime;
	result["mimeType"] = read_result.mime;
	result["text"] = read_result.text;
	result["content"] = read_result.content;
	result["metadata"] = read_result.metadata.duplicate(true);
	result["server_id"] = server_id;
	return result;
}

Dictionary AIV1MCPService::make_resource_context_source(const String &p_server_id, const String &p_uri, bool p_required, int p_priority) {
	Dictionary read = read_resource(p_server_id, p_uri);
	Dictionary result;
	if (!bool(read.get("success", false))) {
		result["success"] = false;
		result["error"] = read.get("error", AIError::make(AI_ERROR_UNAVAILABLE, "Failed to read MCP resource.").to_dictionary());
		return result;
	}

	const String text = String(read.get("text", read.get("content", String())));
	Dictionary source_metadata;
	source_metadata["source"] = "mcp";
	source_metadata["mcp_server_id"] = p_server_id;
	source_metadata["mcp_uri"] = p_uri;
	source_metadata["mime"] = read.get("mime", read.get("mimeType", "text/plain"));

	AISystemContextSource source;
	source.domain = "mcp.resource/" + p_server_id;
	source.text = text;
	source.required = p_required;
	source.available = true;
	source.priority = p_priority;
	source.metadata = source_metadata;

	result["success"] = true;
	result["source"] = source.to_dictionary();
	return result;
}

Dictionary AIV1MCPService::render_prompt(const String &p_server_id, const String &p_prompt_name, const Dictionary &p_arguments) {
	const String server_id = p_server_id.strip_edges();
	const String prompt_name = p_prompt_name.strip_edges();
	Dictionary result;
	if (server_id.is_empty() || prompt_name.is_empty()) {
		result["success"] = false;
		result["error"] = AIError::make(AI_ERROR_VALIDATION, "MCP prompt server_id and prompt_name are required.").to_dictionary();
		return result;
	}

	Ref<AIFakeMCPServer> fake_server;
	ServerHandle handle;
	{
		MutexLock lock(mutex);
		if (!servers.has(server_id)) {
			result["success"] = false;
			result["error"] = AIError::make(AI_ERROR_UNAVAILABLE, "MCP server is not registered: " + server_id).to_dictionary();
			return result;
		}
		handle = servers[server_id];
		fake_server = handle.fake_server;
	}
	if (fake_server.is_valid()) {
		result = fake_server->render_prompt(prompt_name, p_arguments);
		if (bool(result.get("success", false))) {
			result["server_id"] = server_id;
		}
		return result;
	}

	AIError permission_error;
	if (!_assert_configured_server_start_permission(server_id, handle, permission_error)) {
		_commit_server_status_from_handle(server_id, handle);
		result["success"] = false;
		result["error"] = permission_error.to_dictionary();
		return result;
	}

	Dictionary server_config = handle.config.duplicate(true);
	AIMCPServerConfig legacy_config = _aiv1_legacy_mcp_config_from_dictionary(server_config);
	Ref<AIMCPClient> client = AIMCPClientFactory::create_client(legacy_config);
	if (client.is_null()) {
		result["success"] = false;
		result["error"] = AIError::make(AI_ERROR_UNAVAILABLE, "MCP client transport is not available.").to_dictionary();
		return result;
	}
	client->set_timeout_msec(int(server_config.get("timeout_msec", server_config.get("timeoutMsec", 30000))));
	AIMCPPromptRenderResult prompt_result = client->render_prompt(prompt_name, p_arguments);
	if (!prompt_result.success) {
		result["success"] = false;
		result["error"] = AIError::make(AI_ERROR_UNAVAILABLE, prompt_result.error.is_empty() ? String("Failed to render MCP prompt.") : prompt_result.error).to_dictionary();
		return result;
	}
	result["success"] = true;
	result["name"] = prompt_result.name.is_empty() ? prompt_name : prompt_result.name;
	result["messages"] = prompt_result.messages.duplicate(true);
	result["metadata"] = prompt_result.metadata.duplicate(true);
	result["server_id"] = server_id;
	return result;
}

bool AIV1MCPService::_call_fake_tool(const String &p_server_id, const String &p_tool_name, const Dictionary &p_arguments, Dictionary &r_result, AIError &r_error) const {
	Ref<AIFakeMCPServer> fake_server;
	{
		MutexLock lock(mutex);
		if (!servers.has(p_server_id)) {
			r_error = AIError::make(AI_ERROR_UNAVAILABLE, "MCP server is not registered: " + p_server_id);
			return false;
		}
		fake_server = servers[p_server_id].fake_server;
	}
	if (fake_server.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "MCP fake server is not available.");
		return false;
	}
	r_result = fake_server->call_tool(p_tool_name, p_arguments);
	if (!bool(r_result.get("success", false))) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, String(r_result.get("error", "MCP tool call failed.")));
		return false;
	}
	r_error = AIError::none();
	return true;
}

bool AIV1MCPService::_call_configured_tool(const String &p_server_id, const String &p_tool_name, const Dictionary &p_arguments, Dictionary &r_result, AIError &r_error) {
	ServerHandle handle;
	{
		MutexLock lock(mutex);
		if (!servers.has(p_server_id)) {
			r_error = AIError::make(AI_ERROR_UNAVAILABLE, "MCP server is not registered: " + p_server_id);
			return false;
		}
		handle = servers[p_server_id];
	}

	if (!_assert_configured_server_start_permission(p_server_id, handle, r_error)) {
		_commit_server_status_from_handle(p_server_id, handle);
		return false;
	}

	Dictionary server_config = handle.config.duplicate(true);
	AIMCPServerConfig legacy_config = _aiv1_legacy_mcp_config_from_dictionary(server_config);
	Ref<AIMCPClient> client = AIMCPClientFactory::create_client(legacy_config);
	if (client.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "MCP client transport is not available.");
		return false;
	}
	client->set_timeout_msec(int(server_config.get("timeout_msec", server_config.get("timeoutMsec", 30000))));
	AIMCPToolCallResult call_result = client->call_tool(p_tool_name, p_arguments);
	if (!call_result.error.is_empty() || !call_result.success) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, call_result.error.is_empty() ? String("MCP tool call failed.") : call_result.error);
		return false;
	}
	Dictionary result;
	result["success"] = true;
	Dictionary payload;
	payload["content"] = call_result.content;
	result["result"] = payload;
	result["metadata"] = call_result.metadata;
	r_result = result;
	r_error = AIError::none();
	return true;
}

bool AIV1MCPService::call_tool_struct(const String &p_server_id, const String &p_tool_name, const Dictionary &p_arguments, Dictionary &r_result, AIError &r_error) {
	const String server_id = p_server_id.strip_edges();
	const String tool_name = p_tool_name.strip_edges();
	if (server_id.is_empty() || tool_name.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "MCP tool server_id and tool_name are required.");
		return false;
	}

	bool fake = false;
	{
		MutexLock lock(mutex);
		if (!servers.has(server_id)) {
			r_error = AIError::make(AI_ERROR_UNAVAILABLE, "MCP server is not registered: " + server_id);
			return false;
		}
		fake = servers[server_id].fake_server.is_valid();
	}
	return fake ? _call_fake_tool(server_id, tool_name, p_arguments, r_result, r_error) : _call_configured_tool(server_id, tool_name, p_arguments, r_result, r_error);
}
