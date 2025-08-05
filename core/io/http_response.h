#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/variant/variant.h"

class StreamPeer;

class HttpResponse : public RefCounted {
	GDCLASS(HttpResponse, RefCounted);

protected:
	static void _bind_methods();

public:
	// The client currently talking to the server
	StreamPeer *client;

	// The server identifier to use on responses
	String server_identifier;

	// A dictionary of headers
	HashMap<String, String> headers;

	// An array of cookies
	List<String> cookies;

	// Origins allowed to call this resource
	String access_control_origin;

	// Comma separated methods for the access control
	String access_control_allowed_methods;

	// Comma separated headers for the access control
	String access_control_allowed_headers;

	HttpResponse();
	~HttpResponse();

	// Send out a raw (Bytes) response to the client
	void send_raw(int status_code, const Vector<uint8_t> &data = Vector<uint8_t>(), const String &content_type = "application/octet-stream");

	// Send out a response to the client
	void send(int status_code, const String &data = "", const String &content_type = "text/html");

	// Send out a JSON response to the client
	void json(int status_code, const Variant &data);

	// Sets the response's header "field" to "value"
	void set_header(const String &field, const Variant &value);

	// Sets cookie "name" to "value"
	void cookie(const String &name, const String &value, const Dictionary &options = Dictionary());

private:
	// Automatically matches a "status_code" to an RFC 7231 compliant "status_text"
	String _match_status_code(int code);
};