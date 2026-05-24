/**************************************************************************/
/*  ai_mcp_settings.h                                                     */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AIMCPServerConfig {
	String id;
	String display_name;
	String transport = "stdio";
	String command;
	String arguments;
	String working_directory;
	String environment;
	String url;
	String headers;
	bool enabled = true;
};

class AIMCPSettings {
	static String _get_servers_path();
	static Array _get_server_storage();
	static void _set_server_storage(const Array &p_servers);
	static AIMCPServerConfig _server_from_dictionary(const Dictionary &p_server);
	static Dictionary _server_to_dictionary(const AIMCPServerConfig &p_server);
	static String _make_server_id(const String &p_display_name);
	static String _normalize_transport(const String &p_transport);
	static String _string_array_to_arguments(const Array &p_array);
	static String _dictionary_to_env_lines(const Dictionary &p_dictionary);
	static String _dictionary_to_header_lines(const Dictionary &p_dictionary);
	static bool _append_server_from_dictionary(const String &p_name, const Dictionary &p_source, Array &r_servers, String &r_error);

public:
	static String add_server(const String &p_display_name, const String &p_command, const String &p_arguments, const String &p_working_directory, const String &p_environment, bool p_enabled = true);
	static String add_server_config(const AIMCPServerConfig &p_server);
	static bool update_server(const String &p_server_id, const String &p_display_name, const String &p_command, const String &p_arguments, const String &p_working_directory, const String &p_environment, bool p_enabled);
	static bool update_server_config(const AIMCPServerConfig &p_server);
	static bool remove_server(const String &p_server_id);
	static bool set_server_enabled(const String &p_server_id, bool p_enabled);
	static AIMCPServerConfig get_server(const String &p_server_id);
	static Vector<AIMCPServerConfig> get_servers(bool p_enabled_only = false);
	static Vector<AIMCPServerConfig> get_server_status_configs();
	static bool is_server_config_usable(const AIMCPServerConfig &p_server, String *r_error = nullptr);
	static bool import_servers_from_json(const String &p_json, String &r_error);
	static Array get_server_storage_for_test();
	static void set_server_storage_for_test(const Array &p_servers);
	static void clear_servers_for_test();
};
