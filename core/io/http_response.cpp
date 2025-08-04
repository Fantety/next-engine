#include "http_response.h"
#include "core/io/stream_peer.h"
#include "core/variant/json.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/string/print_string.h"
#include "core/variant/variant.h"

void HttpResponse::_bind_methods() {
	ClassDB::bind_method(D_METHOD("send_raw", "status_code", "data", "content_type"), &HttpResponse::send_raw, DEFVAL(Vector<uint8_t>()), DEFVAL("application/octet-stream"));
	ClassDB::bind_method(D_METHOD("send", "status_code", "data", "content_type"), &HttpResponse::send, DEFVAL(""), DEFVAL("text/html"));
	ClassDB::bind_method(D_METHOD("json", "status_code", "data"), &HttpResponse::json);
	ClassDB::bind_method(D_METHOD("set", "field", "value"), &HttpResponse::set);
	ClassDB::bind_method(D_METHOD("cookie", "name", "value", "options"), &HttpResponse::cookie, DEFVAL(Dictionary()));

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "client", PROPERTY_HINT_RESOURCE_TYPE, "StreamPeer"), "", "client");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "server_identifier"), "", "server_identifier");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "headers"), "", "headers");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "cookies"), "", "cookies");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "access_control_origin"), "", "access_control_origin");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "access_control_allowed_methods"), "", "access_control_allowed_methods");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "access_control_allowed_headers"), "", "access_control_allowed_headers");
}

HttpResponse::HttpResponse() {}

HttpResponse::~HttpResponse() {}

void HttpResponse::send_raw(int status_code, const Vector<uint8_t>& data, const String& content_type) {
	if (client.is_valid()) {
		String status_line = "HTTP/1.1 " + itos(status_code) + " " + _match_status_code(status_code) + "\r\n";
		client->put_data(status_line.to_ascii_buffer());
		String server_line = "Server: " + server_identifier + "\r\n";
		client->put_data(server_line.to_ascii_buffer());
		for (const Variant* key : headers.get_field_list()) {
			String header_line = key->operator String() + ": " + headers[*key].operator String() + "\r\n";
			client->put_data(header_line.to_ascii_buffer());
		}
		for (int i = 0; i < cookies.size(); ++i) {
			String cookie_line = "Set-Cookie: " + cookies[i].operator String() + "\r\n";
			client->put_data(cookie_line.to_ascii_buffer());
		}
		String length_line = "Content-Length: " + itos(data.size()) + "\r\n";
		client->put_data(length_line.to_ascii_buffer());
		client->put_data("Connection: close\r\n".to_ascii_buffer());
		String access_control_origin_line = "Access-Control-Allow-Origin: " + access_control_origin + "\r\n";
		client->put_data(access_control_origin_line.to_ascii_buffer());
		String access_control_methods_line = "Access-Control-Allow-Methods: " + access_control_allowed_methods + "\r\n";
		client->put_data(access_control_methods_line.to_ascii_buffer());
		String access_control_headers_line = "Access-Control-Allow-Headers: " + access_control_allowed_headers + "\r\n";
		client->put_data(access_control_headers_line.to_ascii_buffer());
		String content_type_line = "Content-Type: " + content_type + "\r\n\r\n";
		client->put_data(content_type_line.to_ascii_buffer());
		client->put_data(data);
	}
}

void HttpResponse::send(int status_code, const String& data, const String& content_type) {
	send_raw(status_code, data.to_ascii_buffer(), content_type);
}

void HttpResponse::json(int status_code, const Variant& data) {
	send(status_code, JSON::stringify(data), "application/json");
}

void HttpResponse::set(const StringName& field, const Variant& value) {
	headers[field] = value;
}

