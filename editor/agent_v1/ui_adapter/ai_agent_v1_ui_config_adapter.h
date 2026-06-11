/**************************************************************************/
/*  ai_agent_v1_ui_config_adapter.h                                       */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/agents/ai_agent_service_v1.h"
#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/mcp/ai_mcp_service_v1.h"
#include "editor/agent_v1/skills/ai_skill_service_v1.h"

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIAgentV1UIConfigAdapter : public RefCounted {
	GDCLASS(AIAgentV1UIConfigAdapter, RefCounted);

	Ref<AIConfigService> config_service;
	Ref<AIV1MCPService> mcp_service;
	Ref<AIV1SkillService> skill_service;
	Ref<AIAgentService> agent_service;

	void _ensure_defaults();
	void _wire_service_signals();

	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Array _array_from_variant(const Variant &p_value);
	Array _models_from_config(const Dictionary &p_config) const;
	Array _mcp_servers_from_config(const Dictionary &p_config) const;
	Array _skills_from_config(const Dictionary &p_config) const;
	Array _rules_from_config(const Dictionary &p_config) const;
	Array _marquees_from_config(const Dictionary &p_config) const;

	void _config_changed(const Dictionary &p_config);
	void _mcp_status_changed(const Array &p_statuses, const Dictionary &p_summary);
	void _skill_tools_changed();

protected:
	static void _bind_methods();

public:
	AIAgentV1UIConfigAdapter();

	void set_config_service(const Ref<AIConfigService> &p_service);
	Ref<AIConfigService> get_config_service() const;
	void set_mcp_service(const Ref<AIV1MCPService> &p_service);
	Ref<AIV1MCPService> get_mcp_service() const;
	void set_skill_service(const Ref<AIV1SkillService> &p_service);
	Ref<AIV1SkillService> get_skill_service() const;
	void set_agent_service(const Ref<AIAgentService> &p_service);
	Ref<AIAgentService> get_agent_service() const;

	Dictionary get_settings_snapshot();
	Array list_models();
	Array list_agents();
	Dictionary patch_settings(const Dictionary &p_patch, const String &p_scope = "project");
};
