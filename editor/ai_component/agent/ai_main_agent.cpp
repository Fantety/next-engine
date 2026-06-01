/**************************************************************************/
/*  ai_main_agent.cpp                                                     */
/**************************************************************************/

#include "ai_main_agent.h"

#include "editor/ai_component/agent/ai_mcp_service.h"
#include "editor/ai_component/planning/ai_manage_plan_tool.h"
#include "editor/ai_component/skills/ai_activate_skill_tool.h"
#include "editor/ai_component/tools/ai_tool_factory.h"

namespace {

const String MAIN_AGENT_TOOL_LOG_PREFIX = "[AI Agent][MainAgent]";

} // namespace

void AIMainAgent::_bind_methods() {
}

AIMainAgent::AIMainAgent() {
	reload_tools();
}

void AIMainAgent::_register_local_tools() {
	AIToolFactory::register_shared_project_tools(this, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_tool<AIActivateSkillTool>(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_tool<AIManagePlanTool>(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_project_write_tools(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_scene_inspection_tools(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_scene_write_tools(this, AI_TOOL_PERMISSION_ALLOW, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_script_inspection_tools(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_script_write_tools(this, AI_TOOL_PERMISSION_ALLOW, AI_TOOL_PERMISSION_ASK, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_shader_tools(this, AI_TOOL_PERMISSION_ALLOW, AI_TOOL_PERMISSION_ASK, MAIN_AGENT_TOOL_LOG_PREFIX);

	_apply_profile_denylist();
}

void AIMainAgent::_apply_profile_denylist() {
	Ref<AIToolRegistry> registry = get_tool_registry();
	if (registry.is_null()) {
		return;
	}

	// Force profile-disabled tools to DENY while preserving each tool's
	// registered baseline permission otherwise.
	const AIAgentProfile active_profile = get_profile();
	const Vector<String> tool_names = registry->get_tool_names();
	for (int i = 0; i < tool_names.size(); i++) {
		const String &tool_name = tool_names[i];
		if (active_profile.denies_tool(tool_name)) {
			registry->set_tool_permission(tool_name, AI_TOOL_PERMISSION_DENY, vformat("Tool `%s` is disabled in the `%s` mode.", tool_name, active_profile.display_name));
		}
	}
}

void AIMainAgent::_register_mcp_tools() {
	Ref<AIMCPService> mcp_service = AIMCPService::get_singleton();
	if (mcp_service.is_null()) {
		return;
	}
	mcp_service->register_discovered_tools(get_tool_registry(), AI_TOOL_PERMISSION_ASK);
}

void AIMainAgent::set_profile(const AIAgentProfile &p_profile) {
	AIAgentBase::set_profile(p_profile);
	reload_tools();
}

void AIMainAgent::set_agent_profile_id(const String &p_profile_id) {
	AIAgentBase::set_agent_profile_id(p_profile_id);
}
void AIMainAgent::reload_tools() {
	clear_tools();
	_register_local_tools();
	_register_mcp_tools();
}
