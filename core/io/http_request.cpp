#include "http_request.h"
#include "core/variant/json.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "core/string/print_string.h"

void HttpRequest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_body_parsed"), &HttpRequest::get_body_parsed);
	ClassDB::bind_method(D_METHOD("to_string"), &HttpRequest::to_string);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "headers"), "", "headers");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "body"), "", "body");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "query_match", PROPERTY_HINT_RESOURCE_TYPE, "RegExMatch"), "", "query_match");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path"), "", "path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "method"), "", "method");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "parameters"), "", "parameters");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "query"), "", "query");
}

HttpRequest::HttpRequest() {}

HttpRequest::~HttpRequest() {}

Variant HttpRequest::get_body_parsed() const {
	String content_type = "";

	if (headers.has("content-type")) {
		content_type = headers["content-type"];
	} else if (headers.has("Content-Type")) {
		content_type = headers["Content-Type"];
	}

	if (content_type == "application/json") {
		return JSON::parse_string(body);
	}

	if (content_type == "application/x-www-form-urlencoded") {
		Dictionary data;
		Array body_parts = body.split("&");
		for (int i = 0; i < body_parts.size(); ++i) {
			Array key_and_value = body_parts[i].split("=");
			if (key_and_value.size() == 2) {
				data[key_and_value[0]] = key_and_value[1];
			}
		}
		return data;
	}

	// Not supported content type parsing... for now
	return Variant();
}

String HttpRequest::to_string() const {
	Dictionary dict;
	dict["headers"] = headers;
	dict["method"] = method;
	dict["path"] = path;
	return JSON::stringify(dict);
}