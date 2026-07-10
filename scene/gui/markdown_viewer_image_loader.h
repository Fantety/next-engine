/**************************************************************************/
/*  markdown_viewer_image_loader.h                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
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
		uint64_t last_access = 0;
	};

	HashMap<String, ImageEntry> entries;
	bool remote_images_enabled = false;
	PackedStringArray remote_image_allowed_hosts;
	int max_body_size = 8 * 1024 * 1024;
	int max_pixel_count = 16 * 1024 * 1024;
	int max_cache_entries = 64;
	double timeout = 10.0;
	uint64_t access_counter = 0;

	static bool _is_remote_source(const String &p_source);
	static bool _is_private_remote_host(const String &p_host);
	bool _is_remote_source_allowed(const String &p_source, String &r_error) const;
	void _touch_entry(ImageEntry &r_entry);
	void _prune_entries();
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
	void set_remote_image_allowed_hosts(const PackedStringArray &p_hosts);
	PackedStringArray get_remote_image_allowed_hosts() const;

	void set_max_body_size(int p_bytes);
	int get_max_body_size() const;
	void set_max_pixel_count(int p_pixels);
	int get_max_pixel_count() const;
	void set_max_cache_entries(int p_entries);
	int get_max_cache_entries() const;

	void set_timeout(double p_timeout);
	double get_timeout() const;

	MarkdownViewerImageRequestResult ensure_image(const String &p_source);
	bool has_pending_requests() const;

	MarkdownViewerImageRequestResult decode_buffer_for_test(const String &p_source, const Vector<uint8_t> &p_data);
	void complete_request_for_test(const String &p_source, int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	bool has_cached_image_for_test(const String &p_source) const;

	~MarkdownViewerImageLoader();
};
