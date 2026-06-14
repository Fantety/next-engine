/**************************************************************************/
/*  ai_attachment_resolver.cpp                                            */
/**************************************************************************/

#include "ai_attachment_resolver.h"

#include "core/config/project_settings.h"
#include "core/core_bind.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"

#include <cstdint>

static bool _ai_attachment_buffer_has_prefix(const PackedByteArray &p_bytes, const uint8_t *p_prefix, int p_prefix_size) {
	if (p_bytes.size() < p_prefix_size) {
		return false;
	}
	const uint8_t *bytes = p_bytes.ptr();
	for (int i = 0; i < p_prefix_size; i++) {
		if (bytes[i] != p_prefix[i]) {
			return false;
		}
	}
	return true;
}

Dictionary AIAttachmentResolver::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

PackedByteArray AIAttachmentResolver::_bytes_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::PACKED_BYTE_ARRAY) {
		return p_value;
	}
	if (p_value.get_type() == Variant::ARRAY) {
		const Array array = p_value;
		PackedByteArray bytes;
		bytes.resize(array.size());
		for (int i = 0; i < array.size(); i++) {
			bytes.write[i] = uint8_t(int(array[i]) & 0xff);
		}
		return bytes;
	}
	return PackedByteArray();
}

String AIAttachmentResolver::_attachment_type(const Dictionary &p_attachment) {
	String type = String(p_attachment.get("type", String())).strip_edges().to_lower();
	if (type == "attachment" || type == "file") {
		const Dictionary nested = _dictionary_from_variant(p_attachment.get("attachment", Dictionary()));
		if (!nested.is_empty()) {
			type = String(nested.get("type", type)).strip_edges().to_lower();
		}
	}
	if (p_attachment.has("data_url") || p_attachment.has("dataURL") || String(p_attachment.get("data", String())).begins_with("data:")) {
		return "data";
	}
	if (p_attachment.has("blob_id") || p_attachment.has("blobID")) {
		return "blob";
	}
	const String path = String(p_attachment.get("path", String())).strip_edges();
	if (path.begins_with("blob_")) {
		return "blob";
	}
	if (!path.is_empty()) {
		return "path";
	}
	if (!type.is_empty() && type != "attachment" && type != "file") {
		return type;
	}
	return type.is_empty() ? String("path") : type;
}

String AIAttachmentResolver::_name_from_attachment(const Dictionary &p_attachment, const String &p_fallback) {
	String name = String(p_attachment.get("name", p_attachment.get("filename", String()))).strip_edges();
	if (name.is_empty()) {
		name = p_fallback.strip_edges();
	}
	if (name.is_empty()) {
		name = "attachment";
	}
	return name.get_file().is_empty() ? name : name.get_file();
}

String AIAttachmentResolver::_declared_mime_from_attachment(const Dictionary &p_attachment) {
	return String(p_attachment.get("mime", p_attachment.get("mime_type", p_attachment.get("mimeType", String())))).strip_edges().to_lower();
}

String AIAttachmentResolver::_mime_from_extension(const String &p_name) {
	const String ext = p_name.get_extension().to_lower();
	if (ext == "png") {
		return "image/png";
	}
	if (ext == "jpg" || ext == "jpeg") {
		return "image/jpeg";
	}
	if (ext == "webp") {
		return "image/webp";
	}
	if (ext == "gif") {
		return "image/gif";
	}
	if (ext == "bmp") {
		return "image/bmp";
	}
	if (ext == "svg") {
		return "image/svg+xml";
	}
	if (ext == "pdf") {
		return "application/pdf";
	}
	if (ext == "json") {
		return "application/json";
	}
	if (ext == "md" || ext == "markdown") {
		return "text/markdown";
	}
	if (ext == "txt" || ext == "log" || ext == "gd" || ext == "cs" || ext == "tscn" || ext == "tres" || ext == "cfg" || ext == "shader" || ext == "gdshader" || ext == "xml" || ext == "yaml" || ext == "yml" || ext == "csv" || ext == "ini" || ext == "toml" || ext == "h" || ext == "hpp" || ext == "hh" || ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "py" || ext == "js" || ext == "ts" || ext == "tsx" || ext == "jsx" || ext == "html" || ext == "css" || ext == "scss" || ext == "rs" || ext == "go") {
		return "text/plain";
	}
	if (ext == "mp3") {
		return "audio/mpeg";
	}
	if (ext == "wav") {
		return "audio/wav";
	}
	if (ext == "ogg" || ext == "oga") {
		return "audio/ogg";
	}
	if (ext == "flac") {
		return "audio/flac";
	}
	if (ext == "m4a") {
		return "audio/mp4";
	}
	return String();
}

