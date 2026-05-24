/**************************************************************************/
/*  ai_mcp_tool.cpp                                                       */
/**************************************************************************/

#include "ai_mcp_tool.h"

#include "editor/ai_component/providers/ai_mcp_client.h"

void AIMCPTool::_bind_methods() {
}

void AIMCPTool::setup(const AIMCPServerConfig &p_server, const AIMCPToolDescriptor &p_descriptor) {
	server = p_server;
	descriptor = p_descriptor;
}

String AIMCPTool::get_name() const {
	return AIMCPProtocol::make_agent_tool_name(server.id, descriptor.name);
}

String AIMCPTool::get_description() const {
	if (!descriptor.description.is_empty()) {
		return descriptor.description + "\n\nMCP Server: " + server.display_name + "\nMCP Tool: " + descriptor.name;
	}
	return "MCP tool `" + descriptor.name + "` from `" + server.display_name + "`.";
}

Dictionary AIMCPTool::get_parameters_schema() const {
	return descriptor.input_schema;
}

AIToolResult AIMCPTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	result.metadata["tool_origin"] = "mcp";
	result.metadata["mcp_server_id"] = server.id;
	result.metadata["mcp_server_name"] = server.display_name;
	result.metadata["mcp_tool_name"] = descriptor.name;
	result.metadata["mcp_transport"] = server.transport;
	result.metadata["mcp_agent_tool_name"] = get_name();

	Ref<AIMCPClient> client = AIMCPClientFactory::create_client(server);
	if (client.is_null()) {
		result.error = "MCP client transport is not available.";
		return result;
	}
	client->set_timeout_msec(30000);
	AIMCPToolCallResult call_result = client->call_tool(descriptor.name, p_arguments);
	result.content = call_result.content;
	result.error = call_result.error;
	result.metadata.merge(call_result.metadata, true);
	result.metadata["tool_origin"] = "mcp";
	result.metadata["mcp_agent_tool_name"] = get_name();
	return result;
}
