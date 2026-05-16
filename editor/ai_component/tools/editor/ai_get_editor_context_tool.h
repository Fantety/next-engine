/**************************************************************************/
/*  ai_get_editor_context_tool.h                                          */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AIGetEditorContextTool : public AITool {
	GDCLASS(AIGetEditorContextTool, AITool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
