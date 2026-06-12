/**************************************************************************/
/*  ai_script_delete_tool.h                                               */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/agent_v1/tools/editor/ai_script_editing_service.h"

class AIV1ScriptDeleteTool : public AIV1EditorTool {
	GDCLASS(AIV1ScriptDeleteTool, AIV1EditorTool);

	Ref<AIV1ScriptEditingService> service;

public:
	AIV1ScriptDeleteTool();

	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
