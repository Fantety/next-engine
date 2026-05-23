/**************************************************************************/
/*  ai_script_inspect_tool.h                                              */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_script_editing_service.h"

class AIScriptInspectTool : public AITool {
	GDCLASS(AIScriptInspectTool, AITool);

	Ref<AIScriptEditingService> service;

public:
	AIScriptInspectTool();

	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIToolResult execute(const Dictionary &p_arguments) override;
};
