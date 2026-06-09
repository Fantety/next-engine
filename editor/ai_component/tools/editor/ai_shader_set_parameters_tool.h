/**************************************************************************/
/*  ai_shader_set_parameters_tool.h                                       */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_editing_service.h"

class AIShaderSetParametersTool : public AITool {
	GDCLASS(AIShaderSetParametersTool, AITool);

	Ref<AIShaderEditingService> service;

public:
	AIShaderSetParametersTool();

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
