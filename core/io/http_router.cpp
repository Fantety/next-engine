#include "http_router.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/io/http_request.h"
#include "core/io/http_response.h"

void HttpRouter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("handle_get", "request", "response"), &HttpRouter::handle_get);
	ClassDB::bind_method(D_METHOD("handle_post", "request", "response"), &HttpRouter::handle_post);
	ClassDB::bind_method(D_METHOD("handle_head", "request", "response"), &HttpRouter::handle_head);
	ClassDB::bind_method(D_METHOD("handle_put", "request", "response"), &HttpRouter::handle_put);
	ClassDB::bind_method(D_METHOD("handle_patch", "request", "response"), &HttpRouter::handle_patch);
	ClassDB::bind_method(D_METHOD("handle_delete", "request", "response"), &HttpRouter::handle_delete);
	ClassDB::bind_method(D_METHOD("handle_options", "request", "response"), &HttpRouter::handle_options);
}

HttpRouter::HttpRouter() {}

HttpRouter::~HttpRouter() {}

void HttpRouter::handle_get(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) {
	response->send(405, "GET not allowed");
}

void HttpRouter::handle_post(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) {
	response->send(405, "POST not allowed");
}

void HttpRouter::handle_head(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) {
	response->send(405, "HEAD not allowed");
}

void HttpRouter::handle_put(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) {
	response->send(405, "PUT not allowed");
}

void HttpRouter::handle_patch(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) {
	response->send(405, "PATCH not allowed");
}

void HttpRouter::handle_delete(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) {
	response->send(405, "DELETE not allowed");
}

void HttpRouter::handle_options(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) {
	response->send(405, "OPTIONS not allowed");
}