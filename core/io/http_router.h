#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "http_request.h"
#include "http_response.h"

class HttpRouter : public RefCounted {
	GDCLASS(HttpRouter, RefCounted);

protected:
	static void _bind_methods();

public:
	HttpRouter();
	~HttpRouter();

	// Handle different HTTP methods
	virtual void handle_request(Ref<HttpRequest> request, Ref<HttpResponse> response);
	virtual void handle_get(Ref<HttpRequest> request, Ref<HttpResponse> response);
	virtual void handle_post(Ref<HttpRequest> request, Ref<HttpResponse> response);
	virtual void handle_head(Ref<HttpRequest> request, Ref<HttpResponse> response);
	virtual void handle_put(Ref<HttpRequest> request, Ref<HttpResponse> response);
	virtual void handle_patch(Ref<HttpRequest> request, Ref<HttpResponse> response);
	virtual void handle_delete(Ref<HttpRequest> request, Ref<HttpResponse> response);
	virtual void handle_options(Ref<HttpRequest> request, Ref<HttpResponse> response);
};