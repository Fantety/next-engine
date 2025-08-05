/*
 * @FilePath: \editor\ai_component\mcp\http_server.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-08-05 09:49:09
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-05 13:30:08
 */
#include "http_server.h"
#include "core/os/time.h"
#include "core/string/print_string.h"
#include "core/io/json.h"


void HttpServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_router", "path", "router"), &HttpServer::register_router);
	ClassDB::bind_method(D_METHOD("start"), &HttpServer::start);
	ClassDB::bind_method(D_METHOD("stop"), &HttpServer::stop);
	ClassDB::bind_method(D_METHOD("enable_cors", "allowed_origins", "access_control_allowed_methods", "access_control_allowed_headers"), &HttpServer::enable_cors, DEFVAL("POST, GET, OPTIONS"), DEFVAL("content-type"));
	
	ClassDB::bind_method(D_METHOD("_process", "delta"), &HttpServer::_process);
	
	ClassDB::bind_method(D_METHOD("get_bind_address"), &HttpServer::get_bind_address);
	ClassDB::bind_method(D_METHOD("get_port"), &HttpServer::get_port);
	ClassDB::bind_method(D_METHOD("get_server_identifier"), &HttpServer::get_server_identifier);
	ClassDB::bind_method(D_METHOD("set_bind_address", "bind_adress"), &HttpServer::set_bind_address);
	ClassDB::bind_method(D_METHOD("set_port", "port"), &HttpServer::set_port);
	ClassDB::bind_method(D_METHOD("set_server_identifier", "server_identifier"), &HttpServer::set_server_identifier);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bind_address"), "set_bind_address", "get_bind_address");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "port"), "set_port", "get_port");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "server_identifier"), "set_server_identifier", "get_server_identifier");
}

HttpServer::HttpServer() :
		bind_address("*"),
		port(8080),
		server_identifier("GodotTPD"),
		_logging(false),
		_cors_enabled(false) {
	
	
	_access_control_allowed_methods = "POST, GET, OPTIONS";
	_access_control_allowed_headers = "content-type";
	
	set_process(false);
}

String HttpServer::get_bind_address() const {
	return bind_address;
}

int HttpServer::get_port() const {
	return port;
}

String HttpServer::get_server_identifier() const {
	return server_identifier;
}


void HttpServer::set_bind_address(String p_bind_address){
	bind_address = p_bind_address;
}

void HttpServer::set_port(int p_port){
	port = p_port;
}

void HttpServer::set_server_identifier(String p_server_identifier){
	server_identifier = p_server_identifier;
}

HttpServer::~HttpServer() {
	stop();
}

void HttpServer::_notification(int p_what) {
	Node::_notification(p_what);
}

void HttpServer::_process(double delta) {
	if (_server.is_valid()) {
		while (_server->is_connection_available()) {
			Ref<StreamPeerTCP> new_client = _server->take_connection();
			if (new_client.is_valid()) {
				_clients.push_back(new_client);
			}
		}
		
		List<Ref<StreamPeerTCP>>::Element *E = _clients.front();
		while (E) {
			Ref<StreamPeerTCP> client = E->get();
			List<Ref<StreamPeerTCP>>::Element *N = E->next();
			
			client->poll();
			if (client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
				int32_t bytes = client->get_available_bytes();
				if (bytes > 0) {
					String request_string = client->get_string(bytes);
					Error err = client->get_status() == StreamPeerTCP::STATUS_CONNECTED ? OK : FAILED;
					if (err == OK) {
						_handle_request(client, request_string);
					}
				}
			}
			E = N;
		}
		
		_remove_disconnected_clients();
	}
}

