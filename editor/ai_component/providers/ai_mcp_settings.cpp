/**************************************************************************/
/*  ai_mcp_settings.cpp                                                   */
/**************************************************************************/

#include "ai_mcp_settings.h"

#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "editor/settings/editor_settings.h"

namespace {

bool _is_stale_sample_server(const Dictionary &p_server) {
	const String display_name = String(p_server.get("display_name", String())).strip_edges().to_lower();
	const String transport = String(p_server.get("transport", "stdio")).strip_edges().to_lower().replace("-", "_");
	const String command = String(p_server.get("command", String())).strip_edges();
	const String arguments = String(p_server.get("arguments", String())).strip_edges();
	const String working_directory = String(p_server.get("working_directory", String())).strip_edges();
	const String environment = String(p_server.get("environment", String())).strip_edges();
	const String url = String(p_server.get("url", String())).strip_edges();
	const String headers = String(p_server.get("headers", String())).strip_edges();

	const bool filesystem_sample = (display_name == "filesystem" || display_name == "file system") &&
			transport == "stdio" &&
			command == "npx" &&
			arguments == "-y @modelcontextprotocol/server-filesystem ." &&
			(working_directory.is_empty() || working_directory == "res://") &&
			(environment.is_empty() || environment == "NODE_ENV=development");

	const bool remote_docs_sample = display_name == "remote-docs" &&
			(transport == "streamable_http" || transport == "http") &&
			url == "https://mcp.example.test/mcp" &&
			(headers.is_empty() || headers == "Authorization=Bearer test-token");

	const bool legacy_events_sample = display_name == "legacy-events" &&
			transport == "sse" &&
			url == "https://mcp.example.test/sse";

	return filesystem_sample || remote_docs_sample || legacy_events_sample;
}

Array _remove_stale_sample_servers(const Array &p_servers, bool &r_changed) {
	Array filtered_servers;
	r_changed = false;
	for (int i = 0; i < p_servers.size(); i++) {
		const Variant server_value = p_servers[i];
		if (server_value.get_type() == Variant::DICTIONARY && _is_stale_sample_server(server_value)) {
			r_changed = true;
			continue;
		}
		filtered_servers.push_back(server_value);
	}
	return filtered_servers;
}

} // namespace

String AIMCPSettings::_get_servers_path() {
	return "ai_agent/mcp_servers";
}

Array AIMCPSettings::_get_server_storage() {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return Array();
	}

	const String path = _get_servers_path();
	if (!settings->has_setting(path)) {
		return Array();
	}

	Variant value = settings->get(path);
	if (value.get_type() != Variant::ARRAY) {
		return Array();
	}

	bool changed = false;
	Array servers = _remove_stale_sample_servers(value, changed);
	if (changed) {
		_set_server_storage(servers);
	}
	return servers;
}

void AIMCPSettings::_set_server_storage(const Array &p_servers) {
	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL(settings);
	settings->set(_get_servers_path(), p_servers);
}

AIMCPServerConfig AIMCPSettings::_server_from_dictionary(const Dictionary &p_server) {
	AIMCPServerConfig server;
	server.id = String(p_server.get("id", String()));
	server.display_name = String(p_server.get("display_name", String()));
	server.transport = _normalize_transport(String(p_server.get("transport", "stdio")));
	server.command = String(p_server.get("command", String()));
	server.arguments = String(p_server.get("arguments", String()));
	server.working_directory = String(p_server.get("working_directory", String()));
	server.environment = String(p_server.get("environment", String()));
	server.url = String(p_server.get("url", String()));
	server.headers = String(p_server.get("headers", String()));
	server.enabled = bool(p_server.get("enabled", true));
	if (server.display_name.is_empty()) {
		server.display_name = server.transport == "stdio" ? server.command : server.url;
	}
	return server;
}

Dictionary AIMCPSettings::_server_to_dictionary(const AIMCPServerConfig &p_server) {
	Dictionary server;
	server["id"] = p_server.id;
	server["display_name"] = p_server.display_name;
	server["transport"] = _normalize_transport(p_server.transport);
	server["command"] = p_server.command;
	server["arguments"] = p_server.arguments;
	server["working_directory"] = p_server.working_directory;
	server["environment"] = p_server.environment;
	server["url"] = p_server.url;
	server["headers"] = p_server.headers;
	server["enabled"] = p_server.enabled;
	return server;
}

String AIMCPSettings::_make_server_id(const String &p_display_name) {
	String name = p_display_name.strip_edges().validate_node_name().replace(" ", "_").to_lower();
	if (name.is_empty()) {
		name = "server";
	}
	return "mcp:" + name + ":" + String::num_uint64(OS::get_singleton()->get_ticks_usec()) + ":" + itos(Math::rand());
}

