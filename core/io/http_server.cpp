#include "http_server.h"
#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/templates/list.h"
#include "core/os/time.h"
#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "core/io/http_request.h"
#include "core/io/http_response.h"
#include "core/io/http_router.h"
#include "core/io/http_file_router.h"
#include "core/regex.h"
#include "core/string/char_string.h"
#include "core/io/ip_address.h"
#include "core/string/node_path.h"

#include <iostream>

void HttpServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_router", "path", "router"), &HttpServer::register_router);
	ClassDB::bind_method(D_METHOD("start"), &HttpServer::start);
	ClassDB::bind_method(D_METHOD("stop"), &HttpServer::stop);
	ClassDB::bind_method(D_METHOD("set_bind_address", "address"), &HttpServer::bind_address);
	ClassDB::bind_method(D_METHOD("get_bind_address"), &HttpServer::bind_address);
	ClassDB::bind_method(D_METHOD("set_port", "port"), &HttpServer::port);
	ClassDB::bind_method(D_METHOD("get_port"), &HttpServer::port);
	ClassDB::bind_method(D_METHOD("set_server_identifier", "identifier"), &HttpServer::server_identifier);
	ClassDB::bind_method(D_METHOD("get_server_identifier"), &HttpServer::server_identifier);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bind_address"), "set_bind_address", "get_bind_address");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "server_identifier"), "set_server_identifier", "get_server_identifier");
}

HttpServer::HttpServer(bool p_logging) : _logging(p_logging) {
	_method_regex = Ref<RegEx>(memnew(RegEx));
	_method_regex->compile("^(?<method>GET|POST|HEAD|PUT|PATCH|DELETE|OPTIONS) (?<path>[^ ]+) HTTP/1.1$");
	_header_regex = Ref<RegEx>(memnew(RegEx));
	_header_regex->compile("^(?<key>[\\w-]+): (?<value>(.*))$");
	set_process(false);
}

HttpServer::~HttpServer() {
	stop();
}

void HttpServer::_print_debug(const String& message) {
	if (_logging) {
		Dictionary time_dict = Time::get_singleton()->get_datetime_dict_from_system();
		String time_return = vformat("%02d-%02d-%02d %02d:%02d:%02d", time_dict["year"], time_dict["month"], time_dict["day"], time_dict["hour"], time_dict["minute"], time_dict["second"]);
		print_line("[SERVER] " + time_return + " >> " + message);
	}
}

void HttpServer::register_router(const String& path, const Ref<HttpRouter>& router) {
	Ref<RegEx> path_regex = Ref<RegEx>(memnew(RegEx));
	Array params;
	if (path.length() > 0 && path[0] == '^') {
		path_regex->compile(path);
	} else {
		Array regexp = _path_to_regexp(path, router->is_class_ptr(HttpFileRouter::get_class_ptr_static()));
		path_regex->compile(regexp[0]);
		params = regexp[1];
	}
	RouterEntry entry;
	entry.path = path_regex;
	entry.params = params;
	entry.router = router;
	_routers.push_back(entry);
}

void HttpServer::_process(double delta) {
	if (_server.is_valid()) {
		while (_server->is_connection_available()) {
			Ref<StreamPeerTCP> new_client = _server->take_connection();
			if (new_client.is_valid()) {
				_clients.push_back(new_client);
			}
		}
		for (auto client_it = _clients.begin(); client_it != _clients.end(); ) {
			Ref<StreamPeerTCP> client = *client_it;
			client->poll();
			if (client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
				int bytes = client->get_available_bytes();
				if (bytes > 0) {
					CharString request_string = client->get_utf8_string(bytes);
					_handle_request(client, request_string);
				}
				++client_it;
			} else {
				client_it = _clients.erase(client_it);
			}
		}
		_remove_disconnected_clients();
	}
}

void HttpServer::_remove_disconnected_clients() {
	for (auto it = _clients.begin(); it != _clients.end(); ) {
		Ref<StreamPeerTCP> client = *it;
		if (client->get_status() != StreamPeerTCP::STATUS_CONNECTED && client->get_status() != StreamPeerTCP::STATUS_CONNECTING) {
			it = _clients.erase(it);
		} else {
			++it;
		}
	}
}

void HttpServer::start() {
	set_process(true);
	_server = Ref<TCPServer>(memnew(TCPServer));
	Error err = _server->listen(port, bind_address);
	switch (err) {
		case ERR_ALREADY_IN_USE:
			_print_debug("Could not bind to port " + itos(port) + ", already in use");
			stop();
			break;
		default:
			_print_debug("HTTP Server listening on http://" + bind_address + ":" + itos(port));
			break;
	}
}

void HttpServer::stop() {
	for (auto& client : _clients) {
		client->disconnect_from_host();
	}
	_clients.clear();
	if (_server.is_valid()) {
		_server->stop();
	}
	set_process(false);
	_print_debug("Server stopped.");
}