String AIAttachmentResolver::_mime_from_bytes(const PackedByteArray &p_bytes) {
	static constexpr uint8_t png_signature[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	static constexpr uint8_t jpg_signature[] = { 0xFF, 0xD8, 0xFF };
	static constexpr uint8_t gif87_signature[] = { 'G', 'I', 'F', '8', '7', 'a' };
	static constexpr uint8_t gif89_signature[] = { 'G', 'I', 'F', '8', '9', 'a' };
	static constexpr uint8_t riff_signature[] = { 'R', 'I', 'F', 'F' };
	static constexpr uint8_t webp_signature[] = { 'W', 'E', 'B', 'P' };
	static constexpr uint8_t wave_signature[] = { 'W', 'A', 'V', 'E' };
	static constexpr uint8_t pdf_signature[] = { '%', 'P', 'D', 'F', '-' };
	static constexpr uint8_t ogg_signature[] = { 'O', 'g', 'g', 'S' };
	static constexpr uint8_t id3_signature[] = { 'I', 'D', '3' };

	if (_ai_attachment_buffer_has_prefix(p_bytes, png_signature, sizeof(png_signature))) {
		return "image/png";
	}
	if (_ai_attachment_buffer_has_prefix(p_bytes, jpg_signature, sizeof(jpg_signature))) {
		return "image/jpeg";
	}
	if (_ai_attachment_buffer_has_prefix(p_bytes, gif87_signature, sizeof(gif87_signature)) || _ai_attachment_buffer_has_prefix(p_bytes, gif89_signature, sizeof(gif89_signature))) {
		return "image/gif";
	}
	if (p_bytes.size() >= 12 && _ai_attachment_buffer_has_prefix(p_bytes, riff_signature, sizeof(riff_signature))) {
		const uint8_t *bytes = p_bytes.ptr();
		bool is_webp = true;
		bool is_wave = true;
		for (int i = 0; i < 4; i++) {
			if (bytes[8 + i] != webp_signature[i]) {
				is_webp = false;
			}
			if (bytes[8 + i] != wave_signature[i]) {
				is_wave = false;
			}
		}
		if (is_webp) {
			return "image/webp";
		}
		if (is_wave) {
			return "audio/wav";
		}
	}
	if (_ai_attachment_buffer_has_prefix(p_bytes, pdf_signature, sizeof(pdf_signature))) {
		return "application/pdf";
	}
	if (_ai_attachment_buffer_has_prefix(p_bytes, ogg_signature, sizeof(ogg_signature))) {
		return "audio/ogg";
	}
	if (_ai_attachment_buffer_has_prefix(p_bytes, id3_signature, sizeof(id3_signature))) {
		return "audio/mpeg";
	}
	if (p_bytes.size() >= 4) {
		const int read_size = MIN(p_bytes.size(), 512);
		const String text_prefix = String::utf8(reinterpret_cast<const char *>(p_bytes.ptr()), read_size).strip_edges().to_lower();
		if (text_prefix.begins_with("<svg") || (text_prefix.begins_with("<?xml") && text_prefix.find("<svg") >= 0)) {
			return "image/svg+xml";
		}
	}
	if (p_bytes.size() >= 2) {
		const uint8_t *bytes = p_bytes.ptr();
		if (bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0) {
			return "audio/mpeg";
		}
	}
	return String();
}

String AIAttachmentResolver::_canonical_mime(const String &p_mime) {
	const String mime = p_mime.strip_edges().to_lower();
	if (mime == "image/jpg") {
		return "image/jpeg";
	}
	if (mime == "audio/mp3") {
		return "audio/mpeg";
	}
	if (mime == "audio/x-wav") {
		return "audio/wav";
	}
	return mime;
}

bool AIAttachmentResolver::_mime_requires_sniff_match(const String &p_mime) {
	const String mime = _canonical_mime(p_mime);
	return mime.begins_with("image/") || mime.begins_with("audio/") || mime == "application/pdf";
}

bool AIAttachmentResolver::_validate_payload_mime(const String &p_mime, const PackedByteArray &p_bytes, AIError &r_error) {
	if (p_bytes.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment payload cannot be empty.");
		return false;
	}

	const String declared = _canonical_mime(p_mime);
	const String sniffed = _canonical_mime(_mime_from_bytes(p_bytes));
	if (!sniffed.is_empty() && sniffed != declared) {
		Dictionary details;
		details["declared_mime"] = declared;
		details["sniffed_mime"] = sniffed;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment MIME does not match its payload.", details);
		return false;
	}
	if (sniffed.is_empty() && _mime_requires_sniff_match(declared)) {
		Dictionary details;
		details["declared_mime"] = declared;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment payload does not match a supported binary MIME signature.", details);
		return false;
	}

	r_error = AIError::none();
	return true;
}

bool AIAttachmentResolver::_parse_data_url(const String &p_data_url, String &r_mime, PackedByteArray &r_bytes, AIError &r_error) {
	const String data_url = p_data_url.strip_edges();
	if (!data_url.begins_with("data:")) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment data URL must start with data:.");
		return false;
	}

	const int comma = data_url.find(",");
	if (comma < 0) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment data URL is missing a payload separator.");
		return false;
	}

	const String metadata = data_url.substr(5, comma - 5);
	const PackedStringArray segments = metadata.split(";");
	if (segments.is_empty() || String(segments[0]).strip_edges().is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment data URL is missing a MIME type.");
		return false;
	}

	bool is_base64 = false;
	for (int i = 1; i < segments.size(); i++) {
		if (String(segments[i]).strip_edges().to_lower() == "base64") {
			is_base64 = true;
			break;
		}
	}
	if (!is_base64) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment data URL must be base64 encoded.");
		return false;
	}

	CoreBind::Marshalls *marshalls = CoreBind::Marshalls::get_singleton();
	if (!marshalls) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Marshalls singleton is not available for base64 decoding.");
		return false;
	}

	r_mime = String(segments[0]).strip_edges().to_lower();
	r_bytes = marshalls->base64_to_raw(data_url.substr(comma + 1));
	return _validate_payload_mime(r_mime, r_bytes, r_error);
}

