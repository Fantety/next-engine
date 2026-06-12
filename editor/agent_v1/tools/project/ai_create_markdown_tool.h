/**************************************************************************/
/*  ai_create_markdown_tool.h                                             */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

class AIV1CreateMarkdownTool : public AIV1EditorTool {
	GDCLASS(AIV1CreateMarkdownTool, AIV1EditorTool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;
};
