/**************************************************************************/
/*  ai_list_project_tool.h                                                */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AIListProjectTool : public AITool {
	GDCLASS(AIListProjectTool, AITool);

	int entry_count = 0;
	int max_depth = 4;
	int max_entries = 400;
	int max_chars = 16000;
	bool truncated = false;

	void _append_tree(String &r_output, const String &p_path, int p_depth);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
