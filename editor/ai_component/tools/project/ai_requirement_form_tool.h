/**************************************************************************/
/*  ai_requirement_form_tool.h                                            */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AIRequirementFormTool : public AITool {
	GDCLASS(AIRequirementFormTool, AITool);

public:
	static const char *TOOL_NAME;

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;

	static bool is_requirement_form_tool(const String &p_tool_name);
	static AIToolResult make_submission_result(const Dictionary &p_form_arguments, const Dictionary &p_answers);
};
