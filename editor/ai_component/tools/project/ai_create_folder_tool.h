/**************************************************************************/
/*  ai_create_folder_tool.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AICreateFolderTool : public AITool {
	GDCLASS(AICreateFolderTool, AITool);

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