void HttpServer::register_router(const String &path, const Ref<HttpRouter> &router) {
	RouterInfo router_info;
	router_info.path.instantiate();
	
	if (path.length() > 0 && path[0] == '^') {
		router_info.path->compile(path);
	} else {
		Vector<String> params = _path_to_regexp_params(path);
		router_info.params = params;
		
		// For simplicity, we're not implementing the full regex conversion here
		// In a real implementation, you would convert the path to a regex pattern
		String regex_pattern = path;
		// Replace :param with a regex pattern
		regex_pattern = regex_pattern.replace(":", "(?<");
		regex_pattern = regex_pattern.replace("/", ">[^/#?]+?)/");
		if (regex_pattern.length() > 0 && regex_pattern[regex_pattern.length() - 1] == '/') {
			regex_pattern = regex_pattern.substr(0, regex_pattern.length() - 1);
		}
		regex_pattern = "^" + regex_pattern + "[/#?]?$";
		
		router_info.path->compile(regex_pattern);
	}
	
	router_info.router = router;
	_routers.push_back(router_info);
}

void HttpServer::start() {
	_method_regex.instantiate();
	_header_regex.instantiate();
	_method_regex->compile("^(?<method>GET|POST|HEAD|PUT|PATCH|DELETE|OPTIONS) (?<path>[^ ]+) HTTP/1.1$");
	_header_regex->compile("^(?<key>[\w-]+): (?<value>(.*))$");
	set_process(true);
	_server.instantiate();
	Error err = _server->listen(port, bind_address);
	
	if (err == OK) {
		_print_debug("HTTP Server listening on http://" + bind_address + ":" + itos(port));
	} else {
		_print_debug("Could not bind to port " + itos(port) + ", error: " + itos(err));
		stop();
	}
}

void HttpServer::stop() {
	for (List<Ref<StreamPeerTCP>>::Element *E = _clients.front(); E; E = E->next()) {
		Ref<StreamPeerTCP> client = E->get();
		if (client.is_valid()) {
			client->disconnect_from_host();
		}
	}
	_clients.clear();
	
	if (_server.is_valid()) {
		_server->stop();
	}
	
	set_process(false);
	_print_debug("Server stopped.");
}

void HttpServer::_print_debug(const String &message) {
	if (_logging) {
		Dictionary time_dict = Time::get_singleton()->get_datetime_dict_from_system();
		String time_return = "";
		
		// Format the time string
		if (time_dict.has("year") && time_dict.has("month") && time_dict.has("day") &&
		    time_dict.has("hour") && time_dict.has("minute") && time_dict.has("second")) {
			time_return = vformat("%02d-%02d-%02d %02d:%02d:%02d", 
			                      time_dict["year"], time_dict["month"], time_dict["day"],
			                      time_dict["hour"], time_dict["minute"], time_dict["second"]);
		}
		
		print_line("[SERVER] " + time_return + " >> " + message);
	}
}

void HttpServer::_remove_disconnected_clients() {
	List<Ref<StreamPeerTCP>>::Element *E = _clients.front();
	while (E) {
		Ref<StreamPeerTCP> client = E->get();
		List<Ref<StreamPeerTCP>>::Element *N = E->next();
		
		if (client->get_status() != StreamPeerTCP::STATUS_CONNECTED && 
		    client->get_status() != StreamPeerTCP::STATUS_CONNECTING) {
			_clients.erase(E);
		}
		E = N;
	}
}

void HttpServer::_handle_request(Ref<StreamPeerTCP> client, const String &request_string) {
	Ref<HttpRequest> request;
	request.instantiate();
	
	Vector<String> lines = request_string.split("\r\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		Ref<RegExMatch> method_matches = _method_regex->search(line);
		Ref<RegExMatch> header_matches = _header_regex->search(line);
		
		if (method_matches.is_valid()) {
			request->method = method_matches->get_string("method");
			String request_path = method_matches->get_string("path");
			
			// Check if request_path contains "?" character, could be a query parameter
			int query_pos = request_path.find("?");
			if (query_pos == -1) {
				request->path = request_path;
			} else {
				request->path = request_path.substr(0, query_pos);
				String query_string = request_path.substr(query_pos + 1, request_path.length() - query_pos - 1);
				request->query = _extract_query_params(query_string);
			}
			
			request->headers.clear();
			request->body = "";
		} else if (header_matches.is_valid()) {
			request->headers[header_matches->get_string("key")] = header_matches->get_string("value");
		} else {
			request->body += line;
		}
	}
	
	_perform_current_request(client, request);
}

