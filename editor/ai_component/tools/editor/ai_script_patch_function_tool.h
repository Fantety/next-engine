/**************************************************************************/
/*  ai_script_patch_function_tool.h                                       */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_script_editing_service.h"

class AIScriptPatchFunctionTool : public AITool {
	GDCLASS(AIScriptPatchFunctionTool, AITool);

	Ref<AIScriptEditingService> service;

public:
	AIScriptPatchFunctionTool();

	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIToolResult execute(const Dictionary &p_arguments) override;
};
