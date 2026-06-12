/**************************************************************************/
/*  ai_script_write_tool.h                                                */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/agent_v1/tools/editor/ai_script_editing_service.h"

class AIV1ScriptWriteTool : public AIV1EditorTool {
	GDCLASS(AIV1ScriptWriteTool, AIV1EditorTool);

	Ref<AIV1ScriptEditingService> service;

public:
	AIV1ScriptWriteTool();

	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
