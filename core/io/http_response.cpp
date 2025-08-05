#include "http_response.h"
#include "core/io/json.h"
#include "core/io/stream_peer.h"

void HttpResponse::_bind_methods() {
	ClassDB::bind_method(D_METHOD("send_raw", "status_code", "data", "content_type"), &HttpResponse::send_raw, DEFVAL(Vector<uint8_t>()), DEFVAL("application/octet-stream"));
	ClassDB::bind_method(D_METHOD("send", "status_code", "data", "content_type"), &HttpResponse::send, DEFVAL(""), DEFVAL("text/html"));
	ClassDB::bind_method(D_METHOD("json", "status_code", "data"), &HttpResponse::json);
	ClassDB::bind_method(D_METHOD("set_header", "field", "value"), &HttpResponse::set_header);
	ClassDB::bind_method(D_METHOD("cookie", "name", "value", "options"), &HttpResponse::cookie, DEFVAL(Dictionary()));
}

HttpResponse::HttpResponse() {
	server_identifier = "GodotTPD";
	access_control_origin = "*";
	access_control_allowed_methods = "POST, GET, OPTIONS";
	access_control_allowed_headers = "content-type";
}

HttpResponse::~HttpResponse() {}

void HttpResponse::send_raw(int status_code, const Vector<uint8_t> &data, const String &content_type) {
	if (!client) return;

	// Status line
	String response = "HTTP/1.1 " + itos(status_code) + " " + _match_status_code(status_code) + "\r\n";
	Vector<uint8_t> response_data = response.to_ascii_buffer();
	if (response_data.size() > 0) {
		client->put_data(response_data.ptr(), response_data.size());
	}

	// Server identifier
	response = "Server: " + server_identifier + "\r\n";
	response_data = response.to_ascii_buffer();
	if (response_data.size() > 0) {
		client->put_data(response_data.ptr(), response_data.size());
	}

	// Headers
	for (const KeyValue<String, String> &header : headers) {
		response = header.key + ": " + header.value + "\r\n";
		response_data = response.to_ascii_buffer();
		if (response_data.size() > 0) {
			client->put_data(response_data.ptr(), response_data.size());
		}
	}

	// Cookies
	for (const String &cookie_str : cookies) {
		response = "Set-Cookie: " + cookie_str + "\r\n";
		response_data = response.to_ascii_buffer();
		if (response_data.size() > 0) {
			client->put_data(response_data.ptr(), response_data.size());
		}
	}

	// Content-Length
	response = "Content-Length: " + itos(data.size()) + "\r\n";
	response_data = response.to_ascii_buffer();
	if (response_data.size() > 0) {
		client->put_data(response_data.ptr(), response_data.size());
	}

	// Connection
	response = "Connection: close\r\n";
	response_data = response.to_ascii_buffer();
	if (response_data.size() > 0) {
		client->put_data(response_data.ptr(), response_data.size());
	}

	// CORS headers
	response = "Access-Control-Allow-Origin: " + access_control_origin + "\r\n";
	response_data = response.to_ascii_buffer();
	if (response_data.size() > 0) {
		client->put_data(response_data.ptr(), response_data.size());
	}
	response = "Access-Control-Allow-Methods: " + access_control_allowed_methods + "\r\n";
	response_data = response.to_ascii_buffer();
	if (response_data.size() > 0) {
		client->put_data(response_data.ptr(), response_data.size());
	}
	response = "Access-Control-Allow-Headers: " + access_control_allowed_headers + "\r\n";
	response_data = response.to_ascii_buffer();
	if (response_data.size() > 0) {
		client->put_data(response_data.ptr(), response_data.size());
	}

	// Content-Type
	response = "Content-Type: " + content_type + "\r\n\r\n";
	response_data = response.to_ascii_buffer();
	if (response_data.size() > 0) {
		client->put_data(response_data.ptr(), response_data.size());
	}

	// Body
	if (data.size() > 0) {
		client->put_data(data.ptr(), data.size());
	}
}

