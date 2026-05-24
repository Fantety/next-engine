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
	String command;
	String arguments;
	String working_directory;
	String environment;
	bool enabled = true;
};

class AIMCPSettings {
	static String _get_servers_path();
	static Array _get_server_storage();
	static void _set_server_storage(const Array &p_servers);
	static AIMCPServerConfig _server_from_dictionary(const Dictionary &p_server);
	static Dictionary _server_to_dictionary(const AIMCPServerConfig &p_server);
	static String _make_server_id(const String &p_display_name);

public:
	static String add_server(const String &p_display_name, const String &p_command, const String &p_arguments, const String &p_working_directory, const String &p_environment, bool p_enabled = true);
	static bool update_server(const String &p_server_id, const String &p_display_name, const String &p_command, const String &p_arguments, const String &p_working_directory, const String &p_environment, bool p_enabled);
	static bool remove_server(const String &p_server_id);
	static bool set_server_enabled(const String &p_server_id, bool p_enabled);
	static AIMCPServerConfig get_server(const String &p_server_id);
	static Vector<AIMCPServerConfig> get_servers(bool p_enabled_only = false);
	static Array get_server_storage_for_test();
	static void set_server_storage_for_test(const Array &p_servers);
	static void clear_servers_for_test();
};
