/**************************************************************************/
/*  markdown_viewer_image_loader.cpp                                      */
/**************************************************************************/

#include "markdown_viewer_image_loader.h"

#include "core/io/image.h"
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

void MarkdownViewerImageLoader::set_max_body_size(int p_bytes) {
	max_body_size = MAX(0, p_bytes);
}

int MarkdownViewerImageLoader::get_max_body_size() const {
	return max_body_size;
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
		if (_is_remote_source(p_source)) {
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
	if (_is_remote_source(p_source)) {
		if (!remote_images_enabled) {
			entry.result.status = MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED;
			entries.insert(p_source, entry);
			return entry.result;
		}

		entry.result.status = MarkdownViewerImageStatus::STATUS_LOADING;
		entries.insert(p_source, entry);

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

MarkdownViewerImageLoader::~MarkdownViewerImageLoader() {
	for (KeyValue<String, ImageEntry> &E : entries) {
		if (E.value.request) {
			E.value.request->cancel_request();
		}
	}
}
