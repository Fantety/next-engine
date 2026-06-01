/**************************************************************************/
/*  ai_create_markdown_tool.h                                             */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AICreateMarkdownTool : public AITool {
	GDCLASS(AICreateMarkdownTool, AITool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
