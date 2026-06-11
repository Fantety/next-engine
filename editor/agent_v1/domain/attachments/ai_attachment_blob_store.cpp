/**************************************************************************/
/*  ai_attachment_blob_store.cpp                                          */
/**************************************************************************/

#include "ai_attachment_blob_store.h"

#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/string/ustring.h"

#include <cstdint>

Dictionary AIAttachmentBlobRecord::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["hash"] = hash;
	result["mime_type"] = mime_type;
	result["content_path"] = content_path;
	result["metadata_path"] = metadata_path;
	result["source_metadata"] = source_metadata;
	result["size"] = size;
	result["created_at"] = created_at;
	return result;
}

AIAttachmentBlobRecord AIAttachmentBlobRecord::from_dictionary(const Dictionary &p_dict) {
	AIAttachmentBlobRecord result;
	result.id = p_dict.get("id", String());
	result.hash = p_dict.get("hash", String());
	result.mime_type = p_dict.get("mime_type", p_dict.get("mimeType", String()));
	result.content_path = p_dict.get("content_path", p_dict.get("contentPath", String()));
	result.metadata_path = p_dict.get("metadata_path", p_dict.get("metadataPath", String()));
	if (p_dict.get("source_metadata", p_dict.get("sourceMetadata", Variant())).get_type() == Variant::DICTIONARY) {
		result.source_metadata = Dictionary(p_dict.get("source_metadata", p_dict.get("sourceMetadata", Dictionary()))).duplicate(true);
	}
	result.size = uint64_t(p_dict.get("size", 0));
	result.created_at = uint64_t(p_dict.get("created_at", p_dict.get("createdAt", 0)));
	return result;
}

void AIAttachmentBlobStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_dir", "base_dir"), &AIAttachmentBlobStore::set_base_dir);
	ClassDB::bind_method(D_METHOD("get_base_dir"), &AIAttachmentBlobStore::get_base_dir);
	ClassDB::bind_method(D_METHOD("put_bytes", "bytes", "mime_type", "source_metadata"), &AIAttachmentBlobStore::put_bytes, DEFVAL(String()), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("put_file", "path", "mime_type", "source_metadata"), &AIAttachmentBlobStore::put_file, DEFVAL(String()), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_metadata", "blob_ref"), &AIAttachmentBlobStore::get_metadata);
	ClassDB::bind_method(D_METHOD("read_bytes", "blob_ref"), &AIAttachmentBlobStore::read_bytes);
	ClassDB::bind_method(D_METHOD("has_blob", "blob_ref"), &AIAttachmentBlobStore::has_blob);
}

AIAttachmentBlobStore::AIAttachmentBlobStore() {
	base_dir = "user://net.nextengine/agent_v1/attachments";
}

uint64_t AIAttachmentBlobStore::_now_unix_time() {
	return Time::get_singleton() ? Time::get_singleton()->get_unix_time_from_system() : 0;
}

String AIAttachmentBlobStore::_hash_bytes(const PackedByteArray &p_bytes, AIError &r_error) {
	unsigned char hash[32];
	const uint8_t empty = 0;
	const uint8_t *bytes = p_bytes.size() > 0 ? p_bytes.ptr() : &empty;
	if (CryptoCore::sha256(bytes, p_bytes.size(), hash) != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to hash attachment blob bytes.");
		return String();
	}
	r_error = AIError::none();
	return String::hex_encode_buffer(hash, 32);
}

String AIAttachmentBlobStore::_normalize_blob_ref(const String &p_blob_ref) {
	String blob_ref = p_blob_ref.strip_edges();
	if (blob_ref.begins_with("blob_")) {
		blob_ref = blob_ref.substr(5);
	}
	return blob_ref.to_lower().validate_filename();
}

String AIAttachmentBlobStore::_content_path_for_hash(const String &p_hash) const {
	const String hash = _normalize_blob_ref(p_hash);
	const String prefix = hash.length() >= 2 ? hash.substr(0, 2) : String("xx");
	return base_dir.path_join("blobs").path_join(prefix).path_join(hash + ".bin");
}

