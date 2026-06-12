/**************************************************************************/
/*  ai_requirement_form_tool.h                                            */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

class AIV1RequirementFormTool : public AIV1EditorTool {
	GDCLASS(AIV1RequirementFormTool, AIV1EditorTool);

public:
	static const char *TOOL_NAME;

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIV1EditorToolResult execute_tool(const Dictionary &p_arguments) override;

	static bool is_requirement_form_tool(const String &p_tool_name);
	static AIV1EditorToolResult make_submission_result(const Dictionary &p_form_arguments, const Dictionary &p_answers);
};
