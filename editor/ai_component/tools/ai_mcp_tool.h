/**************************************************************************/
/*  ai_mcp_tool.h                                                         */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_mcp_protocol.h"
#include "editor/ai_component/providers/ai_mcp_settings.h"
#include "editor/ai_component/tools/ai_tool.h"

class AIMCPTool : public AITool {
	GDCLASS(AIMCPTool, AITool);

	AIMCPServerConfig server;
	AIMCPToolDescriptor descriptor;

protected:
	static void _bind_methods();

public:
	void setup(const AIMCPServerConfig &p_server, const AIMCPToolDescriptor &p_descriptor);

	virtual String get_name() const override;
	virtual String get_description() const override;
	virtual Dictionary get_parameters_schema() const override;
	virtual AIToolResult execute(const Dictionary &p_arguments) override;
};
