#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "core/io/tcp_server.h"
#include "core/object/ref_counted.h"
#include "core/object/object.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/io/stream_peer_tcp.h"
#include "core/io/http_request.h"
#include "core/io/http_response.h"
#include "core/io/http_router.h"
#include "core/variant/variant.h"
#include "core/string/char_string.h"
#include "core/io/ip_address.h"

#include "core/object/class_db.h"
#include "core/error/error_macros.h"

#include "core/io/http_file_router.h"

#include "core/regex.h"



/**
 * @brief A routable HTTP server for Godot
 *
 * Provides a web server with routes for specific endpoints
 * Example usage:
 * ```cpp
 * HttpServer* server = memnew(HttpServer);
 * server->register_router("/", memnew(MyExampleRouter));
 * add_child(server);
 * server->start();
 * ```
 */
class HttpServer : public Object {
	GDCLASS(HttpServer, Object);

public:
	/// The ip address to bind the server to. Use * for all IP addresses [*]
	String bind_address = "*";

	/// The port to bind the server to. [8080]
	int port = 8080;

	/// The server identifier to use when responding to requests [GodotTPD]
	String server_identifier = "GodotTPD";

private:
	// If `HttpRequest`s and `HttpResponse`s should be logged
	bool _logging = false;

	// The TCP server instance used
	Ref<TCPServer> _server;

	// An array of StreamPeerTCP objects who are currently talking to the server
	List<Ref<StreamPeerTCP>> _clients;

	// A list of HttpRequest routers who could handle a request
	struct RouterEntry {
		Ref<RegEx> path;
		Array params;
		Ref<HttpRouter> router;
	};
	List<RouterEntry> _routers;

	// A regex identifying the method line
	Ref<RegEx> _method_regex;

	// A regex for header lines
	Ref<RegEx> _header_regex;

	// The base path used in a project to serve files
	String _local_base_path = "res://src";

	// list of host allowed to call the server
	PackedStringArray _allowed_origins;

	// Comma separated methods for the access control
	String _access_control_allowed_methods = "POST, GET, OPTIONS";

	// Comma separated headers for the access control
	String _access_control_allowed_headers = "content-type";

protected:
	static void _bind_methods();

public:
	HttpServer(bool p_logging = false);
	~HttpServer();

	/// Register a new router to handle a specific path
	/// @param path - The path the router will handle.
	/// Supports a regular expression and the group matches will be available in HttpRequest.query_match.
	/// @param router - The router which will handle the request
	void register_router(const String& path, const Ref<HttpRouter>& router);

	/// Handle possibly incoming requests
	void _process(double delta);

	/// Start the server
	void start();

	/// Stop the server and disconnect all clients
	void stop();

private:
	void _print_debug(const String& message);
	void _handle_request(const Ref<StreamPeer>& client, const String& request_string);
	void _perform_current_request(const Ref<StreamPeer>& client, const Ref<HttpRequest>& request);
	void _remove_disconnected_clients();
	Array _path_to_regexp(const String& path, bool is_file_router);
	Dictionary _extract_query_params(const String& query_string);
};

#endif // HTTP_SERVER_H