bool AIAttachmentResolver::_is_path_inside_root(const String &p_path, const String &p_root) {
	const String root = p_root.simplify_path().trim_suffix("/");
	const String resolved = p_path.simplify_path();
	if (root.is_empty()) {
		return true;
	}
	return resolved == root || resolved.begins_with(root + "/") || resolved.begins_with(root + "\\");
}

String AIAttachmentResolver::_resolve_location_path(const AILocationRef &p_location, const String &p_path, AIError &r_error) {
	String path = p_path.strip_edges();
	if (path.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment path cannot be empty.");
		return String();
	}

	const String root_dir = p_location.directory.strip_edges();
	if (path.begins_with("res://") || path.begins_with("user://")) {
		path = path.simplify_path();
	} else if (!root_dir.is_empty() && !path.is_absolute_path()) {
		path = root_dir.path_join(path).simplify_path();
	} else {
		path = path.simplify_path();
	}

	if (!root_dir.is_empty() && !_is_path_inside_root(path, root_dir)) {
		Dictionary details;
		details["path"] = path;
		details["root_dir"] = root_dir.simplify_path();
		r_error = AIError::make(AI_ERROR_PERMISSION, "Attachment path escapes its location root.", details);
		return String();
	}

	if (ProjectSettings::get_singleton() && !path.begins_with("res://") && !path.begins_with("user://")) {
		path = ProjectSettings::get_singleton()->localize_path(path).simplify_path();
	}

	r_error = AIError::none();
	return path;
}

