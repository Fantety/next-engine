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

bool _is_write_capable_profile(const String &p_profile_id) {
	return p_profile_id == "write" || p_profile_id == "review";
}

AIToolPermission _allow_when(bool p_condition) {
	return p_condition ? AI_TOOL_PERMISSION_ALLOW : AI_TOOL_PERMISSION_DENY;
}

} // namespace

void AIMainAgent::_bind_methods() {
}

AIMainAgent::AIMainAgent() {
	reload_tools();
}

void AIMainAgent::_register_local_tools() {
	const bool write_capable = _is_write_capable_profile(get_profile().id);
	const AIToolPermission write_permission = _allow_when(write_capable);
	const AIToolPermission destructive_permission = write_capable ? AI_TOOL_PERMISSION_ASK : AI_TOOL_PERMISSION_DENY;

	AIToolFactory::register_shared_project_tools(this, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_tool<AIActivateSkillTool>(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_tool<AIManagePlanTool>(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_project_write_tools(this, write_permission, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_scene_inspection_tools(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_scene_write_tools(this, write_permission, write_permission, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_script_inspection_tools(this, AI_TOOL_PERMISSION_ALLOW, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_script_write_tools(this, write_permission, destructive_permission, MAIN_AGENT_TOOL_LOG_PREFIX);
	AIToolFactory::register_shader_tools(this, write_permission, destructive_permission, MAIN_AGENT_TOOL_LOG_PREFIX);
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
