/**************************************************************************/
/*  markdown_viewer_image_loader.cpp                                      */
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

#include "markdown_viewer_image_loader.h"

#include "core/io/image.h"
#include "core/io/ip_address.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "scene/resources/image_texture.h"

enum class MarkdownViewerImageFormat {
	UNKNOWN,
	PNG,
	JPG,
	WEBP,
};

static bool _buffer_has_prefix(const Vector<uint8_t> &p_data, const uint8_t *p_prefix, int p_prefix_size) {
	if (p_data.size() < p_prefix_size) {
		return false;
	}

	const uint8_t *data = p_data.ptr();
	for (int i = 0; i < p_prefix_size; i++) {
		if (data[i] != p_prefix[i]) {
			return false;
		}
	}
	return true;
}

static MarkdownViewerImageFormat _detect_image_format(const Vector<uint8_t> &p_data) {
	static constexpr uint8_t png_signature[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	static constexpr uint8_t jpg_signature[] = { 0xFF, 0xD8, 0xFF };
	static constexpr uint8_t riff_signature[] = { 'R', 'I', 'F', 'F' };
	static constexpr uint8_t webp_signature[] = { 'W', 'E', 'B', 'P' };

	if (_buffer_has_prefix(p_data, png_signature, sizeof(png_signature))) {
		return MarkdownViewerImageFormat::PNG;
	}
	if (_buffer_has_prefix(p_data, jpg_signature, sizeof(jpg_signature))) {
		return MarkdownViewerImageFormat::JPG;
	}
	if (p_data.size() >= 12 && _buffer_has_prefix(p_data, riff_signature, sizeof(riff_signature))) {
		const uint8_t *data = p_data.ptr();
		bool is_webp = true;
		for (int i = 0; i < (int)sizeof(webp_signature); i++) {
			if (data[8 + i] != webp_signature[i]) {
				is_webp = false;
				break;
			}
		}
		if (is_webp) {
			return MarkdownViewerImageFormat::WEBP;
		}
	}

	return MarkdownViewerImageFormat::UNKNOWN;
}

void MarkdownViewerImageLoader::_bind_methods() {
	ADD_SIGNAL(MethodInfo("image_state_changed", PropertyInfo(Variant::STRING, "source")));
}

void MarkdownViewerImageLoader::_notification(int p_what) {
	if (p_what != NOTIFICATION_ENTER_TREE || !remote_images_enabled) {
		return;
	}

	for (KeyValue<String, ImageEntry> &E : entries) {
		if (_is_remote_source(E.key) && E.value.result.status == MarkdownViewerImageStatus::STATUS_LOADING && E.value.request == nullptr) {
			_start_request(E.key, E.value);
		}
	}
}

bool MarkdownViewerImageLoader::_is_remote_source(const String &p_source) {
	const String lower = p_source.to_lower();
	return lower.begins_with("http://") || lower.begins_with("https://");
}

bool MarkdownViewerImageLoader::_is_private_remote_host(const String &p_host) {
	String host = p_host.strip_edges().to_lower();
	if (host.begins_with("[") && host.ends_with("]")) {
		host = host.substr(1, host.length() - 2);
	}
	if (host == "localhost" || host.ends_with(".localhost")) {
		return true;
	}
	if (!host.is_valid_ip_address()) {
		return false;
	}

	const IPAddress address(host);
	if (address.is_ipv4()) {
		const uint8_t *octets = address.get_ipv4();
		return octets[0] == 0 || octets[0] == 10 || octets[0] == 127 || (octets[0] == 169 && octets[1] == 254) || (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) || (octets[0] == 192 && octets[1] == 168);
	}

	const uint8_t *octets = address.get_ipv6();
	bool all_zero = true;
	for (int i = 0; i < 15; i++) {
		all_zero = all_zero && octets[i] == 0;
	}
	return (all_zero && octets[15] == 1) || (octets[0] & 0xfe) == 0xfc || (octets[0] == 0xfe && (octets[1] & 0xc0) == 0x80);
}

bool MarkdownViewerImageLoader::_is_remote_source_allowed(const String &p_source, String &r_error) const {
	String scheme;
	String host;
	String base_path;
	String fragment;
	int port = 0;
	if (p_source.parse_url(scheme, host, port, base_path, fragment) != OK || scheme.to_lower() != "https://" || host.is_empty()) {
		r_error = "Remote images must use a valid HTTPS URL.";
		return false;
	}

	const String normalized_host = host.strip_edges().to_lower();
	if (_is_private_remote_host(normalized_host)) {
		r_error = "Remote image host is not publicly routable.";
		return false;
	}

	bool allowed = false;
	for (int i = 0; i < remote_image_allowed_hosts.size(); i++) {
		const String allowed_host = remote_image_allowed_hosts[i].strip_edges().to_lower();
		if (allowed_host == normalized_host || (allowed_host.begins_with("*.") && normalized_host.ends_with(allowed_host.substr(1)))) {
			allowed = true;
			break;
		}
	}
	if (!allowed) {
		r_error = "Remote image host is not in the allowed host list.";
		return false;
	}

	return true;
}

void MarkdownViewerImageLoader::_touch_entry(ImageEntry &r_entry) {
	r_entry.last_access = ++access_counter;
}

void MarkdownViewerImageLoader::_prune_entries() {
	while (entries.size() > (uint32_t)max_cache_entries) {
		String oldest_source;
		uint64_t oldest_access = UINT64_MAX;
		for (const KeyValue<String, ImageEntry> &E : entries) {
			if (E.value.request || E.value.last_access >= oldest_access) {
				continue;
			}
			oldest_source = E.key;
			oldest_access = E.value.last_access;
		}
		if (oldest_source.is_empty()) {
			return;
		}
		entries.erase(oldest_source);
	}
}

MarkdownViewerImageRequestResult MarkdownViewerImageLoader::_decode_buffer(const String &p_source, const Vector<uint8_t> &p_data) {
	MarkdownViewerImageRequestResult result;
	Ref<Image> image = memnew(Image);

	Error err = ERR_FILE_UNRECOGNIZED;
	switch (_detect_image_format(p_data)) {
		case MarkdownViewerImageFormat::PNG:
			err = image->load_png_from_buffer(p_data);
			break;
		case MarkdownViewerImageFormat::JPG:
			err = image->load_jpg_from_buffer(p_data);
			break;
		case MarkdownViewerImageFormat::WEBP:
			err = image->load_webp_from_buffer(p_data);
			break;
		case MarkdownViewerImageFormat::UNKNOWN:
			break;
	}

	if (err != OK || image->is_empty()) {
		result.status = MarkdownViewerImageStatus::STATUS_FAILED;
		result.error = "Unsupported or invalid image data: " + p_source;
		return result;
	}
	if ((int64_t)image->get_width() * image->get_height() > max_pixel_count) {
		result.status = MarkdownViewerImageStatus::STATUS_FAILED;
		result.error = "Image exceeds the configured pixel limit: " + p_source;
		return result;
	}

	result.texture = ImageTexture::create_from_image(image);
	result.size = Size2(image->get_width(), image->get_height());
	result.status = result.texture.is_valid() ? MarkdownViewerImageStatus::STATUS_LOADED : MarkdownViewerImageStatus::STATUS_FAILED;
	if (result.status == MarkdownViewerImageStatus::STATUS_FAILED) {
		result.error = "Failed to create texture: " + p_source;
	}
	return result;
}

void MarkdownViewerImageLoader::_start_request(const String &p_source, ImageEntry &r_entry) {
	ERR_FAIL_COND(r_entry.request != nullptr);
	String validation_error;
	if (!_is_remote_source_allowed(p_source, validation_error)) {
		r_entry.result.status = MarkdownViewerImageStatus::STATUS_FAILED;
		r_entry.result.error = validation_error;
		emit_signal(SNAME("image_state_changed"), p_source);
		return;
	}

	HTTPRequest *request = memnew(HTTPRequest);
	request->set_body_size_limit(max_body_size);
	request->set_timeout(timeout);
	request->connect("request_completed", callable_mp(this, &MarkdownViewerImageLoader::_request_completed).bind(p_source));
	add_child(request, false, INTERNAL_MODE_BACK);
	r_entry.request = request;

	Error err = request->request(p_source);
	if (err != OK) {
		r_entry.result.status = MarkdownViewerImageStatus::STATUS_FAILED;
		r_entry.result.error = "Failed to start image request: " + p_source;
		request->queue_free();
		r_entry.request = nullptr;
		emit_signal(SNAME("image_state_changed"), p_source);
	}
}

void MarkdownViewerImageLoader::_complete_request(const String &p_source, int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	ImageEntry *entry = entries.getptr(p_source);
	if (!entry) {
		return;
	}

	if (entry->request) {
		entry->request->queue_free();
		entry->request = nullptr;
	}

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_response_code < 200 || p_response_code >= 300) {
		entry->result.status = MarkdownViewerImageStatus::STATUS_FAILED;
		entry->result.error = vformat("HTTP image request failed (%d/%d): %s", p_result, p_response_code, p_source);
		emit_signal(SNAME("image_state_changed"), p_source);
		return;
	}

	Vector<uint8_t> body;
	body.resize(p_body.size());
	for (int i = 0; i < p_body.size(); i++) {
		body.write[i] = p_body[i];
	}
	entry->result = _decode_buffer(p_source, body);
	_touch_entry(*entry);
	emit_signal(SNAME("image_state_changed"), p_source);
}