void HttpResponse::send(int status_code, const String &data, const String &content_type) {
	send_raw(status_code, data.to_ascii_buffer(), content_type);
}

void HttpResponse::json(int status_code, const Variant &data) {
	String json_string = JSON::stringify(data);
	send(status_code, json_string, "application/json");
}

void HttpResponse::set_header(const String &field, const Variant &value) {
	headers[field] = value.operator String();
}

void HttpResponse::cookie(const String &name, const String &value, const Dictionary &options) {
	String cookie_str = name + "=" + value;

	if (options.has("domain")) cookie_str += "; Domain=" + String(options["domain"]);
	if (options.has("max-age")) cookie_str += "; Max-Age=" + String(options["max-age"]);
	if (options.has("expires")) cookie_str += "; Expires=" + String(options["expires"]);
	if (options.has("path")) cookie_str += "; Path=" + String(options["path"]);
	if (options.has("secure")) cookie_str += "; Secure=" + String(options["secure"]);
	if (options.has("httpOnly")) cookie_str += "; HttpOnly=" + String(options["httpOnly"]);

	if (options.has("sameSite")) {
		Variant same_site = options["sameSite"];
		if (same_site.get_type() == Variant::BOOL && same_site.operator bool()) {
			cookie_str += "; SameSite=Strict";
		} else if (same_site.get_type() == Variant::STRING) {
			String same_site_str = same_site.operator String();
			if (same_site_str == "lax") {
				cookie_str += "; SameSite=Lax";
			} else if (same_site_str == "strict") {
				cookie_str += "; SameSite=Strict";
			} else if (same_site_str == "none") {
				cookie_str += "; SameSite=None";
			}
		}
	}

	cookies.push_back(cookie_str);
}

String HttpResponse::_match_status_code(int code) {
	switch (code) {
		// 1xx - Informational Responses
		case 100: return "Continue";
		case 101: return "Switching protocols";
		case 102: return "Processing";
		case 103: return "Early Hints";

		// 2xx - Successful Responses
		case 200: return "OK";
		case 201: return "Created";
		case 202: return "Accepted";
		case 203: return "Non-Authoritative Information";
		case 204: return "No Content";
		case 205: return "Reset Content";
		case 206: return "Partial Content";
		case 207: return "Multi-Status";
		case 208: return "Already Reported";
		case 226: return "IM Used";

		// 3xx - Redirection Messages
		case 300: return "Multiple Choices";
		case 301: return "Moved Permanently";
		case 302: return "Found (Previously 'Moved Temporarily')";
		case 303: return "See Other";
		case 304: return "Not Modified";
		case 305: return "Use Proxy";
		case 306: return "Switch Proxy";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";

		// 4xx - Client Error Responses
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 402: return "Payment Required";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 406: return "Not Acceptable";
		case 407: return "Proxy Authentication Required";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 410: return "Gone";
		case 411: return "Length Required";
		case 412: return "Precondition Failed";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 415: return "Unsupported Media Type";
		case 416: return "Range Not Satisfiable";
		case 417: return "Expectation Failed";
		case 418: return "I'm a Teapot";
		case 421: return "Misdirected Request";
		case 422: return "Unprocessable Entity";
		case 423: return "Locked";
		case 424: return "Failed Dependency";
		case 425: return "Too Early";
		case 426: return "Upgrade Required";
		case 428: return "Precondition Required";
		case 429: return "Too Many Requests";
		case 431: return "Request Header Fields Too Large";
		case 451: return "Unavailable For Legal Reasons";

		// 5xx - Server Error Responses
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		case 506: return "Variant Also Negotiates";
		case 507: return "Insufficient Storage";
		case 508: return "Loop Detected";
		case 510: return "Not Extended";
		case 511: return "Network Authentication Required";

		default: return "OK";
	}
}