/**************************************************************************/
/*  ai_tool_registry_v1.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/registry/ai_scoped_registration.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/domain/projection/ai_session_projector.h"
#include "editor/agent_v1/permission/ai_permission_service.h"
#include "editor/agent_v1/tools/ai_tool_v1.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"

class AIV1ToolRegistry;

struct AIV1ToolSettlement {
	bool success = false;
	bool executed = false;
	bool stale = false;
	bool pending = false;
	bool provider_executed = false;
	bool needs_continuation = false;
	String session_id;
	String agent_id;
	String assistant_message_id;
	String call_id;
	String tool_name;
	Variant input;
	Variant result;
	Variant content;
	Variant structured;
	PackedStringArray output_paths;
	Dictionary metadata;
	AIError error;

	Dictionary to_dictionary() const;
};

class AIV1ToolMaterialization : public RefCounted {
	GDCLASS(AIV1ToolMaterialization, RefCounted);

public:
	struct MaterializedEntry {
		Ref<AIV1Tool> tool;
		AIRegistrationIdentity identity;
		AIModelToolDefinition definition;
	};

private:
	Ref<AIV1ToolRegistry> registry;
	HashMap<String, MaterializedEntry> entries;
	Array definition_cache;
	String root_dir;

protected:
	static void _bind_methods();

public:
	void setup(const Ref<AIV1ToolRegistry> &p_registry, const HashMap<String, MaterializedEntry> &p_entries, const String &p_root_dir = String());
	Array get_definitions() const;
	bool has_tool(const String &p_name) const;
	Dictionary get_tool_identity(const String &p_name) const;
	bool settle_struct(const Dictionary &p_input, AIV1ToolSettlement &r_settlement, AIError &r_error);
	Dictionary settle(const Dictionary &p_input);
};

class AIV1ToolRegistry : public RefCounted {
	GDCLASS(AIV1ToolRegistry, RefCounted);

	struct ToolEntry {
		Ref<AIV1Tool> tool;
		AIRegistrationIdentity identity;
	};

	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;
	Ref<AIPermissionService> permission_service;
	HashMap<String, Vector<ToolEntry>> tools;
	uint64_t generation = 0;
	String owner = "agent_v1";
	String root_dir;
	mutable Mutex mutex;

	static bool _is_valid_tool_name(const String &p_name);
	static bool _validate_arguments(const Ref<AIV1Tool> &p_tool, const Dictionary &p_arguments, AIError &r_error);
	static bool _validate_schema_type(const Variant &p_value, const String &p_type);
	static bool _wildcard_match(const String &p_pattern, const String &p_value);
	static bool _is_tool_wholly_disabled(const String &p_name, const Ref<AIV1Tool> &p_tool, const Array &p_rules);
	bool _get_entry_struct(const String &p_name, ToolEntry &r_entry) const;
	bool _append_settlement_event(const AIV1ToolSettlement &p_settlement, bool p_success, AIError &r_error);
	void _close_registration(const Dictionary &p_identity);

protected:
	static void _bind_methods();

public:
	AIV1ToolRegistry();

	void set_event_store(const Ref<AIEventStore> &p_event_store);
	Ref<AIEventStore> get_event_store() const;
	void set_projector(const Ref<AISessionProjector> &p_projector);
	Ref<AISessionProjector> get_projector() const;
	void set_permission_service(const Ref<AIPermissionService> &p_permission_service);
	Ref<AIPermissionService> get_permission_service() const;
	void set_root_dir(const String &p_root_dir);
	String get_root_dir() const;

	bool register_tool_struct(const String &p_name, const Ref<AIV1Tool> &p_tool, const String &p_source = String(), const Dictionary &p_metadata = Dictionary(), AIRegistrationIdentity *r_identity = nullptr);
	bool register_tool(const String &p_name, const Ref<AIV1Tool> &p_tool);
	Ref<AIScopedRegistration> register_tool_scope(const String &p_name, const Ref<AIV1Tool> &p_tool);
	Ref<AIScopedRegistration> register_tool_scope_struct(const String &p_name, const Ref<AIV1Tool> &p_tool, const String &p_source = String(), const Dictionary &p_metadata = Dictionary(), AIError *r_error = nullptr);
	void register_builtin_tools();
	bool has_tool(const String &p_name) const;
	Dictionary get_tool_identity(const String &p_name) const;
	bool is_identity_current(const String &p_name, const AIRegistrationIdentity &p_identity) const;
	Ref<AIV1ToolMaterialization> materialize_struct();
	Ref<AIV1ToolMaterialization> materialize_struct(const String &p_root_dir, const Array &p_permission_rules);
	Ref<AIV1ToolMaterialization> materialize_for_context(const String &p_root_dir, const Array &p_permission_rules);
	Ref<AIV1ToolMaterialization> materialize();
	Array get_tool_names() const;
	void clear();

	bool settle_materialized_tool(const Ref<AIV1Tool> &p_tool, const AIRegistrationIdentity &p_identity, const Dictionary &p_input, AIV1ToolSettlement &r_settlement, AIError &r_error);
};
