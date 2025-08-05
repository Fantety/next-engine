/*
 * @FilePath: \core\io\http_file_router.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-08-05 09:46:54
 * @LastEditors: Fantety
 * @LastEditTime: 2025-08-05 11:30:58
 */
#pragma once

#include "http_router.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

class HttpFileRouter : public HttpRouter {
	GDCLASS(HttpFileRouter, HttpRouter);

protected:
	static void _bind_methods();

public:
	// Full path to the folder which will be exposed to web
	String path;

	// Relative path to the index page, which will be served when a request is made to "/" (server root)
	String index_page;

	// Relative path to the fallback page which will be served if the requested file was not found
	String fallback_page;

	// An ordered list of extensions that will be checked
	// if no file extension is provided by the request
	Vector<String> extensions;

	// A list of extensions that will be excluded if requested
	Vector<String> exclude_extensions;
public:
		HttpFileRouter() {}
		HttpFileRouter(const String &path, const Dictionary &options = Dictionary());
	~HttpFileRouter();

	// Handle a GET request
	void handle_get(Ref<HttpRequest> request, Ref<HttpResponse> response) override;

private:
	// Reads a file as text
	Vector<uint8_t> _serve_file(const String &file_path);

	// Check if a file exists
	bool _file_exists(const String &file_path);

	// Get the full MIME type of a file from its extension
	String _get_mime(const String &file_extension);
};