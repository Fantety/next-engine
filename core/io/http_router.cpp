#include "http_router.h"

void HttpRouter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("handle_request", "request", "response"), &HttpRouter::handle_request);
}

HttpRouter::HttpRouter() {}

HttpRouter::~HttpRouter() {}

void HttpRouter::handle_request(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	String method = request->get_method();
	if (method == "GET") {
		handle_get(request, response);
	} else if (method == "POST") {
		handle_post(request, response);
	} else if (method == "HEAD") {
		handle_head(request, response);
	} else if (method == "PUT") {
		handle_put(request, response);
	} else if (method == "PATCH") {
		handle_patch(request, response);
	} else if (method == "DELETE") {
		handle_delete(request, response);
	} else if (method == "OPTIONS") {
		handle_options(request, response);
	} else {
		response->send(405, "Method not allowed");
	}
}

void HttpRouter::handle_get(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	response->send(405, "GET not allowed");
}

void HttpRouter::handle_post(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	response->send(405, "POST not allowed");
}

void HttpRouter::handle_head(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	response->send(405, "HEAD not allowed");
}

void HttpRouter::handle_put(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	response->send(405, "PUT not allowed");
}

void HttpRouter::handle_patch(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	response->send(405, "PATCH not allowed");
}

void HttpRouter::handle_delete(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	response->send(405, "DELETE not allowed");
}

void HttpRouter::handle_options(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	response->send(405, "OPTIONS not allowed");
}