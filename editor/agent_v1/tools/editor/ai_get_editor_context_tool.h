/**************************************************************************/
/*  ai_get_editor_context_tool.h                                          */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

class AIV1GetEditorContextTool : public AIV1EditorTool {
	GDCLASS(AIV1GetEditorContextTool, AIV1EditorTool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
