/**************************************************************************/
/*  ai_shader_apply_to_node_tool.h                                        */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_shader_editing_service.h"

class AIShaderApplyToNodeTool : public AITool {
	GDCLASS(AIShaderApplyToNodeTool, AITool);

	Ref<AIShaderEditingService> service;

public:
	AIShaderApplyToNodeTool();

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
