#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/variant/variant.h"
#include "modules/regex/regex.h"

class HttpRequest : public RefCounted {
	GDCLASS(HttpRequest, RefCounted);

protected:
	static void _bind_methods();

public:
	// A dictionary of the headers of the request
	HashMap<String, String> headers;

	Dictionary get_headers() const;

	// The received raw body
	String body;
	String get_body() const;

	// A match object of the regular expression that matches the path
	Ref<RegExMatch> query_match;
	Ref<RegExMatch> get_query_match() const;

	// The path that matches the router path
	String path;
	String get_path() const;

	// The method
	String method;
	String get_method() const;

	// A dictionary of request (aka. routing) parameters
	HashMap<String, String> parameters;
	Dictionary get_parameters() const;

	// A dictionary of request query parameters
	HashMap<String, Variant> query;
	Dictionary get_query() const;

	HttpRequest();
	~HttpRequest();

	// Returns the body object based on the raw body and the content type of the request
	Variant get_body_parsed() const;
};