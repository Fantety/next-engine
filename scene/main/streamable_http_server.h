/*
 * @FilePath: \scene\main\streamable_http_server.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-08-06 10:38:20
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-06 10:38:50
 */
#pragma once

#include "scene/main/node.h"
#include "core/io/tcp_server.h"
#include "core/io/stream_peer_tcp.h"
#include "core/templates/vector.h"
#include "core/templates/list.h"
#include "modules/regex/regex.h"

#include "core/io/http_request.h"
#include "core/io/http_response.h"
#include "core/io/http_router.h"

struct StreamableRouterInfo {
	Ref<RegEx> path;
	Vector<String> params;
	Ref<HttpRouter> router;
};

class StreamableHttpServer : public Node {
	GDCLASS(StreamableHttpServer, Node);

protected:
	static void _bind_methods();

	GDVIRTUAL1(_process, double)

public:
	// The ip address to bind the server to. Use * for all IP addresses
	String bind_address;

	// The port to bind the server to.
	int port;

	// The server identifier to use when responding to requests
	String server_identifier;
	
	// Getters for properties
	String get_bind_address() const;
	int get_port() const;
	String get_server_identifier() const;

	void set_bind_address(String p_bind_address);
	void set_port(int p_port);
	void set_server_identifier(String p_server_identifier);

	StreamableHttpServer();
	~StreamableHttpServer();

	void _notification(int p_what);
	void _process(double delta);

	// Register a new router to handle a specific path
	void register_router(const String &path, const Ref<HttpRouter> &router);

	// Start the server
	void start();

	// Stop the server and disconnect all clients
	void stop();

	// Send a stream message to client
	void send_stream_message(Ref<StreamPeerTCP> client, const String &event, const String &data);

private:
	bool _logging;
	bool _cors_enabled;
	Ref<TCPServer> _server;
	List<Ref<StreamPeerTCP>> _clients;
	Vector<StreamableRouterInfo> _routers;
	Ref<RegEx> _method_regex;
	Ref<RegEx> _header_regex;
	String _local_base_path;
	Vector<String> _allowed_origins;
	String _access_control_allowed_methods;
	String _access_control_allowed_headers;

	// Print a debug message in console, if the debug mode is enabled
	void _print_debug(const String &message);

	// Handle possibly incoming requests
	void _remove_disconnected_clients();

	// Interpret a request string and perform the request
	void _handle_request(Ref<StreamPeerTCP> client, const String &request_string);

	// Handle a specific request and send it to a router
	void _perform_current_request(Ref<StreamPeerTCP> client, Ref<HttpRequest> request);

	// Converts a URL path to RegExp, providing a mechanism to fetch groups from the expression
	Vector<String> _path_to_regexp_params(const String &path);

	// Enable CORS (Cross-origin resource sharing) which only allows requests from the specified servers
	void enable_cors(const Vector<String> &allowed_origins, const String &access_control_allowed_methods = "POST, GET, OPTIONS", const String &access_control_allowed_headers = "content-type");

	// Extracts query parameters from a String query,
	HashMap<String, Variant> _extract_query_params(const String &query_string);
};