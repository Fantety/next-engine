/**************************************************************************/
/*  ai_agent_service_v1.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/session/service/ai_session_store.h"
#include "editor/agent_v1/tools/ai_tool_v1.h"

#include "core/object/object_id.h"
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AISessionService;
class AISessionProjector;

struct AIAgentConfig {
	String id;
	String name;
	String description;
	String provider;
	String model;
	Array system;
	Dictionary tools;
	Dictionary permissions;
	Dictionary context;
	Dictionary skills;
	Dictionary subagents;
	Dictionary metadata;
	bool permission_inherit_explicit = false;
	bool permission_default_effect_explicit = false;

	Dictionary to_dictionary() const;
	static AIAgentConfig from_dictionary(const String &p_id, const Dictionary &p_dict, const String &p_default_provider, const String &p_default_model);
};

class AIAgentService : public RefCounted {
	GDCLASS(AIAgentService, RefCounted);
	friend class AIV1TaskTool;

	Ref<AIConfigService> config_service;
	Ref<AISessionStore> session_store;

	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Array _array_from_variant(const Variant &p_value);
	static Array _string_array_from_variant(const Variant &p_value);
	static Dictionary _agents_from_config(const Dictionary &p_config);
	static String _default_agent_id_from_config(const Dictionary &p_config);
	static bool _array_contains_string(const Array &p_array, const String &p_value);
	int _subagent_depth_for_session(const String &p_session_id) const;
	int _child_session_count(const String &p_parent_session_id) const;
	int _active_child_session_count(const String &p_parent_session_id) const;

protected:
	static void _bind_methods();

public:
	AIAgentService();

	void set_config_service(const Ref<AIConfigService> &p_config_service);
	Ref<AIConfigService> get_config_service() const;
	void set_session_store(const Ref<AISessionStore> &p_session_store);
	Ref<AISessionStore> get_session_store() const;

	bool resolve_struct(const String &p_agent_id, AIAgentConfig &r_agent, AIError &r_error) const;
	bool resolve_for_session_struct(const String &p_session_id, AIAgentConfig &r_agent, AIError &r_error) const;
	bool assert_can_spawn_struct(const String &p_parent_session_id, const String &p_child_agent_id, AIError &r_error) const;
	Array permission_rules_for_agent(const AIAgentConfig &p_agent, const Array &p_base_rules) const;

	Dictionary resolve(const String &p_agent_id = String()) const;
	Dictionary resolve_for_session(const String &p_session_id) const;
	Array list() const;
	Dictionary assert_can_spawn(const String &p_parent_session_id, const String &p_child_agent_id) const;
};

class AIV1TaskTool : public AIV1Tool {
	GDCLASS(AIV1TaskTool, AIV1Tool);

	ObjectID session_service_id;
	ObjectID agent_service_id;

	static Dictionary _schema_property(const String &p_type, const String &p_description = String());
	static Dictionary _object_schema(const Dictionary &p_properties, const Array &p_required);
	static String _build_child_prompt(const Dictionary &p_arguments);
	static String _latest_assistant_text(const Ref<AISessionProjector> &p_projector, const String &p_session_id);
	static String _summary_from_result(const String &p_agent_id, const String &p_result, const String &p_status);

protected:
	static void _bind_methods();

public:
	AIV1TaskTool();

	void setup(AISessionService *p_session_service, AIAgentService *p_agent_service);

	virtual bool execute_struct(const Dictionary &p_arguments, const AIV1ToolExecutionContext &p_context, AIV1ToolExecutionResult &r_result, AIError &r_error) override;
};
