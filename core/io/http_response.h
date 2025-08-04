#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"
#include "core/io/stream_peer.h"
#include "core/variant/array.h"

#include "core/object/class_db.h"

/**
 * @brief A response object useful to send out responses
 */
class HttpResponse : public RefCounted {
	GDCLASS(HttpResponse, RefCounted);

public:
	/// The client currently talking to the server
	Ref<StreamPeer> client;

	/// The server identifier to use on responses [GodotTPD]
	String server_identifier = "GodotTPD";

	/// A dictionary of headers
	/// Headers can be set using the `set(name, value)` function
	Dictionary headers;

	/// An array of cookies
	/// Cookies can be set using the `cookie(name, value, options)` function
	/// Cookies will be automatically sent via "Set-Cookie" headers to clients
	Array cookies;

	/// Origins allowed to call this resource
	String access_control_origin = "*";

	/// Comma separated methods for the access control
	String access_control_allowed_methods = "POST, GET, OPTIONS";

	/// Comma separated headers for the access control
	String access_control_allowed_headers = "content-type";

protected:
	static void _bind_methods();

public:
	HttpResponse();
	~HttpResponse();

	/// Send out a raw (Bytes) response to the client
	/// Useful to send files faster or raw data which will be converted by the client
	/// @param status_code - The HTTP Status code to send
	/// @param data - The body data to send
	/// @param content_type - The type of content to send.
	void send_raw(int status_code, const Vector<uint8_t>& data = Vector<uint8_t>(), const String& content_type = "application/octet-stream");

	/// Send out a response to the client
	/// @param status_code - The HTTP status code to send
	/// @param data - The body to send
	/// @param content_type - The type of the content to send
	void send(int status_code, const String& data = "", const String& content_type = "text/html");

	/// Send out a JSON response to the client
	/// This function will internally call the `send` method
	/// @param status_code - The HTTP status code to send
	/// @param data - The body to send
	void json(int status_code, const Variant& data);

	/// Sets the response's header "field" to "value"
	/// @param field - The name of the header. i.e. `Accept-Type`
	/// @param value - The value of this header. i.e. `application/json`
	void set(const StringName& field, const Variant& value);

	/// Sets cookie "name" to "value"
	/// @param name - The name of the cookie. i.e. `user-id`
	/// @param value - The value of this cookie. i.e. `abcdef`
	/// @param options - A Dictionary of cookie attributes for this specific cookie in the `{ "secure" : "true"}` format.
	void cookie(const String& name, const String& value, const Dictionary& options = Dictionary());

private:
	/// Automatically matches a "status_code" to an RFC 7231 compliant "status_text"
	/// @param code - The HTTP Status code to be matched
	/// Returns: the matched `status_text`
	String _match_status_code(int code);
};

#endif // HTTP_RESPONSE_H