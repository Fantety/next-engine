#pragma once

#include "core/io/http_router.h"
#include "core/io/http_request.h"
#include "core/io/http_response.h"
#include "streamable_http_server.h"
#include "editor/ai_component/mcp/mcp_http_server.h"

class McpRouter : public HttpRouter {
	GDCLASS(McpRouter, HttpRouter);

protected:
	static void _bind_methods();

public:
	McpRouter();
	~McpRouter();

	// Handle MCP POST requests
	void handle_post(Ref<HttpRequest> request, Ref<HttpResponse> response) override;

private:
	// Process MCP request and send streaming response
	void _process_mcp_request(Ref<HttpRequest> request, Ref<HttpResponse> response);
};