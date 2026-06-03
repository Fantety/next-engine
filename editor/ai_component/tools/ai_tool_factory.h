/**************************************************************************/
/*  ai_tool_factory.h                                                     */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_agent_base.h"
#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/ai_tool_permission.h"

namespace AIToolFactory {

template <typename T>
Ref<AITool> make_tool() {
	Ref<T> tool;
	tool.instantiate();
	return tool;
}

bool register_tool(AIAgentBase *p_agent, const Ref<AITool> &p_tool, AIToolPermission p_permission, const String &p_log_prefix = String());

template <typename T>
bool register_tool(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix = String()) {
	return register_tool(p_agent, make_tool<T>(), p_permission, p_log_prefix);
}

void register_shared_project_tools(AIAgentBase *p_agent, const String &p_log_prefix = String());
void register_editor_runtime_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix = String());
void register_project_write_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix = String());
void register_scene_inspection_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix = String());
void register_scene_write_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix = String());
void register_script_inspection_tools(AIAgentBase *p_agent, AIToolPermission p_permission, const String &p_log_prefix = String());
void register_script_write_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix = String());
void register_shader_tools(AIAgentBase *p_agent, AIToolPermission p_write_permission, AIToolPermission p_delete_permission, const String &p_log_prefix = String());

} // namespace AIToolFactory
