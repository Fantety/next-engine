#ifndef HTTP_ROUTER_H
#define HTTP_ROUTER_H

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/object/class_db.h"

#include "core/io/http_request.h"
#include "core/io/http_response.h"

/**
 * @brief A base class for all HTTP routers
 *
 * This router handles all the requests that the client sends to the server.
 * NOTE: This class is meant to be expanded upon instead of used directly.
 * Usage:
 * ```cpp
 * class MyCustomRouter : public HttpRouter {
 *     GDCLASS(MyCustomRouter, HttpRouter);
 *
 * public:
 *     void handle_get(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) override {
 *         response->send(200, "Hello World");
 *     }
 * };
 * ```
 */
class HttpRouter : public RefCounted {
	GDCLASS(HttpRouter, RefCounted);

protected:
	static void _bind_methods();

public:
	HttpRouter();
	~HttpRouter();

	/// Handle a GET request
	/// @param request - The request from the client
	/// @param response - The node to send the response back to the client
	virtual void handle_get(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response);

	/// Handle a POST request
	/// @param request - The request from the client
	/// @param response - The node to send the response back to the client
	virtual void handle_post(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response);

	/// Handle a HEAD request
	/// @param request - The request from the client
	/// @param response - The node to send the response back to the client
	virtual void handle_head(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response);

	/// Handle a PUT request
	/// @param request - The request from the client
	/// @param response - The node to send the response back to the client
	virtual void handle_put(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response);

	/// Handle a PATCH request
	/// @param request - The request from the client
	/// @param response - The node to send the response back to the client
	virtual void handle_patch(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response);

	/// Handle a DELETE request
	/// @param request - The request from the client
	/// @param response - The node to send the response back to the client
	virtual void handle_delete(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response);

	/// Handle an OPTIONS request
	/// @param request - The request from the client
	/// @param response - The node to send the response back to the client
	virtual void handle_options(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response);
};

#endif // HTTP_ROUTER_H