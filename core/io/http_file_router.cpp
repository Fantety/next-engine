#include "http_file_router.h"
#include "core/io/file_access.h"

void HttpFileRouter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("handle_get", "request", "response"), &HttpFileRouter::handle_get);
}

HttpFileRouter::HttpFileRouter(const String &p_path, const Dictionary &options) {
	path = p_path;
	index_page = options.get("index_page", "index.html");
	fallback_page = options.get("fallback_page", "");
	
	// Handle extensions array
	if (options.has("extensions")) {
		Array ext_array = options["extensions"];
		extensions.resize(ext_array.size());
		for (int i = 0; i < ext_array.size(); i++) {
			extensions.set(i, ext_array[i]);
		}
	} else {
		extensions.push_back("html");
	}
	
	// Handle exclude_extensions array
	if (options.has("exclude_extensions")) {
		Array exclude_array = options["exclude_extensions"];
		exclude_extensions.resize(exclude_array.size());
		for (int i = 0; i < exclude_array.size(); i++) {
			exclude_extensions.set(i, exclude_array[i]);
		}
	}
}

HttpFileRouter::~HttpFileRouter() {}

void HttpFileRouter::handle_get(Ref<HttpRequest> request, Ref<HttpResponse> response) {
	String serving_path = path + request->path;
	bool file_exists = _file_exists(serving_path);

	if (request->path == "/" && !file_exists) {
		if (!index_page.is_empty()) {
			serving_path = path + "/" + index_page;
			file_exists = _file_exists(serving_path);
		}
	}

	if (request->path.get_extension().is_empty() && !file_exists) {
		for (int i = 0; i < extensions.size(); i++) {
			serving_path = path + request->path + "." + extensions[i];
			file_exists = _file_exists(serving_path);
			if (file_exists) {
				break;
			}
		}
	}

	// GDScript must be excluded, unless it is used as a preprocessor (php-like)
	Vector<String> excluded;
	excluded.push_back("gd");
	for (int i = 0; i < exclude_extensions.size(); i++) {
		excluded.push_back(exclude_extensions[i]);
	}

	bool is_excluded = false;
	String extension = serving_path.get_extension();
	for (int i = 0; i < excluded.size(); i++) {
		if (extension == excluded[i]) {
			is_excluded = true;
			break;
		}
	}

	if (file_exists && !is_excluded) {
		response->send_raw(200, _serve_file(serving_path), _get_mime(serving_path.get_extension()));
	} else {
		if (!fallback_page.is_empty()) {
			serving_path = path + "/" + fallback_page;
			int status_code = (index_page == fallback_page) ? 200 : 404;
			response->send_raw(status_code, _serve_file(serving_path), _get_mime(fallback_page.get_extension()));
		} else {
			response->send_raw(404);
		}
	}
}

Vector<uint8_t> HttpFileRouter::_serve_file(const String &file_path) {
	Vector<uint8_t> content;
	Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ);
	
	if (file.is_valid()) {
		content.resize(file->get_length());
		file->get_buffer(content.ptrw(), content.size());
		file->close();
	} else {
		String error_msg = "Couldn't serve file, ERROR = " + itos(FileAccess::get_open_error());
		content = error_msg.to_ascii_buffer();
	}
	
	return content;
}

bool HttpFileRouter::_file_exists(const String &file_path) {
	return FileAccess::exists(file_path);
}

String HttpFileRouter::_get_mime(const String &file_extension) {
	String type = "application";
	String subtype = "octet-stream";
	
	if (file_extension == "css" || file_extension == "html" || file_extension == "csv" || 
	    file_extension == "js" || file_extension == "mjs") {
		type = "text";
		subtype = (file_extension == "js" || file_extension == "mjs") ? "javascript" : file_extension;
	} else if (file_extension == "php") {
		subtype = "x-httpd-php";
	} else if (file_extension == "ttf" || file_extension == "woff" || file_extension == "woff2") {
		type = "font";
		subtype = file_extension;
	} else if (file_extension == "png" || file_extension == "bmp" || file_extension == "gif" ||
	           file_extension == "webp" || file_extension == "jpeg" || file_extension == "jpg") {
		type = "image";
		subtype = (file_extension == "jpeg" || file_extension == "jpg") ? "jpeg" : file_extension;
	} else if (file_extension == "tiff" || file_extension == "tif") {
		type = "image";
		subtype = "tiff";
	} else if (file_extension == "svg") {
		type = "image";
		subtype = "svg+xml";
	} else if (file_extension == "ico") {
		type = "image";
		subtype = "vnd.microsoft.icon";
	} else if (file_extension == "doc") {
		subtype = "msword";
	} else if (file_extension == "docx") {
		subtype = "vnd.openxmlformats-officedocument.wordprocessingml.document";
	} else if (file_extension == "7z") {
		subtype = "x-7z-compressed";
	} else if (file_extension == "gz") {
		subtype = "gzip";
	} else if (file_extension == "tar") {
		subtype = "application/x-tar";
	} else if (file_extension == "json" || file_extension == "pdf" || file_extension == "zip") {
		subtype = file_extension;
	} else if (file_extension == "txt") {
		type = "text";
		subtype = "plain";
	} else if (file_extension == "ppt") {
		subtype = "vnd.ms-powerpoint";
	} else if (file_extension == "midi" || file_extension == "mp3" || file_extension == "wav") {
		type = "audio";
		subtype = file_extension;
	} else if (file_extension == "mp4" || file_extension == "mpeg" || file_extension == "webm") {
		type = "video";
		subtype = file_extension;
	} else if (file_extension == "oga" || file_extension == "ogg") {
		type = "audio";
		subtype = "ogg";
	} else if (file_extension == "mpkg") {
		subtype = "vnd.apple.installer+xml";
	} else if (file_extension == "ogv") {
		type = "video";
		subtype = "ogg";
	} else if (file_extension == "avi") {
		type = "video";
		subtype = "x-msvideo";
	} else if (file_extension == "ogx") {
		subtype = "ogg";
	}
	
	return type + "/" + subtype;
}