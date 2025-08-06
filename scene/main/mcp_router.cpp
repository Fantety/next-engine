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
	
	// Check if it's an MCP initialization request
	if (json_request.has("method") && json_request["method"] == "initialize") {
		// Send streaming response headers first
		response->set_header("Content-Type", "text/event-stream");
		response->set_header("Cache-Control", "no-cache");
		response->set_header("Connection", "keep-alive");
		response->set_header("X-Accel-Buffering", "no");
		response->set_header("Transfer-Encoding", "chunked");
		
		// Check if we have a client reference
		StreamPeer* peer = response->client;
		StreamPeerTCP* client = Object::cast_to<StreamPeerTCP>(peer);
		if (client) {
			// Create the initialization response
			Dictionary init_response;
			init_response["jsonrpc"] = "2.0";
			init_response["id"] = json_request["id"];
			
			Dictionary result;
			result["protocolversion"] = "2024-11-05";
			
			Dictionary capabilities;
			capabilities["experimental"] = 0;
			
			Dictionary prompts;
			prompts["listChanged"] = false;
			capabilities["prompts"] = prompts;
			
			Dictionary resources;
			resources["subscribe"] = false;
			resources["listChanged"] = false;
			capabilities["resources"] = resources;
			
			Dictionary tools;
			tools["listChanged"] = false;
			capabilities["tools"] = tools;
			
			result["capabilities"] = capabilities;
			
			Dictionary server_info;
			server_info["name"] = "godot-mcp-server";
			server_info["version"] = "1.0.0";
			result["serverInfo"] = server_info;
			
			init_response["result"] = result;
			
			// Convert response to JSON
			String response_json = JSON::stringify(init_response);
			
			// Send as streaming message
			// Note: In a real implementation, you would keep the connection open and send more messages
			// For this example, we'll send one message and close
			// We need to get the server instance from the client
			// This is a simplified approach, in a real implementation you might need to store a reference to the server
			Node* node = Object::cast_to<Node>(client->get_meta("server"));
			StreamableHttpServer* server = Object::cast_to<StreamableHttpServer>(node);
			if (server) {
				Ref<StreamPeerTCP> client_ref = Ref<StreamPeerTCP>(client);
				server->send_stream_message(client_ref, "message", response_json);
			}
			
			// In a real implementation, you would not send a regular response after streaming
			// But for this example, we'll send a simple response to close the request
			response->send(200, "Streaming response sent");
		} else {
			response->send(500, "Internal server error");
		}
	} else {
		response->send(400, "Unsupported MCP method");
	}
}