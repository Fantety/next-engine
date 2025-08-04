#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"
#include "core/regex.h"

#include "core/object/class_db.h"

/**
 * @brief An HTTP request received by the server
 */
class HttpRequest : public RefCounted {
	GDCLASS(HttpRequest, RefCounted);

public:
	/// A dictionary of the headers of the request
	Dictionary headers;

	/// The received raw body
	String body;

	/// A match object of the regular expression that matches the path
	Ref<RegExMatch> query_match;

	/// The path that matches the router path
	String path;

	/// The method
	String method;

	/// A dictionary of request (aka. routing) parameters
	Dictionary parameters;

	/// A dictionary of request query parameters
	Dictionary query;

protected:
	static void _bind_methods();

public:
	HttpRequest();
	~HttpRequest();

	/// Returns the body object based on the raw body and the content type of the request
	Variant get_body_parsed() const;

	/// Override `str()` method, automatically called in `print()` function
	String to_string() const;
};

#endif // HTTP_REQUEST_H