String AIAttachmentBlobStore::_metadata_path_for_hash(const String &p_hash) const {
	const String hash = _normalize_blob_ref(p_hash);
	const String prefix = hash.length() >= 2 ? hash.substr(0, 2) : String("xx");
	return base_dir.path_join("metadata").path_join(prefix).path_join(hash + ".json");
}

bool AIAttachmentBlobStore::_ensure_dirs_locked(const String &p_hash, AIError &r_error) const {
	if (base_dir.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment blob store base_dir cannot be empty.");
		return false;
	}

	const Error content_err = DirAccess::make_dir_recursive_absolute(_content_path_for_hash(p_hash).get_base_dir());
	if (content_err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to create attachment blob content directory.");
		return false;
	}

	const Error metadata_err = DirAccess::make_dir_recursive_absolute(_metadata_path_for_hash(p_hash).get_base_dir());
	if (metadata_err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to create attachment blob metadata directory.");
		return false;
	}

	r_error = AIError::none();
	return true;
}

bool AIAttachmentBlobStore::_write_metadata_locked(const AIAttachmentBlobRecord &p_record, AIError &r_error) const {
	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(p_record.metadata_path, FileAccess::WRITE, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open attachment blob metadata for write: " + p_record.metadata_path);
		return false;
	}

	file->store_string(JSON::stringify(p_record.to_dictionary()));
	file->flush();
	r_error = AIError::none();
	return true;
}

bool AIAttachmentBlobStore::_read_metadata_locked(const String &p_blob_ref, AIAttachmentBlobRecord &r_record, AIError &r_error) const {
	const String hash = _normalize_blob_ref(p_blob_ref);
	if (hash.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment blob ref cannot be empty.");
		return false;
	}

	const String metadata_path = _metadata_path_for_hash(hash);
	if (!FileAccess::exists(metadata_path)) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Attachment blob metadata not found: " + hash);
		return false;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(metadata_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open attachment blob metadata: " + metadata_path);
		return false;
	}

	Ref<JSON> json;
	json.instantiate();
	const Error parse_err = json->parse(file->get_as_text());
	if (parse_err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to parse attachment blob metadata: " + metadata_path);
		return false;
	}

	r_record = AIAttachmentBlobRecord::from_dictionary(json->get_data());
	if (r_record.hash.is_empty()) {
		r_record.hash = hash;
	}
	if (r_record.id.is_empty()) {
		r_record.id = "blob_" + r_record.hash;
	}
	if (r_record.content_path.is_empty()) {
		r_record.content_path = _content_path_for_hash(r_record.hash);
	}
	if (r_record.metadata_path.is_empty()) {
		r_record.metadata_path = metadata_path;
	}
	r_error = AIError::none();
	return true;
}

void AIAttachmentBlobStore::set_base_dir(const String &p_base_dir) {
	MutexLock lock(mutex);
	base_dir = p_base_dir.strip_edges();
}

String AIAttachmentBlobStore::get_base_dir() const {
	MutexLock lock(mutex);
	return base_dir;
}

bool AIAttachmentBlobStore::put_bytes_struct(const PackedByteArray &p_bytes, const String &p_mime_type, const Dictionary &p_source_metadata, AIAttachmentBlobRecord &r_record, AIError &r_error) {
	const String hash = _hash_bytes(p_bytes, r_error);
	if (hash.is_empty()) {
		return false;
	}

	MutexLock lock(mutex);
	if (!_ensure_dirs_locked(hash, r_error)) {
		return false;
	}

	const String content_path = _content_path_for_hash(hash);
	if (!FileAccess::exists(content_path)) {
		Error err = OK;
		Ref<FileAccess> file = FileAccess::open(content_path, FileAccess::WRITE, &err);
		if (file.is_null() || err != OK) {
			r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to open attachment blob content for write: " + content_path);
			return false;
		}
		if (p_bytes.size() > 0 && !file->store_buffer(p_bytes.ptr(), p_bytes.size())) {
			r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to write attachment blob content: " + content_path);
			return false;
		}
		file->flush();
	}

	AIAttachmentBlobRecord record;
	record.id = "blob_" + hash;
	record.hash = hash;
	record.mime_type = p_mime_type.strip_edges();
	record.content_path = content_path;
	record.metadata_path = _metadata_path_for_hash(hash);
	record.source_metadata = p_source_metadata.duplicate(true);
	record.size = p_bytes.size();
	record.created_at = _now_unix_time();
	if (!_write_metadata_locked(record, r_error)) {
		return false;
	}

	r_record = record;
	r_error = AIError::none();
	return true;
}

