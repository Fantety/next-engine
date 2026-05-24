/**************************************************************************/
/*  ai_mcp_client.cpp                                                     */
/**************************************************************************/

#include "ai_mcp_client.h"

#include "core/math/math_funcs.h"

#include "editor/ai_component/providers/ai_mcp_http_client.h"
#include "editor/ai_component/providers/ai_mcp_stdio_client.h"

void AIMCPClient::_bind_methods() {
}

void AIMCPClient::set_server_config(const AIMCPServerConfig &p_server) {
	server = p_server;
}

AIMCPServerConfig AIMCPClient::get_server_config() const {
	return server;
}

void AIMCPClient::set_timeout_msec(int p_timeout_msec) {
	timeout_msec = MAX(1000, p_timeout_msec);
}

int AIMCPClient::get_timeout_msec() const {
	return timeout_msec;
}

bool AIMCPClient::initialize(String &r_error) {
	r_error = "MCP client transport is not implemented.";
	return false;
}

bool AIMCPClient::list_tools(Vector<AIMCPToolDescriptor> &r_tools, String &r_error) {
	r_tools.clear();
	r_error = "MCP client transport is not implemented.";
	return false;
}

AIMCPToolCallResult AIMCPClient::call_tool(const String &p_tool_name, const Dictionary &p_arguments) {
	(void)p_tool_name;
	(void)p_arguments;
	AIMCPToolCallResult result;
	result.error = "MCP client transport is not implemented.";
	return result;
}

Ref<AIMCPClient> AIMCPClientFactory::create_client(const AIMCPServerConfig &p_server) {
	Ref<AIMCPClient> client;
	if (p_server.transport == "streamable_http" || p_server.transport == "sse") {
		Ref<AIMCPHTTPClient> http_client;
		http_client.instantiate();
		client = http_client;
	} else {
		Ref<AIMCPStdioClient> stdio_client;
		stdio_client.instantiate();
		client = stdio_client;
	}

	client->set_server_config(p_server);
	return client;
}