AIFileAttachment AIAttachmentResolver::_attachment_from_blob_record(const AIAttachmentBlobRecord &p_record, const String &p_attachment_id, const String &p_name, const String &p_source_type, const Dictionary &p_extra_metadata) {
	AIFileAttachment file;
	file.id = p_attachment_id.strip_edges().is_empty() ? p_record.id : p_attachment_id.strip_edges();
	file.path = p_record.id;
	file.name = p_name.strip_edges().is_empty() ? String(p_record.source_metadata.get("name", p_record.id)) : p_name.strip_edges();
	file.mime = p_record.mime_type.strip_edges().is_empty() ? String("application/octet-stream") : p_record.mime_type.strip_edges().to_lower();
	file.size_bytes = int64_t(p_record.size);
	file.metadata = p_extra_metadata.duplicate(true);
	file.metadata["blob_id"] = p_record.id;
	file.metadata["blob_hash"] = p_record.hash;
	file.metadata["source_type"] = p_source_type;
	file.metadata["content_addressed"] = true;
	return file;
}

void AIAttachmentResolver::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_blob_store", "blob_store"), &AIAttachmentResolver::set_blob_store);
	ClassDB::bind_method(D_METHOD("get_blob_store"), &AIAttachmentResolver::get_blob_store);
	ClassDB::bind_method(D_METHOD("set_max_attachment_bytes", "max_attachment_bytes"), &AIAttachmentResolver::set_max_attachment_bytes);
	ClassDB::bind_method(D_METHOD("get_max_attachment_bytes"), &AIAttachmentResolver::get_max_attachment_bytes);
	ClassDB::bind_method(D_METHOD("detect_mime", "name", "bytes", "declared_mime"), &AIAttachmentResolver::detect_mime, DEFVAL(PackedByteArray()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("mime_to_modality", "mime"), &AIAttachmentResolver::mime_to_modality);
	ClassDB::bind_method(D_METHOD("resolve", "input"), &AIAttachmentResolver::resolve);
}

AIAttachmentResolver::AIAttachmentResolver() {
	blob_store.instantiate();
}

String AIAttachmentResolver::detect_mime_static(const String &p_name, const PackedByteArray &p_bytes, const String &p_declared_mime) {
	const String sniffed = _mime_from_bytes(p_bytes);
	if (!sniffed.is_empty()) {
		return sniffed;
	}
	const String declared = p_declared_mime.strip_edges().to_lower();
	if (!declared.is_empty()) {
		return declared;
	}
	const String by_ext = _mime_from_extension(p_name);
	if (!by_ext.is_empty()) {
		return by_ext;
	}
	return "application/octet-stream";
}

String AIAttachmentResolver::mime_to_modality_static(const String &p_mime) {
	const String mime = p_mime.strip_edges().to_lower();
	if (mime.begins_with("image/")) {
		return "image";
	}
	if (mime.begins_with("audio/")) {
		return "audio";
	}
	if (mime.begins_with("text/") || mime == "application/json" || mime == "application/x-ndjson" || mime == "application/xml" || mime.ends_with("+json") || mime.ends_with("+xml")) {
		return "text";
	}
	return "file";
}

void AIAttachmentResolver::set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store) {
	blob_store = p_blob_store;
	if (blob_store.is_null()) {
		blob_store.instantiate();
	}
}

Ref<AIAttachmentBlobStore> AIAttachmentResolver::get_blob_store() const {
	return blob_store;
}

