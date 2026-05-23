/**************************************************************************/
/*  ai_script_delete_tool.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/editor/ai_script_editing_service.h"

class AIScriptDeleteTool : public AITool {
	GDCLASS(AIScriptDeleteTool, AITool);

	Ref<AIScriptEditingService> service;

public:
	AIScriptDeleteTool();

	String get_name() const override;
	String get_description() const override;
	Dictionary get_parameters_schema() const override;
	AIToolResult execute(const Dictionary &p_arguments) override;
};
