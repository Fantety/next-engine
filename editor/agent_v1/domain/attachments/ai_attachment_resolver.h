/**************************************************************************/
/*  ai_attachment_resolver.h                                              */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/attachments/ai_attachment_blob_store.h"
#include "editor/agent_v1/domain/model/ai_domain_types.h"

#include "core/object/ref_counted.h"

class AIAttachmentResolver : public RefCounted {
	GDCLASS(AIAttachmentResolver, RefCounted);

	Ref<AIAttachmentBlobStore> blob_store;
	int64_t max_attachment_bytes = 20 * 1024 * 1024;

	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static PackedByteArray _bytes_from_variant(const Variant &p_value);
	static String _attachment_type(const Dictionary &p_attachment);
	static String _name_from_attachment(const Dictionary &p_attachment, const String &p_fallback);
	static String _declared_mime_from_attachment(const Dictionary &p_attachment);
	static String _mime_from_extension(const String &p_name);
	static String _mime_from_bytes(const PackedByteArray &p_bytes);
	static String _canonical_mime(const String &p_mime);
	static bool _mime_requires_sniff_match(const String &p_mime);
	static bool _validate_payload_mime(const String &p_mime, const PackedByteArray &p_bytes, AIError &r_error);
	static bool _parse_data_url(const String &p_data_url, String &r_mime, PackedByteArray &r_bytes, AIError &r_error);
	static String _resolve_location_path(const AILocationRef &p_location, const String &p_path, AIError &r_error);
	static bool _is_path_inside_root(const String &p_path, const String &p_root);
	static AIFileAttachment _attachment_from_blob_record(const AIAttachmentBlobRecord &p_record, const String &p_attachment_id, const String &p_name, const String &p_source_type, const Dictionary &p_extra_metadata);

	bool _ensure_blob_store(AIError &r_error);
	bool _resolve_data_attachment(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error);
	bool _resolve_text_attachment(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error);
	bool _resolve_path_attachment(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error);
	bool _resolve_blob_attachment(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error);

protected:
	static void _bind_methods();

public:
	AIAttachmentResolver();

	static String detect_mime_static(const String &p_name, const PackedByteArray &p_bytes = PackedByteArray(), const String &p_declared_mime = String());
	static String mime_to_modality_static(const String &p_mime);

	void set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store);
	Ref<AIAttachmentBlobStore> get_blob_store() const;
	void set_max_attachment_bytes(int64_t p_max_attachment_bytes);
	int64_t get_max_attachment_bytes() const;

	bool resolve_struct(const String &p_session_id, const AILocationRef &p_location, const Dictionary &p_attachment, AIFileAttachment &r_file, AIError &r_error);
	bool resolve_many_struct(const String &p_session_id, const AILocationRef &p_location, const Array &p_attachments, Vector<AIFileAttachment> &r_files, AIError &r_error);

	String detect_mime(const String &p_name, const PackedByteArray &p_bytes = PackedByteArray(), const String &p_declared_mime = String()) const;
	String mime_to_modality(const String &p_mime) const;
	Dictionary resolve(const Dictionary &p_input);
};