void MarkdownViewerImageLoader::_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body, String p_source) {
	_complete_request(p_source, p_result, p_response_code, p_headers, p_body);
}

void MarkdownViewerImageLoader::set_remote_images_enabled(bool p_enabled) {
	if (remote_images_enabled == p_enabled) {
		return;
	}
	remote_images_enabled = p_enabled;

	for (KeyValue<String, ImageEntry> &E : entries) {
		if (!_is_remote_source(E.key)) {
			continue;
		}
		String validation_error;
		if (!_is_remote_source_allowed(E.key, validation_error)) {
			if (E.value.request) {
				E.value.request->cancel_request();
				E.value.request->queue_free();
				E.value.request = nullptr;
			}
			E.value.result.status = MarkdownViewerImageStatus::STATUS_FAILED;
			E.value.result.error = validation_error;
			continue;
		}

		if (remote_images_enabled && E.value.result.status == MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED) {
			E.value.result.status = MarkdownViewerImageStatus::STATUS_LOADING;
			E.value.result.error.clear();
			if (is_inside_tree()) {
				_start_request(E.key, E.value);
			}
		} else if (!remote_images_enabled && E.value.result.status == MarkdownViewerImageStatus::STATUS_LOADING) {
			if (E.value.request) {
				E.value.request->cancel_request();
				E.value.request->queue_free();
				E.value.request = nullptr;
			}
			E.value.result.status = MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED;
			E.value.result.error.clear();
		}
	}
}