void AIAttachmentResolver::set_max_attachment_bytes(int64_t p_max_attachment_bytes) {
	max_attachment_bytes = p_max_attachment_bytes < 1024 ? 1024 : p_max_attachment_bytes;
}

int64_t AIAttachmentResolver::get_max_attachment_bytes() const {
	return max_attachment_bytes;
}

bool AIAttachmentResolver::_ensure_blob_store(AIError &r_error) {
	if (blob_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "AttachmentResolver has no AttachmentBlobStore.");
		return false;
	}
	r_error = AIError::none();
	return true;
}

bool AIAttachmentResolver::_resolve_data_attachment(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error) {
	if (!_ensure_blob_store(r_error)) {
		return false;
	}

	const String data_url = String(p_attachment.get("data_url", p_attachment.get("dataURL", p_attachment.get("data", String())))).strip_edges();
	String parsed_mime;
	PackedByteArray bytes;
	if (!_parse_data_url(data_url, parsed_mime, bytes, r_error)) {
		return false;
	}

	const String declared_mime = _declared_mime_from_attachment(p_attachment);
	if (!declared_mime.is_empty() && declared_mime != parsed_mime) {
		Dictionary details;
		details["declared_mime"] = declared_mime;
		details["data_url_mime"] = parsed_mime;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment MIME does not match its data URL.", details);
		return false;
	}

	if (bytes.size() > max_attachment_bytes) {
		Dictionary details;
		details["size_bytes"] = bytes.size();
		details["max_bytes"] = max_attachment_bytes;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment exceeds the configured byte limit.", details);
		return false;
	}
	if (!_validate_payload_mime(parsed_mime, bytes, r_error)) {
		return false;
	}

	Dictionary source_metadata;
	source_metadata["source_type"] = "data";
	source_metadata["session_id"] = p_session_id;
	source_metadata["workspace_id"] = p_location.workspace_id;
	source_metadata["name"] = _name_from_attachment(p_attachment, String());
	source_metadata["submitted_id"] = String(p_attachment.get("id", String()));

	AIAttachmentBlobRecord record;
	if (!blob_store->put_bytes_struct(bytes, parsed_mime, source_metadata, record, r_error)) {
		return false;
	}

	Dictionary attachment_metadata;
	attachment_metadata["submitted_id"] = String(p_attachment.get("id", String()));
	r_file = _attachment_from_blob_record(record, String(p_attachment.get("id", String())), String(source_metadata["name"]), "data", attachment_metadata);
	r_error = AIError::none();
	return true;
}

bool AIAttachmentResolver::_resolve_text_attachment(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error) {
	if (!_ensure_blob_store(r_error)) {
		return false;
	}

	const String text = String(p_attachment.get("text", p_attachment.get("content", String())));
	if (text.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Text attachment content cannot be empty.");
		return false;
	}

	const PackedByteArray bytes = text.to_utf8_buffer();
	if (bytes.size() > max_attachment_bytes) {
		Dictionary details;
		details["size_bytes"] = bytes.size();
		details["max_bytes"] = max_attachment_bytes;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment exceeds the configured byte limit.", details);
		return false;
	}

	const String name = _name_from_attachment(p_attachment, "attachment.txt");
	const String mime = detect_mime_static(name, bytes, _declared_mime_from_attachment(p_attachment));
	if (!_validate_payload_mime(mime, bytes, r_error)) {
		return false;
	}

	Dictionary source_metadata;
	source_metadata["source_type"] = "text";
	source_metadata["session_id"] = p_session_id;
	source_metadata["workspace_id"] = p_location.workspace_id;
	source_metadata["name"] = name;
	source_metadata["submitted_id"] = String(p_attachment.get("id", String()));
	if (p_attachment.has("source")) {
		source_metadata["source"] = p_attachment["source"];
	}
	if (p_attachment.has("label")) {
		source_metadata["label"] = p_attachment["label"];
	}

	AIAttachmentBlobRecord record;
	if (!blob_store->put_bytes_struct(bytes, mime, source_metadata, record, r_error)) {
		return false;
	}

	Dictionary attachment_metadata;
	attachment_metadata["submitted_id"] = String(p_attachment.get("id", String()));
	r_file = _attachment_from_blob_record(record, String(p_attachment.get("id", String())), name, "text", attachment_metadata);
	r_error = AIError::none();
	return true;
}