String AIMCPSettings::_normalize_transport(const String &p_transport) {
	String transport = p_transport.strip_edges().to_lower();
	transport = transport.replace("-", "_");
	if (transport == "http" || transport == "streamablehttp" || transport == "streamable_http") {
		return "streamable_http";
	}
	if (transport == "sse") {
		return "sse";
	}
	return "stdio";
}

String AIMCPSettings::_string_array_to_arguments(const Array &p_array) {
	Vector<String> arguments;
	for (int i = 0; i < p_array.size(); i++) {
		const String argument = String(p_array[i]).strip_edges();
		if (argument.is_empty()) {
			continue;
		}
		if (argument.contains(" ") || argument.contains("\t") || argument.contains("\"")) {
			arguments.push_back("\"" + argument.replace("\"", "\\\"") + "\"");
		} else {
			arguments.push_back(argument);
		}
	}
	return String(" ").join(arguments);
}

String AIMCPSettings::_dictionary_to_env_lines(const Dictionary &p_dictionary) {
	Vector<String> lines;
	Array keys = p_dictionary.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = String(keys[i]).strip_edges();
		if (key.is_empty()) {
			continue;
		}
		lines.push_back(key + "=" + String(p_dictionary[key]));
	}
	return String("\n").join(lines);
}

String AIMCPSettings::_dictionary_to_header_lines(const Dictionary &p_dictionary) {
	Vector<String> lines;
	Array keys = p_dictionary.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = String(keys[i]).strip_edges();
		if (key.is_empty()) {
			continue;
		}
		lines.push_back(key + "=" + String(p_dictionary[key]));
	}
	return String("\n").join(lines);
}

bool AIMCPSettings::_append_server_from_dictionary(const String &p_name, const Dictionary &p_source, Array &r_servers, String &r_error) {
	AIMCPServerConfig server;
	server.display_name = String(p_source.get("display_name", p_source.get("name", p_name))).strip_edges();
	if (server.display_name.is_empty()) {
		server.display_name = p_name.strip_edges();
	}
	Variant transport_value = p_source.get("transport", p_source.get("type", Variant()));
	if (transport_value.get_type() == Variant::NIL && p_source.has("url")) {
		server.transport = "streamable_http";
	} else {
		server.transport = _normalize_transport(transport_value.get_type() == Variant::NIL ? String("stdio") : String(transport_value));
	}
	server.enabled = bool(p_source.get("enabled", true));

	if (server.transport == "stdio") {
		server.command = String(p_source.get("command", String())).strip_edges();
		if (p_source.has("args") && Variant(p_source["args"]).get_type() == Variant::ARRAY) {
			server.arguments = _string_array_to_arguments(p_source["args"]);
		} else {
			server.arguments = String(p_source.get("arguments", String())).strip_edges();
		}
		server.working_directory = String(p_source.get("working_directory", p_source.get("cwd", String()))).strip_edges();
		if (p_source.has("env") && Variant(p_source["env"]).get_type() == Variant::DICTIONARY) {
			server.environment = _dictionary_to_env_lines(p_source["env"]);
		} else {
			server.environment = String(p_source.get("environment", String())).strip_edges();
		}
		if (server.command.is_empty()) {
			r_error = "MCP stdio server `" + server.display_name + "` is missing command.";
			return false;
		}
	} else {
		server.url = String(p_source.get("url", String())).strip_edges();
		if (p_source.has("headers") && Variant(p_source["headers"]).get_type() == Variant::DICTIONARY) {
			server.headers = _dictionary_to_header_lines(p_source["headers"]);
		} else {
			server.headers = String(p_source.get("headers", String())).strip_edges();
		}
		if (server.url.is_empty()) {
			r_error = "MCP HTTP server `" + server.display_name + "` is missing url.";
			return false;
		}
	}

	server.id = _make_server_id(server.display_name);
	r_servers.push_back(_server_to_dictionary(server));
	return true;
}

String AIMCPSettings::add_server(const String &p_display_name, const String &p_command, const String &p_arguments, const String &p_working_directory, const String &p_environment, bool p_enabled) {
	const String display_name = p_display_name.strip_edges();
	const String command = p_command.strip_edges();
	if (display_name.is_empty() || command.is_empty()) {
		return String();
	}

	AIMCPServerConfig server;
	server.id = _make_server_id(display_name);
	server.display_name = display_name;
	server.transport = "stdio";
	server.command = command;
	server.arguments = p_arguments.strip_edges();
	server.working_directory = p_working_directory.strip_edges();
	server.environment = p_environment.strip_edges();
	server.enabled = p_enabled;

	Array servers = _get_server_storage();
	servers.push_back(_server_to_dictionary(server));
	_set_server_storage(servers);
	return server.id;
}