void HttpServer::_perform_current_request(Ref<StreamPeerTCP> client, Ref<HttpRequest> request) {
	// Convert HashMap<String, String> to Dictionary for JSON::stringify
	Dictionary headers_dict;
	for (const KeyValue<String, String> &kv : request->headers) {
		headers_dict[kv.key] = kv.value;
	}
	_print_debug("HTTP Request: " + String(JSON::stringify(headers_dict)));
	
	bool found = false;
	bool is_allowed_origin = false;
	Ref<HttpResponse> response;
	response.instantiate();
	String fetch_mode = "";
	String origin = "";
	response->client = client.ptr();
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
	
	if (_cors_enabled) {
		for (int i = 0; i < _allowed_origins.size(); i++) {
			if (_allowed_origins[i] == origin) {
				is_allowed_origin = true;
				response->access_control_origin = origin;
				break;
			}
		}
		
		response->access_control_allowed_methods = _access_control_allowed_methods;
		response->access_control_allowed_headers = _access_control_allowed_headers;
	}
	
	for (int i = 0; i < _routers.size(); i++) {
		RouterInfo router_info = _routers[i];
		Ref<RegExMatch> matches = router_info.path->search(request->path);
		
		if (matches.is_valid()) {
			request->query_match = matches;
			
			// Handle subpath if needed
			// In a full implementation, you would handle subpath extraction here
			
			// Handle parameters
			for (int j = 0; j < router_info.params.size(); j++) {
				String param = router_info.params[j];
				request->parameters[param] = matches->get_string(param);
			}
			
			// Handle different HTTP methods
			if (request->method == "GET") {
				found = true;
				router_info.router->handle_get(request, response);
			} else if (request->method == "POST") {
				found = true;
				router_info.router->handle_post(request, response);
			} else if (request->method == "HEAD") {
				found = true;
				router_info.router->handle_head(request, response);
			} else if (request->method == "PUT") {
				found = true;
				router_info.router->handle_put(request, response);
			} else if (request->method == "PATCH") {
				found = true;
				router_info.router->handle_patch(request, response);
			} else if (request->method == "DELETE") {
				found = true;
				router_info.router->handle_delete(request, response);
			} else if (request->method == "OPTIONS") {
				if (_cors_enabled && _allowed_origins.size() > 0 && fetch_mode == "cors") {
					if (is_allowed_origin) {
						response->send(204);
					} else {
						response->send(400, origin + " is not present in the allowed origins");
					}
					return;
				}
				
				found = true;
				router_info.router->handle_options(request, response);
			}
			
			break;
		}
	}
	
	if (!found) {
		response->send(404, "Not found");
	}
}

Vector<String> HttpServer::_path_to_regexp_params(const String &path) {
	Vector<String> params;
	Vector<String> fragments = path.split("/");
	
	// Skip the first empty fragment
	for (int i = 1; i < fragments.size(); i++) {
		String fragment = fragments[i];
		if (fragment.length() > 0 && fragment[0] == ':') {
			String param = fragment.substr(1, fragment.length() - 1);
			params.push_back(param);
		}
	}
	
	return params;
}

void HttpServer::enable_cors(const Vector<String> &allowed_origins, const String &access_control_allowed_methods, const String &access_control_allowed_headers) {
	_cors_enabled = true;
	_allowed_origins = allowed_origins;
	_access_control_allowed_methods = access_control_allowed_methods;
	_access_control_allowed_headers = access_control_allowed_headers;
}

HashMap<String, Variant> HttpServer::_extract_query_params(const String &query_string) {
	HashMap<String, Variant> query;
	if (query_string.is_empty()) {
		return query;
	}
	
	Vector<String> parameters = query_string.split("&");
	for (int i = 0; i < parameters.size(); i++) {
		String param = parameters[i];
		int equal_pos = param.find("=");
		
		if (equal_pos != -1) {
			String key = param.substr(0, equal_pos);
			String value = param.substr(equal_pos + 1, param.length() - equal_pos - 1);
			
			// Try to parse as int or float
			if (value.is_valid_int()) {
				query[key] = value.to_int();
			} else if (value.is_valid_float()) {
				query[key] = value.to_float();
			} else {
				query[key] = value;
			}
		}
	}
	
	return query;
}