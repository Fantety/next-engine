#include "http_request.h"
#include "core/io/json.h"

void HttpRequest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_body_parsed"), &HttpRequest::get_body_parsed);
	ClassDB::bind_method(D_METHOD("get_headers"), &HttpRequest::get_headers);
	ClassDB::bind_method(D_METHOD("get_body"), &HttpRequest::get_body);
	ClassDB::bind_method(D_METHOD("get_query_match"), &HttpRequest::get_query_match);
	ClassDB::bind_method(D_METHOD("get_path"), &HttpRequest::get_path);
	ClassDB::bind_method(D_METHOD("get_method"), &HttpRequest::get_method);
	ClassDB::bind_method(D_METHOD("get_parameters"), &HttpRequest::get_parameters);
	ClassDB::bind_method(D_METHOD("get_query"), &HttpRequest::get_query);
}

HttpRequest::HttpRequest() {}

HttpRequest::~HttpRequest() {}

Dictionary HttpRequest::get_headers() const {
	Dictionary result;
	for (const KeyValue<String, String> &kv : headers) {
		result[kv.key] = kv.value;
	}
	return result;
}

String HttpRequest::get_body() const {
	return body;
}

Ref<RegExMatch> HttpRequest::get_query_match() const {
	return query_match;
}

String HttpRequest::get_path() const {
	return path;
}

String HttpRequest::get_method() const {
	return method;
}

Dictionary HttpRequest::get_parameters() const {
	Dictionary result;
	for (const KeyValue<String, String> &kv : parameters) {
		result[kv.key] = kv.value;
	}
	return result;
}

Dictionary HttpRequest::get_query() const {
	Dictionary result;
	for (const KeyValue<String, Variant> &kv : query) {
		result[kv.key] = kv.value;
	}
	return result;
}

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

		Vector<String> body_parts = body.split("&");
		for (int i = 0; i < body_parts.size(); i++) {
			Vector<String> key_and_value = body_parts[i].split("=");
			if (key_and_value.size() == 2) {
				data[key_and_value[0]] = key_and_value[1];
			}
		}

		return data;
	}

	// Not supported content type parsing... for now
	return Variant();
}