void HttpResponse::cookie(const String& name, const String& value, const Dictionary& options) {
	String cookie_str = name + "=" + value;
	if (options.has("domain")) cookie_str += "; Domain=" + options["domain"].operator String();
	if (options.has("max-age")) cookie_str += "; Max-Age=" + options["max-age"].operator String();
	if (options.has("expires")) cookie_str += "; Expires=" + options["expires"].operator String();
	if (options.has("path")) cookie_str += "; Path=" + options["path"].operator String();
	if (options.has("secure")) cookie_str += "; Secure=" + options["secure"].operator String();
	if (options.has("httpOnly")) cookie_str += "; HttpOnly=" + options["httpOnly"].operator String();
	if (options.has("sameSite")) {
		Variant same_site = options["sameSite"];
		if (same_site.operator bool()) {
			cookie_str += "; SameSite=Strict";
		} else if (same_site.operator String() == "lax") {
			cookie_str += "; SameSite=Lax";
		} else if (same_site.operator String() == "strict") {
			cookie_str += "; SameSite=Strict";
		} else if (same_site.operator String() == "none") {
			cookie_str += "; SameSite=None";
		}
	}
	cookies.push_back(cookie_str);
}

String HttpResponse::_match_status_code(int code) {
	String text = "OK";
	switch (code) {
		// 1xx - Informational Responses
		case 100: text = "Continue"; break;
		case 101: text = "Switching protocols"; break;
		case 102: text = "Processing"; break;
		case 103: text = "Early Hints"; break;
		// 2xx - Successful Responses
		case 200: text = "OK"; break;
		case 201: text = "Created"; break;
		case 202: text = "Accepted"; break;
		case 203: text = "Non-Authoritative Information"; break;
		case 204: text = "No Content"; break;
		case 205: text = "Reset Content"; break;
		case 206: text = "Partial Content"; break;
		case 207: text = "Multi-Status"; break;
		case 208: text = "Already Reported"; break;
		case 226: text = "IM Used"; break;
		// 3xx - Redirection Messages
		case 300: text = "Multiple Choices"; break;
		case 301: text = "Moved Permanently"; break;
		case 302: text = "Found (Previously 'Moved Temporarily')"; break;
		case 303: text = "See Other"; break;
		case 304: text = "Not Modified"; break;
		case 305: text = "Use Proxy"; break;
		case 306: text = "Switch Proxy"; break;
		case 307: text = "Temporary Redirect"; break;
		case 308: text = "Permanent Redirect"; break;
		// 4xx - Client Error Responses
		case 400: text = "Bad Request"; break;
		case 401: text = "Unauthorized"; break;
		case 402: text = "Payment Required"; break;
		case 403: text = "Forbidden"; break;
		case 404: text = "Not Found"; break;
		case 405: text = "Method Not Allowed"; break;
		case 406: text = "Not Acceptable"; break;
		case 407: text = "Proxy Authentication Required"; break;
		case 408: text = "Request Timeout"; break;
		case 409: text = "Conflict"; break;
		case 410: text = "Gone"; break;
		case 411: text = "Length Required"; break;
		case 412: text = "Precondition Failed"; break;
		case 413: text = "Payload Too Large"; break;
		case 414: text = "URI Too Long"; break;
		case 415: text = "Unsupported Media Type"; break;
		case 416: text = "Range Not Satisfiable"; break;
		case 417: text = "Expectation Failed"; break;
		case 418: text = "I'm a Teapot"; break;
		case 421: text = "Misdirected Request"; break;
		case 422: text = "Unprocessable Entity"; break;
		case 423: text = "Locked"; break;
		case 424: text = "Failed Dependency"; break;
		case 425: text = "Too Early"; break;
		case 426: text = "Upgrade Required"; break;
		case 428: text = "Precondition Required"; break;
		case 429: text = "Too Many Requests"; break;
		case 431: text = "Request Header Fields Too Large"; break;
		case 451: text = "Unavailable For Legal Reasons"; break;
		// 5xx - Server Error Responses
		case 500: text = "Internal Server Error"; break;
		case 501: text = "Not Implemented"; break;
		case 502: text = "Bad Gateway"; break;
		case 503: text = "Service Unavailable"; break;
		case 504: text = "Gateway Timeout"; break;
		case 505: text = "HTTP Version Not Supported"; break;
		case 506: text = "Variant Also Negotiates"; break;
		case 507: text = "Insufficient Storage"; break;
		case 508: text = "Loop Detected"; break;
		case 510: text = "Not Extended"; break;
		case 511: text = "Network Authentication Required"; break;
		default: text = "Unknown Status"; break;
	}
	return text;
}