bool MarkdownViewerImageLoader::is_remote_images_enabled() const {
	return remote_images_enabled;
}

void MarkdownViewerImageLoader::set_remote_image_allowed_hosts(const PackedStringArray &p_hosts) {
	remote_image_allowed_hosts = p_hosts;
}

PackedStringArray MarkdownViewerImageLoader::get_remote_image_allowed_hosts() const {
	return remote_image_allowed_hosts;
}

void MarkdownViewerImageLoader::set_max_body_size(int p_bytes) {
	max_body_size = MAX(0, p_bytes);
}

int MarkdownViewerImageLoader::get_max_body_size() const {
	return max_body_size;
}

void MarkdownViewerImageLoader::set_max_pixel_count(int p_pixels) {
	max_pixel_count = MAX(1, p_pixels);
}

int MarkdownViewerImageLoader::get_max_pixel_count() const {
	return max_pixel_count;
}

void MarkdownViewerImageLoader::set_max_cache_entries(int p_entries) {
	max_cache_entries = MAX(1, p_entries);
	_prune_entries();
}

int MarkdownViewerImageLoader::get_max_cache_entries() const {
	return max_cache_entries;
}

void MarkdownViewerImageLoader::set_timeout(double p_timeout) {
	timeout = MAX(0.0, p_timeout);
}

