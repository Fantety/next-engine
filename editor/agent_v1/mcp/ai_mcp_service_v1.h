/**************************************************************************/
/*  ai_mcp_service_v1.h                                                   */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"
#include "editor/agent_v1/core/registry/ai_scoped_registration.h"
#include "editor/agent_v1/domain/context/ai_system_context.h"
#include "editor/agent_v1/tools/ai_tool_registry_v1.h"

#include "core/object/ref_counted.h"
#include "core/object/object_id.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIV1MCPService;
class AIFakeMCPServer;

class AIV1MCPToolAdapter : public AIV1Tool {
	GDCLASS(AIV1MCPToolAdapter, AIV1Tool);

	ObjectID service_id;
	Dictionary server_config;
	Dictionary descriptor;
	String server_id;
	String server_name;
	String tool_name;
	String agent_tool_name;
	String permission_default;

protected:
	static void _bind_methods();

public:
	void setup(AIV1MCPService *p_service, const Dictionary &p_server_config, const Dictionary &p_descriptor);

	String get_server_id() const;
	String get_tool_name() const;
	String get_agent_tool_name() const;

	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override;
};

class AIV1MCPService : public RefCounted {
	GDCLASS(AIV1MCPService, RefCounted);
	friend class AIV1MCPToolAdapter;

	struct ServerHandle {
		Dictionary config;
		Ref<AIFakeMCPServer> fake_server;
		String state = "stopped";
		String last_error;
		Array tools;
		Array resources;
		Array prompts;
		uint64_t discovered_at = 0;
		Vector<Ref<AIScopedRegistration>> registrations;
	};

	Ref<AIV1ToolRegistry> tool_registry;
	HashMap<String, ServerHandle> servers;
	mutable Mutex mutex;

	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Array _array_from_variant(const Variant &p_value);
	static String _server_id_from_config(const Dictionary &p_config);
	static String _server_name_from_config(const Dictionary &p_config);
	static String _transport_type_from_config(const Dictionary &p_config);
	static String _permission_default_from_config(const Dictionary &p_config);
	static String _startup_permission_default_from_config(const Dictionary &p_config);
	static Dictionary _normalize_server_config(const Dictionary &p_config);
	static Dictionary _normalize_tool_descriptor(const String &p_server_id, const String &p_server_name, const Dictionary &p_descriptor);
	static String _content_from_mcp_result(const Variant &p_value, PackedStringArray &r_content_types);
	static Dictionary _metadata_for_server_tool(const Dictionary &p_server_config, const Dictionary &p_descriptor, const String &p_agent_tool_name);
	static String _arguments_hash(const Dictionary &p_arguments);
	static String _arguments_preview(const Dictionary &p_arguments, int p_max_chars = 160);
	static String _permission_resource_for_tool_call(const String &p_server_id, const String &p_tool_name, const Dictionary &p_arguments);
	static bool _validate_server_config(const Dictionary &p_config, AIError &r_error);

	static void _close_registration_list(Vector<Ref<AIScopedRegistration>> &r_registrations);
	void _close_registrations(ServerHandle &r_handle) const;
	bool _register_discovered_tools(const String &p_server_id, ServerHandle &r_handle, AIError &r_error);
	bool _refresh_fake_server(const String &p_server_id, ServerHandle &r_handle, AIError &r_error);
	bool _refresh_configured_server(const String &p_server_id, ServerHandle &r_handle, AIError &r_error);
	bool _assert_configured_server_start_permission(const String &p_server_id, ServerHandle &r_handle, AIError &r_error) const;
	void _commit_server_status_from_handle(const String &p_server_id, const ServerHandle &p_handle);
	bool _call_fake_tool(const String &p_server_id, const String &p_tool_name, const Dictionary &p_arguments, Dictionary &r_result, AIError &r_error) const;
	bool _call_configured_tool(const String &p_server_id, const String &p_tool_name, const Dictionary &p_arguments, Dictionary &r_result, AIError &r_error);

protected:
	static void _bind_methods();

public:
	~AIV1MCPService();

	static String sanitize_name_part(const String &p_text);
	static String make_tool_name(const String &p_server_id, const String &p_tool_name);

	void set_tool_registry(const Ref<AIV1ToolRegistry> &p_tool_registry);
	Ref<AIV1ToolRegistry> get_tool_registry() const;

	bool register_server_struct(const Dictionary &p_config, AIError &r_error);
	bool register_fake_server_for_test(const Dictionary &p_config, const Ref<AIFakeMCPServer> &p_server);
	Dictionary register_server(const Dictionary &p_config);
	bool import_config_struct(const Dictionary &p_config, AIError &r_error);
	Dictionary import_config(const Dictionary &p_config);
	void clear();

	bool refresh_struct(AIError &r_error);
	Dictionary refresh();

	Array get_statuses() const;
	Dictionary get_status_summary() const;
	Array get_discovery_snapshots() const;
	Array list_resources(const String &p_server_id = String()) const;
	Array list_prompts(const String &p_server_id = String()) const;
	Dictionary read_resource(const String &p_server_id, const String &p_uri);
	Dictionary make_resource_context_source(const String &p_server_id, const String &p_uri, bool p_required = false, int p_priority = 300);
	Dictionary render_prompt(const String &p_server_id, const String &p_prompt_name, const Dictionary &p_arguments = Dictionary());

	bool call_tool_struct(const String &p_server_id, const String &p_tool_name, const Dictionary &p_arguments, Dictionary &r_result, AIError &r_error);
};
