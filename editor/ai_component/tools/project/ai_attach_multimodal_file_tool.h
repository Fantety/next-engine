/**************************************************************************/
/*  ai_attach_multimodal_file_tool.h                                      */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

class AIAttachMultimodalFileTool : public AITool {
	GDCLASS(AIAttachMultimodalFileTool, AITool);

protected:
	static void _bind_methods() {}

public:
	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