double MarkdownViewerImageLoader::get_timeout() const {
	return timeout;
}

MarkdownViewerImageRequestResult MarkdownViewerImageLoader::ensure_image(const String &p_source) {
	if (p_source.is_empty()) {
		MarkdownViewerImageRequestResult result;
		result.status = MarkdownViewerImageStatus::STATUS_FAILED;
		result.error = "Empty image source";
		return result;
	}

	if (ImageEntry *entry = entries.getptr(p_source)) {
		_touch_entry(*entry);
		if (_is_remote_source(p_source)) {
			String validation_error;
			if (!_is_remote_source_allowed(p_source, validation_error)) {
				if (entry->request) {
					entry->request->cancel_request();
					entry->request->queue_free();
					entry->request = nullptr;
				}
				entry->result.status = MarkdownViewerImageStatus::STATUS_FAILED;
				entry->result.error = validation_error;
				return entry->result;
			}
			if (!remote_images_enabled && entry->result.status == MarkdownViewerImageStatus::STATUS_LOADING) {
				if (entry->request) {
					entry->request->cancel_request();
					entry->request->queue_free();
					entry->request = nullptr;
				}
				entry->result.status = MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED;
			} else if (remote_images_enabled && entry->result.status == MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED) {
				entry->result.status = MarkdownViewerImageStatus::STATUS_LOADING;
				entry->result.error.clear();
			}
			if (remote_images_enabled && entry->result.status == MarkdownViewerImageStatus::STATUS_LOADING && entry->request == nullptr && is_inside_tree()) {
				_start_request(p_source, *entry);
			}
		}
		return entry->result;
	}

	ImageEntry entry;
	_touch_entry(entry);
	if (_is_remote_source(p_source)) {
		if (!remote_images_enabled) {
			entry.result.status = MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED;
			entries.insert(p_source, entry);
			_prune_entries();
			return entry.result;
		}
		String validation_error;
		if (!_is_remote_source_allowed(p_source, validation_error)) {
			entry.result.status = MarkdownViewerImageStatus::STATUS_FAILED;
			entry.result.error = validation_error;
			entries.insert(p_source, entry);
			_prune_entries();
			return entry.result;
		}

		entry.result.status = MarkdownViewerImageStatus::STATUS_LOADING;
		entries.insert(p_source, entry);
		_prune_entries();

		ImageEntry *stored = entries.getptr(p_source);
		if (is_inside_tree()) {
			_start_request(p_source, *stored);
		}
		return stored->result;
	}

	Ref<Resource> resource = ResourceLoader::load(p_source);
	Ref<Texture2D> texture = resource;
	if (texture.is_valid()) {
		entry.result.status = MarkdownViewerImageStatus::STATUS_LOADED;
		entry.result.texture = texture;
		entry.result.size = texture->get_size();
	} else {
		entry.result.status = MarkdownViewerImageStatus::STATUS_FAILED;
		entry.result.error = "Failed to load image: " + p_source;
	}
	entries.insert(p_source, entry);
	_prune_entries();
	return entry.result;
}

bool MarkdownViewerImageLoader::has_pending_requests() const {
	for (const KeyValue<String, ImageEntry> &E : entries) {
		if (E.value.result.status == MarkdownViewerImageStatus::STATUS_LOADING) {
			return true;
		}
	}
	return false;
}

MarkdownViewerImageRequestResult MarkdownViewerImageLoader::decode_buffer_for_test(const String &p_source, const Vector<uint8_t> &p_data) {
	return _decode_buffer(p_source, p_data);
}

void MarkdownViewerImageLoader::complete_request_for_test(const String &p_source, int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	_complete_request(p_source, p_result, p_response_code, p_headers, p_body);
}

bool MarkdownViewerImageLoader::has_cached_image_for_test(const String &p_source) const {
	return entries.has(p_source);
}

MarkdownViewerImageLoader::~MarkdownViewerImageLoader() {
	for (KeyValue<String, ImageEntry> &E : entries) {
		if (E.value.request) {
			E.value.request->cancel_request();
		}
	}
}
