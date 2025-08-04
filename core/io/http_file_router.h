#ifndef HTTP_FILE_ROUTER_H
#define HTTP_FILE_ROUTER_H

#include "core/io/http_router.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/object/class_db.h"

/**
 * @brief Class inheriting HttpRouter for handling file serving requests
 *
 * NOTE: This class mainly handles behind the scenes stuff.
 */
class HttpFileRouter : public HttpRouter {
	GDCLASS(HttpFileRouter, HttpRouter);

public:
	/// Full path to the folder which will be exposed to web
	String path = "";

	/// Relative path to the index page, which will be served when a request is made to "/" (server root)
	String index_page = "index.html";

	/// Relative path to the fallback page which will be served if the requested file was not found
	String fallback_page = "";

	/// An ordered list of extensions that will be checked
	/// if no file extension is provided by the request
	PackedStringArray extensions;

	/// A list of extensions that will be excluded if requested
	PackedStringArray exclude_extensions;

protected:
	static void _bind_methods();

public:
	/// Creates an HttpFileRouter instance
	/// @param path - Full path to the folder which will be exposed to web.
	/// @param options - Optional Dictionary of options which can be configured:
	///  - fallback_page: Full path to the fallback page which will be served if the requested file was not found
	///  - extensions: A list of extensions that will be checked if no file extension is provided by the request
	///  - exclude_extensions: A list of extensions that will be excluded if requested
	HttpFileRouter(const String& path, const Dictionary& options = Dictionary());
	~HttpFileRouter();

	/// Handle a GET request
	/// @param request - The request from the client
	/// @param response - The response to send to the client
	void handle_get(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) override;

private:
	/// Reads a file as text
	/// @param file_path: Full path to the file
	Vector<uint8_t> _serve_file(const String& file_path);

	/// Check if a file exists
	/// @param file_path: Full path to the file
	bool _file_exists(const String& file_path);

	/// Get the full MIME type of a file from its extension
	/// @param file_extension: Extension of the file to be served
	String _get_mime(const String& file_extension);
};

#endif // HTTP_FILE_ROUTER_H