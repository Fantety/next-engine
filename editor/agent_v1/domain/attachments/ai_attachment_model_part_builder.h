/**************************************************************************/
/*  ai_attachment_model_part_builder.h                                    */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/runtime/ai_model_request.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_resolver.h"

#include "core/io/image.h"
#include "core/object/ref_counted.h"

class AIFilePreprocessor : public RefCounted {
	GDCLASS(AIFilePreprocessor, RefCounted);

	int64_t max_text_bytes = 256 * 1024;

protected:
	static void _bind_methods();

public:
	void set_max_text_bytes(int64_t p_max_text_bytes);
	int64_t get_max_text_bytes() const;

	bool bytes_to_text_context_struct(const AIFileAttachment &p_attachment, const PackedByteArray &p_bytes, String &r_text, bool &r_truncated, AIError &r_error) const;
	Dictionary bytes_to_text_context(const Dictionary &p_attachment, const PackedByteArray &p_bytes) const;
};

class AIImageNormalizer : public RefCounted {
	GDCLASS(AIImageNormalizer, RefCounted);

	Ref<AIAttachmentBlobStore> blob_store;
	int max_width = 2048;
	int max_height = 2048;
	int64_t max_output_bytes = 20 * 1024 * 1024;

	static bool _decode_image(const String &p_mime, const PackedByteArray &p_bytes, Ref<Image> &r_image, AIError &r_error);

protected:
	static void _bind_methods();

public:
	AIImageNormalizer();

	void set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store);
	Ref<AIAttachmentBlobStore> get_blob_store() const;
	void set_max_width(int p_max_width);
	int get_max_width() const;
	void set_max_height(int p_max_height);
	int get_max_height() const;
	void set_max_output_bytes(int64_t p_max_output_bytes);
	int64_t get_max_output_bytes() const;

	bool normalize_struct(const AIFileAttachment &p_attachment, AIFileAttachment &r_attachment, AIError &r_error);
	Dictionary normalize(const Dictionary &p_attachment);
};

class AIModelPartBuilder : public RefCounted {
	GDCLASS(AIModelPartBuilder, RefCounted);

	Ref<AIAttachmentBlobStore> blob_store;
	Ref<AIFilePreprocessor> file_preprocessor;
	Ref<AIImageNormalizer> image_normalizer;
	int64_t max_inline_bytes = 20 * 1024 * 1024;

	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Array _array_from_variant(const Variant &p_value);
	static bool _array_contains_string(const Array &p_array, const String &p_value);
	static Array _provider_input_modalities(const Dictionary &p_provider_config);
	static AIModelPartType _part_type_for_modality(const String &p_modality);
	static String _blob_ref_from_attachment(const AIFileAttachment &p_attachment);
	static String _bytes_to_data_url(const String &p_mime, const PackedByteArray &p_bytes, AIError &r_error);
	static Dictionary _safe_part_metadata(const AIFileAttachment &p_attachment, const String &p_modality, const String &p_blob_ref);

	bool _ensure_dependencies(AIError &r_error);
	bool _read_attachment_bytes(const AIFileAttachment &p_attachment, PackedByteArray &r_bytes, String &r_blob_ref, AIError &r_error) const;
	bool _build_text_part(const AIFileAttachment &p_attachment, AIModelPart &r_part, AIError &r_error);
	bool _build_binary_part(const AIFileAttachment &p_attachment, const Dictionary &p_provider_config, const String &p_modality, AIModelPart &r_part, AIError &r_error);

protected:
	static void _bind_methods();

public:
	AIModelPartBuilder();

	static bool provider_supports_modality_static(const Dictionary &p_provider_config, const String &p_modality);

	void set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store);
	Ref<AIAttachmentBlobStore> get_blob_store() const;
	void set_file_preprocessor(const Ref<AIFilePreprocessor> &p_file_preprocessor);
	Ref<AIFilePreprocessor> get_file_preprocessor() const;
	void set_image_normalizer(const Ref<AIImageNormalizer> &p_image_normalizer);
	Ref<AIImageNormalizer> get_image_normalizer() const;
	void set_max_inline_bytes(int64_t p_max_inline_bytes);
	int64_t get_max_inline_bytes() const;

	bool build_attachment_part_struct(const AIFileAttachment &p_attachment, const Dictionary &p_provider_config, AIModelPart &r_part, AIError &r_error);
	bool append_attachment_parts_struct(const Vector<AIFileAttachment> &p_attachments, const Dictionary &p_provider_config, Vector<AIModelPart> &r_parts, AIError &r_error);

	bool provider_supports_modality(const Dictionary &p_provider_config, const String &p_modality) const;
	Dictionary build_attachment_part(const Dictionary &p_attachment, const Dictionary &p_provider_config);
};