String AIMCPSettings::add_server_config(const AIMCPServerConfig &p_server) {
	AIMCPServerConfig server = p_server;
	server.display_name = server.display_name.strip_edges();
	server.transport = _normalize_transport(server.transport);
	server.command = server.command.strip_edges();
	server.arguments = server.arguments.strip_edges();
	server.working_directory = server.working_directory.strip_edges();
	server.environment = server.environment.strip_edges();
	server.url = server.url.strip_edges();
	server.headers = server.headers.strip_edges();
	if (server.display_name.is_empty()) {
		return String();
	}
	if (server.transport == "stdio" && server.command.is_empty()) {
		return String();
	}
	if (server.transport != "stdio" && server.url.is_empty()) {
		return String();
	}
	server.id = _make_server_id(server.display_name);

	Array servers = _get_server_storage();
	servers.push_back(_server_to_dictionary(server));
	_set_server_storage(servers);
	return server.id;
}

bool AIMCPSettings::update_server(const String &p_server_id, const String &p_display_name, const String &p_command, const String &p_arguments, const String &p_working_directory, const String &p_environment, bool p_enabled) {
	if (p_server_id.is_empty()) {
		return false;
	}

	const String display_name = p_display_name.strip_edges();
	const String command = p_command.strip_edges();
	if (display_name.is_empty() || command.is_empty()) {
		return false;
	}

	Array servers = _get_server_storage();
	for (int i = 0; i < servers.size(); i++) {
		if (Variant(servers[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIMCPServerConfig server = _server_from_dictionary(servers[i]);
		if (server.id != p_server_id) {
			continue;
		}

		server.display_name = display_name;
		server.transport = "stdio";
		server.command = command;
		server.arguments = p_arguments.strip_edges();
		server.working_directory = p_working_directory.strip_edges();
		server.environment = p_environment.strip_edges();
		server.url = String();
		server.headers = String();
		server.enabled = p_enabled;
		servers[i] = _server_to_dictionary(server);
		_set_server_storage(servers);
		return true;
	}

	return false;
}

bool AIMCPSettings::update_server_config(const AIMCPServerConfig &p_server) {
	if (p_server.id.is_empty()) {
		return false;
	}

	AIMCPServerConfig updated = p_server;
	updated.display_name = updated.display_name.strip_edges();
	updated.transport = _normalize_transport(updated.transport);
	updated.command = updated.command.strip_edges();
	updated.arguments = updated.arguments.strip_edges();
	updated.working_directory = updated.working_directory.strip_edges();
	updated.environment = updated.environment.strip_edges();
	updated.url = updated.url.strip_edges();
	updated.headers = updated.headers.strip_edges();
	if (updated.display_name.is_empty()) {
		return false;
	}
	if (updated.transport == "stdio" && updated.command.is_empty()) {
		return false;
	}
	if (updated.transport != "stdio" && updated.url.is_empty()) {
		return false;
	}

	Array servers = _get_server_storage();
	for (int i = 0; i < servers.size(); i++) {
		if (Variant(servers[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIMCPServerConfig server = _server_from_dictionary(servers[i]);
		if (server.id != updated.id) {
			continue;
		}

		servers[i] = _server_to_dictionary(updated);
		_set_server_storage(servers);
		return true;
	}

	return false;
}

bool AIMCPSettings::remove_server(const String &p_server_id) {
	Array servers = _get_server_storage();
	for (int i = 0; i < servers.size(); i++) {
		if (Variant(servers[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIMCPServerConfig server = _server_from_dictionary(servers[i]);
		if (server.id == p_server_id) {
			servers.remove_at(i);
			_set_server_storage(servers);
			return true;
		}
	}
	return false;
}

bool AIMCPSettings::set_server_enabled(const String &p_server_id, bool p_enabled) {
	Array servers = _get_server_storage();
	for (int i = 0; i < servers.size(); i++) {
		if (Variant(servers[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIMCPServerConfig server = _server_from_dictionary(servers[i]);
		if (server.id == p_server_id) {
			server.enabled = p_enabled;
			servers[i] = _server_to_dictionary(server);
			_set_server_storage(servers);
			return true;
		}
	}
	return false;
}

AIMCPServerConfig AIMCPSettings::get_server(const String &p_server_id) {
	Vector<AIMCPServerConfig> servers = get_servers(false);
	for (int i = 0; i < servers.size(); i++) {
		if (servers[i].id == p_server_id) {
			return servers[i];
		}
	}
	return AIMCPServerConfig();
}

Vector<AIMCPServerConfig> AIMCPSettings::get_servers(bool p_enabled_only) {
	Vector<AIMCPServerConfig> servers;
	Array storage = _get_server_storage();
	for (int i = 0; i < storage.size(); i++) {
		if (Variant(storage[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIMCPServerConfig server = _server_from_dictionary(storage[i]);
		if (server.id.is_empty()) {
			continue;
		}
		if (p_enabled_only) {
			if (!server.enabled) {
				continue;
			}
			if (!is_server_config_usable(server)) {
				continue;
			}
		}
		servers.push_back(server);
	}
	return servers;
}

Vector<AIMCPServerConfig> AIMCPSettings::get_server_status_configs() {
	Vector<AIMCPServerConfig> servers;
	Array storage = _get_server_storage();
	for (int i = 0; i < storage.size(); i++) {
		if (Variant(storage[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		AIMCPServerConfig server = _server_from_dictionary(storage[i]);
		if (server.id.is_empty()) {
			continue;
		}
		servers.push_back(server);
	}
	return servers;
}

bool AIMCPSettings::is_server_config_usable(const AIMCPServerConfig &p_server, String *r_error) {
	if (p_server.id.is_empty()) {
		if (r_error) {
			*r_error = "MCP server id is empty.";
		}
		return false;
	}
	if (p_server.transport == "stdio" && p_server.command.is_empty()) {
		if (r_error) {
			*r_error = "MCP stdio server command is empty.";
		}
		return false;
	}
	if (p_server.transport != "stdio" && p_server.url.is_empty()) {
		if (r_error) {
			*r_error = "MCP HTTP/SSE server URL is empty.";
		}
		return false;
	}
	if (r_error) {
		r_error->clear();
	}
	return true;
}

bool AIMCPSettings::import_servers_from_json(const String &p_json, String &r_error) {
	r_error.clear();

	Ref<JSON> parser;
	parser.instantiate();
	Error err = parser->parse(p_json);
	if (err != OK || (parser->get_data().get_type() != Variant::DICTIONARY && parser->get_data().get_type() != Variant::ARRAY)) {
		r_error = "MCP JSON configuration is invalid.";
		return false;
	}

	Array imported;
	if (parser->get_data().get_type() == Variant::ARRAY) {
		Array source_array = parser->get_data();
		for (int i = 0; i < source_array.size(); i++) {
			if (Variant(source_array[i]).get_type() != Variant::DICTIONARY) {
				continue;
			}
			if (!_append_server_from_dictionary(String(), source_array[i], imported, r_error)) {
				return false;
			}
		}
	} else {
		Dictionary root = parser->get_data();
		Variant servers_value = root.has("mcpServers") ? root["mcpServers"] : root.get("servers", Variant());
		if (servers_value.get_type() == Variant::DICTIONARY) {
			Dictionary server_map = servers_value;
			Array keys = server_map.keys();
			for (int i = 0; i < keys.size(); i++) {
				if (Variant(server_map[keys[i]]).get_type() != Variant::DICTIONARY) {
					continue;
				}
				if (!_append_server_from_dictionary(String(keys[i]), server_map[keys[i]], imported, r_error)) {
					return false;
				}
			}
		} else if (servers_value.get_type() == Variant::ARRAY) {
			Array server_array = servers_value;
			for (int i = 0; i < server_array.size(); i++) {
				if (Variant(server_array[i]).get_type() != Variant::DICTIONARY) {
					continue;
				}
				if (!_append_server_from_dictionary(String(), server_array[i], imported, r_error)) {
					return false;
				}
			}
		} else {
			if (!_append_server_from_dictionary(String(), root, imported, r_error)) {
				return false;
			}
		}
	}

	if (imported.is_empty()) {
		r_error = "MCP JSON configuration did not contain any usable servers.";
		return false;
	}

	Array servers = _get_server_storage();
	for (int i = 0; i < imported.size(); i++) {
		servers.push_back(imported[i]);
	}
	_set_server_storage(servers);
	return true;
}

Array AIMCPSettings::get_server_storage_for_test() {
	return _get_server_storage();
}

void AIMCPSettings::set_server_storage_for_test(const Array &p_servers) {
	_set_server_storage(p_servers);
}

void AIMCPSettings::clear_servers_for_test() {
	_set_server_storage(Array());
}
