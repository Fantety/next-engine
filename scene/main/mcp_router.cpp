#include "mcp_router.h"
#include "core/io/json.h"
#include "core/os/time.h"

void McpRouter::_bind_methods() {
	// No specific methods to bind for this router
}

McpRouter::McpRouter() {}

McpRouter::~McpRouter() {}

void McpRouter::handle_post(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	_process_mcp_request(request, response);
}

void McpRouter::_process_mcp_request(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	// Parse the JSON request body
	String body = request->get_body();
	Variant json_variant;
	String err_str;
	int err_line;
	
	Ref<JSON> json_parser;
	json_parser.instantiate();
	Error err = json_parser->parse(body);
	if (err != OK) {
		response->send(400, "Invalid JSON in request body");
		return;
	}
	json_variant = json_parser->get_data();
	
	Dictionary json_request = json_variant;
	
	// Check if we have a client reference
	StreamPeer* peer = response->client;
	StreamPeerTCP* client = Object::cast_to<StreamPeerTCP>(peer);
	if (client) {
		// Get the server instance from the client
		Node* node = Object::cast_to<Node>(client->get_meta("server"));
		MCPHttpServer* server = Object::cast_to<MCPHttpServer>(node);
		if (server) {
			// Handle the MCP message
			server->_handle_mcp_message(Ref<StreamPeerTCP>(client), json_request);
			// Don't send a response here, as _handle_mcp_message will send the appropriate JSON-RPC response
		} else {
			response->send(500, "Internal server error");
		}
	} else {
		response->send(500, "Internal server error");
	}
}