bool AIAttachmentResolver::_resolve_path_attachment(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error) {
	if (!_ensure_blob_store(r_error)) {
		return false;
	}

	const String original_path = String(p_attachment.get("path", String())).strip_edges();
	const String path = _resolve_location_path(p_location, original_path, r_error);
	if (path.is_empty()) {
		return false;
	}

	Error open_err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &open_err);
	if (file.is_null() || open_err != OK) {
		Dictionary details;
		details["path"] = path;
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Failed to open attachment file.", details);
		return false;
	}

	const uint64_t length = file->get_length();
	if (length > uint64_t(max_attachment_bytes)) {
		Dictionary details;
		details["path"] = path;
		details["size_bytes"] = length;
		details["max_bytes"] = max_attachment_bytes;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment exceeds the configured byte limit.", details);
		return false;
	}
	if (length > uint64_t(INT32_MAX)) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment is too large to load into memory.");
		return false;
	}

	PackedByteArray bytes = file->get_buffer(length);
	const String name = _name_from_attachment(p_attachment, path.get_file());
	const String mime = detect_mime_static(name, bytes, _declared_mime_from_attachment(p_attachment));

	Dictionary source_metadata;
	source_metadata["source_type"] = "path";
	source_metadata["session_id"] = p_session_id;
	source_metadata["workspace_id"] = p_location.workspace_id;
	source_metadata["source_path"] = path;
	source_metadata["name"] = name;
	source_metadata["submitted_id"] = String(p_attachment.get("id", String()));

	AIAttachmentBlobRecord record;
	if (!blob_store->put_bytes_struct(bytes, mime, source_metadata, record, r_error)) {
		return false;
	}

	Dictionary attachment_metadata;
	attachment_metadata["submitted_id"] = String(p_attachment.get("id", String()));
	r_file = _attachment_from_blob_record(record, String(p_attachment.get("id", String())), name, "path", attachment_metadata);
	r_error = AIError::none();
	return true;
}

bool AIAttachmentResolver::_resolve_blob_attachment(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error) {
	(void)p_session_id;
	(void)p_location;
	if (!_ensure_blob_store(r_error)) {
		return false;
	}

	String blob_id = String(p_attachment.get("blob_id", p_attachment.get("blobID", String()))).strip_edges();
	if (blob_id.is_empty()) {
		blob_id = String(p_attachment.get("path", String())).strip_edges();
	}
	if (blob_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Blob attachment requires blob_id.");
		return false;
	}

	AIAttachmentBlobRecord record;
	if (!blob_store->get_metadata_struct(blob_id, record, r_error)) {
		return false;
	}

	const String current_session_id = p_session_id.strip_edges();
	const String record_session_id = String(record.source_metadata.get("session_id", String())).strip_edges();
	if (!current_session_id.is_empty() && record_session_id != current_session_id) {
		Dictionary details;
		details["blob_id"] = record.id;
		details["session_id"] = current_session_id;
		details["blob_session_id"] = record_session_id;
		r_error = AIError::make(AI_ERROR_PERMISSION, "Blob attachment does not belong to this session.", details);
		return false;
	}
	if (record.size > uint64_t(max_attachment_bytes)) {
		Dictionary details;
		details["blob_id"] = record.id;
		details["size_bytes"] = record.size;
		details["max_bytes"] = max_attachment_bytes;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment blob exceeds the configured byte limit.", details);
		return false;
	}

	Dictionary attachment_metadata;
	attachment_metadata["submitted_id"] = String(p_attachment.get("id", String()));
	r_file = _attachment_from_blob_record(record, String(p_attachment.get("id", String())), _name_from_attachment(p_attachment, String(record.source_metadata.get("name", record.id))), "blob", attachment_metadata);
	r_error = AIError::none();
	return true;
}

