/**************************************************************************/
/*  ai_shader_edit_tool.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_editing_service.h"

class AIShaderEditTool : public AITool {
	GDCLASS(AIShaderEditTool, AITool);

	Ref<AIShaderEditingService> service;

public:
	AIShaderEditTool();

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