void HttpServer::_handle_request(const Ref<StreamPeer>& client, const String& request_string) {
	Ref<HttpRequest> request = Ref<HttpRequest>(memnew(HttpRequest));
	Array lines = request_string.split("\r\n");
	for (int i = 0; i < lines.size(); ++i) {
		String line = lines[i];
		Ref<RegExMatch> method_matches = _method_regex->search(line);
		Ref<RegExMatch> header_matches = _header_regex->search(line);
		if (method_matches.is_valid()) {
			request->method = method_matches->get_string("method");
			String request_path = method_matches->get_string("path");
			// Check if request_path contains "?" character, could be a query parameter
			if (!request_path.contains("?")) {
				request->path = request_path;
			} else {
				PackedStringArray path_query = request_path.split("?");
				request->path = path_query[0];
				request->query = _extract_query_params(path_query[1]);
			}
			request->headers = Dictionary();
			request->body = "";
		} else if (header_matches.is_valid()) {
			request->headers[header_matches->get_string("key")] = header_matches->get_string("value");
		} else {
			request->body += line;
		}
	}
	_perform_current_request(client, request);
}

void HttpServer::_perform_current_request(const Ref<StreamPeer>& client, const Ref<HttpRequest>& request) {
	_print_debug("HTTP Request: " + request->to_string());
	bool found = false;
	bool is_allowed_origin = false;
	Ref<HttpResponse> response = Ref<HttpResponse>(memnew(HttpResponse));
	String fetch_mode = "";
	String origin = "";
	response->client = client;
	response->server_identifier = server_identifier;

	if (request->headers.has("Sec-Fetch-Mode")) {
		fetch_mode = request->headers["Sec-Fetch-Mode"];
	} else if (request->headers.has("sec-fetch-mode")) {
		fetch_mode = request->headers["sec-fetch-mode"];
	}

	if (request->headers.has("Origin")) {
		origin = request->headers["Origin"];
	} else if (request->headers.has("origin")) {
		origin = request->headers["origin"];
	}

	if (_allowed_origins.has(origin)) {
		is_allowed_origin = true;
		response->access_control_origin = origin;
	}

	for (const RouterEntry& entry : _routers) {
		Ref<RegExMatch> path_match = entry.path->search(request->path);
		if (path_match.is_valid()) {
			found = true;
			request->query_match = path_match;
			request->parameters = Dictionary();
			for (int i = 0; i < entry.params.size(); ++i) {
				String param_name = entry.params[i];
				request->parameters[param_name] = path_match->get_string(param_name);
			}

			String method = request->method;
			if (method == "GET") {
				entry.router->handle_get(request, response);
			} else if (method == "POST") {
				entry.router->handle_post(request, response);
			} else if (method == "HEAD") {
				entry.router->handle_head(request, response);
			} else if (method == "PUT") {
				entry.router->handle_put(request, response);
			} else if (method == "PATCH") {
				entry.router->handle_patch(request, response);
			} else if (method == "DELETE") {
				entry.router->handle_delete(request, response);
			} else if (method == "OPTIONS") {
				entry.router->handle_options(request, response);
			} else {
				response->send(405, "Method not allowed");
			}
			break;
		}
	}

	if (!found) {
		response->send(404, "Not Found");
	}
}

Array HttpServer::_path_to_regexp(const String& path, bool is_file_router) {
	// This is a simplified version. In a real implementation, this would need to be more complex.
	Array result;
	String regex_path = path;
	// Escape special regex characters
	regex_path = regex_path.replace(".", "\\.");
	regex_path = regex_path.replace("(", "\\(");
	regex_path = regex_path.replace(")", "\\)");
	regex_path = regex_path.replace("[", "\\[");
	regex_path = regex_path.replace("]", "\\]");
	regex_path = regex_path.replace("{", "\\{");
	regex_path = regex_path.replace("}", "\\}");
	regex_path = regex_path.replace("+", "\\+");
	regex_path = regex_path.replace("?", "\\?");
	regex_path = regex_path.replace("$", "\\$");
	regex_path = regex_path.replace("^", "\\^");
	regex_path = regex_path.replace("|", "\\|");
	// Replace path parameters with regex groups
	regex_path = regex_path.replace("*", "(.*)");
	// For file routers, we might want to handle extensions differently
	if (is_file_router) {
		// Add a fallback for file routers
		regex_path = "^" + regex_path + "$";
	} else {
		regex_path = "^" + regex_path + "$";
	}
	result.push_back(regex_path);
	// In a real implementation, we would also extract parameter names here
	result.push_back(Array());
	return result;
}

Dictionary HttpServer::_extract_query_params(const String& query_string) {
	Dictionary params;
	Array pairs = query_string.split("&");
	for (int i = 0; i < pairs.size(); ++i) {
		Array key_value = pairs[i].split("=");
		if (key_value.size() == 2) {
			params[key_value[0]] = key_value[1];
		}
	}
	return params;
}