bool AIAttachmentResolver::resolve_struct(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error) {
	Dictionary attachment = p_attachment;
	if (attachment.get("attachment", Variant()).get_type() == Variant::DICTIONARY) {
		attachment = Dictionary(attachment["attachment"]).duplicate(true);
	}

	if (attachment.has("bytes")) {
		const PackedByteArray bytes = _bytes_from_variant(attachment["bytes"]);
		if (bytes.size() > 0) {
			if (bytes.size() > max_attachment_bytes) {
				Dictionary details;
				details["size_bytes"] = bytes.size();
				details["max_bytes"] = max_attachment_bytes;
				r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment exceeds the configured byte limit.", details);
				return false;
			}
			const String name = _name_from_attachment(attachment, String());
			const String mime = detect_mime_static(name, bytes, _declared_mime_from_attachment(attachment));
			Dictionary source_metadata;
			source_metadata["source_type"] = "bytes";
			source_metadata["session_id"] = p_session_id;
			source_metadata["workspace_id"] = p_location.workspace_id;
			source_metadata["name"] = name;
			source_metadata["submitted_id"] = String(attachment.get("id", String()));
			AIAttachmentBlobRecord record;
			if (!blob_store->put_bytes_struct(bytes, mime, source_metadata, record, r_error)) {
				return false;
			}
			Dictionary attachment_metadata;
			attachment_metadata["submitted_id"] = String(attachment.get("id", String()));
			r_file = _attachment_from_blob_record(record, String(attachment.get("id", String())), name, "bytes", attachment_metadata);
			r_error = AIError::none();
			return true;
		}
	}

	const String type = _attachment_type(attachment);
	if (type == "data" || type == "dataurl" || type == "data_url") {
		return _resolve_data_attachment(p_session_id, p_location, attachment, r_file, r_error);
	}
	if (type == "blob") {
		return _resolve_blob_attachment(p_session_id, p_location, attachment, r_file, r_error);
	}
	if (type == "text" && (attachment.has("text") || attachment.has("content"))) {
		return _resolve_text_attachment(p_session_id, p_location, attachment, r_file, r_error);
	}
	return _resolve_path_attachment(p_session_id, p_location, attachment, r_file, r_error);
}

bool AIAttachmentResolver::resolve_many_struct(const String &p_session_id, const AILocationRef &p_location, const Array &p_attachments, Vector<AIFileAttachment> &r_files, AIError &r_error) {
	for (int i = 0; i < p_attachments.size(); i++) {
		if (p_attachments[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		AIFileAttachment file;
		if (!resolve_struct(p_session_id, p_location, p_attachments[i], file, r_error)) {
			return false;
		}
		r_files.push_back(file);
	}
	r_error = AIError::none();
	return true;
}

String AIAttachmentResolver::detect_mime(const String &p_name, const PackedByteArray &p_bytes, const String &p_declared_mime) const {
	return detect_mime_static(p_name, p_bytes, p_declared_mime);
}

String AIAttachmentResolver::mime_to_modality(const String &p_mime) const {
	return mime_to_modality_static(p_mime);
}

Dictionary AIAttachmentResolver::resolve(const Dictionary &p_input) {
	const Dictionary attachment = _dictionary_from_variant(p_input.get("attachment", p_input));
	const AILocationRef location = AILocationRef::from_dictionary(_dictionary_from_variant(p_input.get("location", Dictionary())));
	const String session_id = String(p_input.get("session_id", p_input.get("sessionID", String()))).strip_edges();

	AIFileAttachment file;
	AIError error;
	if (!resolve_struct(session_id, location, attachment, file, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result = file.to_dictionary();
	result["success"] = true;
	return result;
}
