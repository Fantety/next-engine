/**************************************************************************/
/*  ai_mcp_settings.cpp                                                   */
/**************************************************************************/

#include "ai_mcp_settings.h"

#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "editor/settings/editor_settings.h"

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
	return value;
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
	server.command = String(p_server.get("command", String()));
	server.arguments = String(p_server.get("arguments", String()));
	server.working_directory = String(p_server.get("working_directory", String()));
	server.environment = String(p_server.get("environment", String()));
	server.enabled = bool(p_server.get("enabled", true));
	if (server.display_name.is_empty()) {
		server.display_name = server.command;
	}
	return server;
}

Dictionary AIMCPSettings::_server_to_dictionary(const AIMCPServerConfig &p_server) {
	Dictionary server;
	server["id"] = p_server.id;
	server["display_name"] = p_server.display_name;
	server["command"] = p_server.command;
	server["arguments"] = p_server.arguments;
	server["working_directory"] = p_server.working_directory;
	server["environment"] = p_server.environment;
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

String AIMCPSettings::add_server(const String &p_display_name, const String &p_command, const String &p_arguments, const String &p_working_directory, const String &p_environment, bool p_enabled) {
	const String display_name = p_display_name.strip_edges();
	const String command = p_command.strip_edges();
	if (display_name.is_empty() || command.is_empty()) {
		return String();
	}

	AIMCPServerConfig server;
	server.id = _make_server_id(display_name);
	server.display_name = display_name;
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
		server.command = command;
		server.arguments = p_arguments.strip_edges();
		server.working_directory = p_working_directory.strip_edges();
		server.environment = p_environment.strip_edges();
		server.enabled = p_enabled;
		servers[i] = _server_to_dictionary(server);
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
		if (server.id.is_empty() || server.command.is_empty()) {
			continue;
		}
		if (p_enabled_only && !server.enabled) {
			continue;
		}
		servers.push_back(server);
	}
	return servers;
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
