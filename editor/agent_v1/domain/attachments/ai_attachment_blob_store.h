/**************************************************************************/
/*  ai_attachment_blob_store.h                                            */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

struct AIAttachmentBlobRecord {
	String id;
	String hash;
	String mime_type;
	String content_path;
	String metadata_path;
	Dictionary source_metadata;
	uint64_t size = 0;
	uint64_t created_at = 0;

	Dictionary to_dictionary() const;
	static AIAttachmentBlobRecord from_dictionary(const Dictionary &p_dict);
};

class AIAttachmentBlobStore : public RefCounted {
	GDCLASS(AIAttachmentBlobStore, RefCounted);

	String base_dir;
	mutable Mutex mutex;

	static uint64_t _now_unix_time();
	static String _hash_bytes(const PackedByteArray &p_bytes, AIError &r_error);
	static String _normalize_blob_ref(const String &p_blob_ref);
	String _content_path_for_hash(const String &p_hash) const;
	String _metadata_path_for_hash(const String &p_hash) const;
	bool _ensure_dirs_locked(const String &p_hash, AIError &r_error) const;
	bool _write_metadata_locked(const AIAttachmentBlobRecord &p_record, AIError &r_error) const;
	bool _read_metadata_locked(const String &p_blob_ref, AIAttachmentBlobRecord &r_record, AIError &r_error) const;

protected:
	static void _bind_methods();

public:
	AIAttachmentBlobStore();

	void set_base_dir(const String &p_base_dir);
	String get_base_dir() const;

	bool put_bytes_struct(const PackedByteArray &p_bytes, const String &p_mime_type, const Dictionary &p_source_metadata, AIAttachmentBlobRecord &r_record, AIError &r_error);
	bool put_file_struct(const String &p_path, const String &p_mime_type, const Dictionary &p_source_metadata, AIAttachmentBlobRecord &r_record, AIError &r_error);
	bool has_blob_struct(const String &p_blob_ref) const;
	bool get_metadata_struct(const String &p_blob_ref, AIAttachmentBlobRecord &r_record, AIError &r_error) const;
	bool read_bytes_struct(const String &p_blob_ref, PackedByteArray &r_bytes, AIError &r_error) const;

	Dictionary put_bytes(const PackedByteArray &p_bytes, const String &p_mime_type = String(), const Dictionary &p_source_metadata = Dictionary());
	Dictionary put_file(const String &p_path, const String &p_mime_type = String(), const Dictionary &p_source_metadata = Dictionary());
	Dictionary get_metadata(const String &p_blob_ref) const;
	Dictionary read_bytes(const String &p_blob_ref) const;
	bool has_blob(const String &p_blob_ref) const;
};