bool AIAttachmentBlobStore::put_file_struct(const String &p_path, const String &p_mime_type, const Dictionary &p_source_metadata, AIAttachmentBlobRecord &r_record, AIError &r_error) {
	const String path = p_path.strip_edges();
	if (path.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment file path cannot be empty.");
		return false;
	}

	Error err = OK;
	PackedByteArray bytes = FileAccess::get_file_as_bytes(path, &err);
	if (err != OK) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Failed to read attachment file: " + path);
		return false;
	}

	Dictionary source_metadata = p_source_metadata.duplicate(true);
	if (!source_metadata.has("source_path")) {
		source_metadata["source_path"] = path;
	}
	return put_bytes_struct(bytes, p_mime_type, source_metadata, r_record, r_error);
}

bool AIAttachmentBlobStore::has_blob_struct(const String &p_blob_ref) const {
	const String hash = _normalize_blob_ref(p_blob_ref);
	if (hash.is_empty()) {
		return false;
	}

	MutexLock lock(mutex);
	return FileAccess::exists(_content_path_for_hash(hash)) && FileAccess::exists(_metadata_path_for_hash(hash));
}

bool AIAttachmentBlobStore::get_metadata_struct(const String &p_blob_ref, AIAttachmentBlobRecord &r_record, AIError &r_error) const {
	MutexLock lock(mutex);
	return _read_metadata_locked(p_blob_ref, r_record, r_error);
}

bool AIAttachmentBlobStore::read_bytes_struct(const String &p_blob_ref, PackedByteArray &r_bytes, AIError &r_error) const {
	AIAttachmentBlobRecord record;
	{
		MutexLock lock(mutex);
		if (!_read_metadata_locked(p_blob_ref, record, r_error)) {
			return false;
		}
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(record.content_path, FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Attachment blob content not found: " + record.hash);
		return false;
	}

	const uint64_t length = file->get_length();
	if (length > uint64_t(INT32_MAX)) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment blob is too large to load into memory.");
		return false;
	}

	r_bytes.resize(int(length));
	if (length > 0 && file->get_buffer(r_bytes.ptrw(), length) != length) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to read attachment blob content: " + record.hash);
		return false;
	}

	r_error = AIError::none();
	return true;
}

Dictionary AIAttachmentBlobStore::put_bytes(const PackedByteArray &p_bytes, const String &p_mime_type, const Dictionary &p_source_metadata) {
	AIAttachmentBlobRecord record;
	AIError error;
	if (!put_bytes_struct(p_bytes, p_mime_type, p_source_metadata, record, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result = record.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AIAttachmentBlobStore::put_file(const String &p_path, const String &p_mime_type, const Dictionary &p_source_metadata) {
	AIAttachmentBlobRecord record;
	AIError error;
	if (!put_file_struct(p_path, p_mime_type, p_source_metadata, record, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result = record.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AIAttachmentBlobStore::get_metadata(const String &p_blob_ref) const {
	AIAttachmentBlobRecord record;
	AIError error;
	if (!get_metadata_struct(p_blob_ref, record, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result = record.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AIAttachmentBlobStore::read_bytes(const String &p_blob_ref) const {
	PackedByteArray bytes;
	AIError error;
	if (!read_bytes_struct(p_blob_ref, bytes, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	AIAttachmentBlobRecord record;
	get_metadata_struct(p_blob_ref, record, error);

	Dictionary result;
	result["success"] = true;
	result["bytes"] = bytes;
	result["metadata"] = record.to_dictionary();
	return result;
}

bool AIAttachmentBlobStore::has_blob(const String &p_blob_ref) const {
	return has_blob_struct(p_blob_ref);
}
