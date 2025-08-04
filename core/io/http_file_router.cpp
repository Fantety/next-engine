#include "http_file_router.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#include "core/io/file_access.h"
#include "core/io/http_request.h"
#include "core/io/http_response.h"

void HttpFileRouter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("handle_get", "request", "response"), &HttpFileRouter::handle_get);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path"), "", "path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "index_page"), "", "index_page");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "fallback_page"), "", "fallback_page");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "extensions"), "", "extensions");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "exclude_extensions"), "", "exclude_extensions");
}

HttpFileRouter::HttpFileRouter(const String& p_path, const Dictionary& options) : path(p_path) {
	index_page = options.get("index_page", "");
	fallback_page = options.get("fallback_page", "");
	
	// Handle extensions array
	Variant ext_var = options.get("extensions", Array());
	if (ext_var.get_type() == Variant::PACKED_STRING_ARRAY) {
		extensions = ext_var;
	} else if (ext_var.get_type() == Variant::ARRAY) {
		Array ext_array = ext_var;
		extensions.resize(ext_array.size());
		for (int i = 0; i < ext_array.size(); ++i) {
			extensions[i] = ext_array[i];
		}
	}
	
	// Handle exclude_extensions array
	Variant exclude_ext_var = options.get("exclude_extensions", Array());
	if (exclude_ext_var.get_type() == Variant::PACKED_STRING_ARRAY) {
		exclude_extensions = exclude_ext_var;
	} else if (exclude_ext_var.get_type() == Variant::ARRAY) {
		Array exclude_ext_array = exclude_ext_var;
		exclude_extensions.resize(exclude_ext_array.size());
		for (int i = 0; i < exclude_ext_array.size(); ++i) {
			exclude_extensions[i] = exclude_ext_array[i];
		}
	}
}

HttpFileRouter::~HttpFileRouter() {}

void HttpFileRouter::handle_get(const Ref<HttpRequest>& request, const Ref<HttpResponse>& response) {
	String serving_path = path + request->path;
	bool file_exists = _file_exists(serving_path);

	if (request->path == "/" && !file_exists) {
		if (!index_page.is_empty()) {
			serving_path = path + "/" + index_page;
			file_exists = _file_exists(serving_path);
		}
	}

	String request_extension = request->path.get_extension();
	if (request_extension.is_empty() && !file_exists) {
		for (int i = 0; i < extensions.size(); ++i) {
			String extension = extensions[i];
			serving_path = path + request->path + "." + extension;
			file_exists = _file_exists(serving_path);
			if (file_exists) {
				break;
			}
		}
	}

	// GDScript must be excluded, unless it is used as a preprocessor (php-like)
	String serving_extension = serving_path.get_extension();
	bool is_excluded = serving_extension == "gd";
	for (int i = 0; i < exclude_extensions.size(); ++i) {
		if (serving_extension == exclude_extensions[i]) {
			is_excluded = true;
			break;
		}
	}

	if (file_exists && !is_excluded) {
		response->send_raw(
			200,
			_serve_file(serving_path),
			_get_mime(serving_extension)
		);
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

Vector<uint8_t> HttpFileRouter::_serve_file(const String& file_path) {
	Vector<uint8_t> content;
	Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::READ);
	Error error = file->get_open_error();
	if (error != OK) {
		String error_msg = "Couldn't serve file, ERROR = " + itos(error);
		content = error_msg.to_ascii_buffer();
	} else {
		uint64_t length = file->get_length();
		content.resize(length);
		file->get_buffer(content.ptrw(), length);
	}
	return content;
}

bool HttpFileRouter::_file_exists(const String& file_path) {
	return FileAccess::exists(file_path);
}

String HttpFileRouter::_get_mime(const String& file_extension) {
	String type = "application";
	String subtype = "octet-stream";
	
	// Web files
	if (file_extension == "css" || file_extension == "html" || file_extension == "csv" || 
		file_extension == "js" || file_extension == "mjs") {
		type = "text";
		subtype = (file_extension == "js" || file_extension == "mjs") ? "javascript" : file_extension;
	} else if (file_extension == "php") {
		subtype = "x-httpd-php";
	} else if (file_extension == "ttf" || file_extension == "woff" || file_extension == "woff2") {
		type = "font";
		subtype = file_extension;
	}
	// Image
	else if (file_extension == "png" || file_extension == "bmp" || file_extension == "gif" || 
			file_extension == "webp") {
		type = "image";
		subtype = file_extension;
	} else if (file_extension == "jpeg" || file_extension == "jpg") {
		type = "image";
		subtype = "jpg";
	} else if (file_extension == "tiff" || file_extension == "tif") {
		type = "image";
		subtype = "jpg";
	} else if (file_extension == "svg") {
		type = "image";
		subtype = "svg+xml";
	} else if (file_extension == "ico") {
		type = "image";
		subtype = "vnd.microsoft.icon";
	}
	// Documents
	else if (file_extension == "doc") {
		subtype = "msword";
	} else if (file_extension == "docx") {
		subtype = "vnd.openxmlformats-officedocument.wordprocessingml.document";
	} else if (file_extension == "7z") {
		subtype = "x-7x-compressed";
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
	}
	// Audio
	else if (file_extension == "midi" || file_extension == "mp3" || file_extension == "wav") {
		type = "audio";
		subtype = file_extension;
	} else if (file_extension == "mp4" || file_extension == "mpeg" || file_extension == "webm") {
		type = "audio";
		subtype = file_extension;
	} else if (file_extension == "oga" || file_extension == "ogg") {
		type = "audio";
		subtype = "ogg";
	} else if (file_extension == "mpkg") {
		subtype = "vnd.apple.installer+xml";
	}
	// Video
	else if (file_extension == "ogv") {
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