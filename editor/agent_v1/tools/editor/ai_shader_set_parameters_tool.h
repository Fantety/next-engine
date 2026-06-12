/**************************************************************************/
/*  ai_shader_set_parameters_tool.h                                       */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/agent_v1/tools/editor/ai_shader_editing_service.h"

class AIV1ShaderSetParametersTool : public AIV1EditorTool {
	GDCLASS(AIV1ShaderSetParametersTool, AIV1EditorTool);

	Ref<AIV1ShaderEditingService> service;

public:
	AIV1ShaderSetParametersTool();

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
