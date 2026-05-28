/**************************************************************************/
/*  markdown_viewer_image_loader.h                                        */
/**************************************************************************/

#pragma once

#include "core/templates/hash_map.h"
#include "scene/main/http_request.h"
#include "scene/resources/texture.h"

enum class MarkdownViewerImageStatus {
	STATUS_EMPTY,
	STATUS_LOADING,
	STATUS_LOADED,
	STATUS_FAILED,
	STATUS_REMOTE_DISABLED,
};

struct MarkdownViewerImageRequestResult {
	MarkdownViewerImageStatus status = MarkdownViewerImageStatus::STATUS_EMPTY;
	Ref<Texture2D> texture;
	Size2 size;
	String error;
};

class MarkdownViewerImageLoader : public Node {
	GDCLASS(MarkdownViewerImageLoader, Node);

	struct ImageEntry {
		MarkdownViewerImageRequestResult result;
		HTTPRequest *request = nullptr;
	};

	HashMap<String, ImageEntry> entries;
	bool remote_images_enabled = false;
	int max_body_size = 8 * 1024 * 1024;
	double timeout = 10.0;

	static bool _is_remote_source(const String &p_source);
	void _start_request(const String &p_source, ImageEntry &r_entry);
	MarkdownViewerImageRequestResult _decode_buffer(const String &p_source, const Vector<uint8_t> &p_data);
	void _complete_request(const String &p_source, int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body, String p_source);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_remote_images_enabled(bool p_enabled);
	bool is_remote_images_enabled() const;

	void set_max_body_size(int p_bytes);
	int get_max_body_size() const;

	void set_timeout(double p_timeout);
	double get_timeout() const;

	MarkdownViewerImageRequestResult ensure_image(const String &p_source);
	bool has_pending_requests() const;

	MarkdownViewerImageRequestResult decode_buffer_for_test(const String &p_source, const Vector<uint8_t> &p_data);
	void complete_request_for_test(const String &p_source, int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	~MarkdownViewerImageLoader();
};
