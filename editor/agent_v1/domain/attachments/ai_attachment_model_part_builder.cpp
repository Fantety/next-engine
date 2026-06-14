/**************************************************************************/
/*  ai_attachment_model_part_builder.cpp                                  */
/**************************************************************************/

#include "ai_attachment_model_part_builder.h"

#include "core/core_bind.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"

void AIFilePreprocessor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_text_bytes", "max_text_bytes"), &AIFilePreprocessor::set_max_text_bytes);
	ClassDB::bind_method(D_METHOD("get_max_text_bytes"), &AIFilePreprocessor::get_max_text_bytes);
	ClassDB::bind_method(D_METHOD("bytes_to_text_context", "attachment", "bytes"), &AIFilePreprocessor::bytes_to_text_context);
}

void AIFilePreprocessor::set_max_text_bytes(int64_t p_max_text_bytes) {
	max_text_bytes = p_max_text_bytes < 1024 ? 1024 : p_max_text_bytes;
}

int64_t AIFilePreprocessor::get_max_text_bytes() const {
	return max_text_bytes;
}

bool AIFilePreprocessor::bytes_to_text_context_struct(const AIFileAttachment &p_attachment, const PackedByteArray &p_bytes, String &r_text, bool &r_truncated, AIError &r_error) const {
	r_truncated = false;
	const int64_t limit = max_text_bytes < 1024 ? 1024 : max_text_bytes;
	const int read_size = int(p_bytes.size() < limit ? p_bytes.size() : limit);

	String body;
	if (read_size > 0) {
		body = String::utf8(reinterpret_cast<const char *>(p_bytes.ptr()), read_size);
	}
	if (p_bytes.size() > read_size) {
		r_truncated = true;
	}

	const String name = p_attachment.name.strip_edges().is_empty() ? p_attachment.id : p_attachment.name.strip_edges();
	const String mime = p_attachment.mime.strip_edges().is_empty() ? String("text/plain") : p_attachment.mime.strip_edges();
	String text;
	text += "Attachment " + name + " (" + mime + ", " + itos(p_attachment.size_bytes) + " bytes):\n";
	text += "```text\n";
	text += body;
	if (!body.ends_with("\n")) {
		text += "\n";
	}
	text += "```";
	if (r_truncated) {
		text += "\n[attachment text truncated]";
	}

	r_text = text;
	r_error = AIError::none();
	return true;
}

Dictionary AIFilePreprocessor::bytes_to_text_context(const Dictionary &p_attachment, const PackedByteArray &p_bytes) const {
	AIFileAttachment attachment = AIFileAttachment::from_dictionary(p_attachment);
	String text;
	bool truncated = false;
	AIError error;
	if (!bytes_to_text_context_struct(attachment, p_bytes, text, truncated, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result;
	result["success"] = true;
	result["text"] = text;
	result["truncated"] = truncated;
	return result;
}

bool AIImageNormalizer::_decode_image(const String &p_mime, const PackedByteArray &p_bytes, Ref<Image> &r_image, AIError &r_error) {
	const String mime = p_mime.strip_edges().to_lower();
	if (mime != "image/png" && mime != "image/jpeg" && mime != "image/jpg" && mime != "image/webp") {
		r_image.unref();
		r_error = AIError::none();
		return true;
	}

	Ref<Image> image = memnew(Image);
	Error decode_error = ERR_FILE_UNRECOGNIZED;
	if (mime == "image/png") {
		decode_error = image->load_png_from_buffer(p_bytes);
	} else if (mime == "image/jpeg" || mime == "image/jpg") {
		decode_error = image->load_jpg_from_buffer(p_bytes);
	} else if (mime == "image/webp") {
		decode_error = image->load_webp_from_buffer(p_bytes);
	}

	if (decode_error != OK || image->is_empty()) {
		Dictionary details;
		details["mime"] = p_mime;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment image could not be decoded.", details);
		return false;
	}

	r_image = image;
	r_error = AIError::none();
	return true;
}

void AIImageNormalizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_blob_store", "blob_store"), &AIImageNormalizer::set_blob_store);
	ClassDB::bind_method(D_METHOD("get_blob_store"), &AIImageNormalizer::get_blob_store);
	ClassDB::bind_method(D_METHOD("set_max_width", "max_width"), &AIImageNormalizer::set_max_width);
	ClassDB::bind_method(D_METHOD("get_max_width"), &AIImageNormalizer::get_max_width);
	ClassDB::bind_method(D_METHOD("set_max_height", "max_height"), &AIImageNormalizer::set_max_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &AIImageNormalizer::get_max_height);
	ClassDB::bind_method(D_METHOD("set_max_output_bytes", "max_output_bytes"), &AIImageNormalizer::set_max_output_bytes);
	ClassDB::bind_method(D_METHOD("get_max_output_bytes"), &AIImageNormalizer::get_max_output_bytes);
	ClassDB::bind_method(D_METHOD("normalize", "attachment"), &AIImageNormalizer::normalize);
}

AIImageNormalizer::AIImageNormalizer() {
	blob_store.instantiate();
}

void AIImageNormalizer::set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store) {
	blob_store = p_blob_store;
	if (blob_store.is_null()) {
		blob_store.instantiate();
	}
}

Ref<AIAttachmentBlobStore> AIImageNormalizer::get_blob_store() const {
	return blob_store;
}

void AIImageNormalizer::set_max_width(int p_max_width) {
	max_width = MAX(1, p_max_width);
}

int AIImageNormalizer::get_max_width() const {
	return max_width;
}

void AIImageNormalizer::set_max_height(int p_max_height) {
	max_height = MAX(1, p_max_height);
}

int AIImageNormalizer::get_max_height() const {
	return max_height;
}

void AIImageNormalizer::set_max_output_bytes(int64_t p_max_output_bytes) {
	max_output_bytes = p_max_output_bytes < 1024 ? 1024 : p_max_output_bytes;
}

int64_t AIImageNormalizer::get_max_output_bytes() const {
	return max_output_bytes;
}

bool AIImageNormalizer::normalize_struct(const AIFileAttachment &p_attachment, AIFileAttachment &r_attachment, AIError &r_error) {
	r_attachment = p_attachment;
	if (AIAttachmentResolver::mime_to_modality_static(p_attachment.mime) != "image") {
		r_error = AIError::none();
		return true;
	}
	if (blob_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ImageNormalizer has no AttachmentBlobStore.");
		return false;
	}

	const String blob_ref = String(p_attachment.metadata.get("blob_id", p_attachment.path)).strip_edges();
	PackedByteArray bytes;
	if (!blob_store->read_bytes_struct(blob_ref, bytes, r_error)) {
		return false;
	}

	Ref<Image> image;
	if (!_decode_image(p_attachment.mime, bytes, image, r_error)) {
		return false;
	}
	if (image.is_null()) {
		r_error = AIError::none();
		return true;
	}

	r_attachment.metadata["image_width"] = image->get_width();
	r_attachment.metadata["image_height"] = image->get_height();
	if (image->get_width() <= max_width && image->get_height() <= max_height && bytes.size() <= max_output_bytes) {
		r_error = AIError::none();
		return true;
	}

	const double scale_x = double(max_width) / double(MAX(1, image->get_width()));
	const double scale_y = double(max_height) / double(MAX(1, image->get_height()));
	const double scale = MIN(1.0, MIN(scale_x, scale_y));
	const int target_width = MAX(1, int(double(image->get_width()) * scale));
	const int target_height = MAX(1, int(double(image->get_height()) * scale));
	if (target_width != image->get_width() || target_height != image->get_height()) {
		image->resize(target_width, target_height, Image::INTERPOLATE_LANCZOS);
	}

	PackedByteArray normalized_bytes = image->save_png_to_buffer();
	if (normalized_bytes.is_empty()) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to encode normalized attachment image.");
		return false;
	}
	if (normalized_bytes.size() > max_output_bytes) {
		Dictionary details;
		details["size_bytes"] = normalized_bytes.size();
		details["max_bytes"] = max_output_bytes;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Normalized attachment image exceeds the configured byte limit.", details);
		return false;
	}

	Dictionary source_metadata;
	source_metadata["source_type"] = "normalized_image";
	source_metadata["source_blob_id"] = blob_ref;
	source_metadata["name"] = p_attachment.name;
	source_metadata["original_mime"] = p_attachment.mime;
	source_metadata["width"] = target_width;
	source_metadata["height"] = target_height;

	AIAttachmentBlobRecord record;
	if (!blob_store->put_bytes_struct(normalized_bytes, "image/png", source_metadata, record, r_error)) {
		return false;
	}

	r_attachment.path = record.id;
	r_attachment.mime = "image/png";
	r_attachment.size_bytes = int64_t(record.size);
	r_attachment.metadata["blob_id"] = record.id;
	r_attachment.metadata["blob_hash"] = record.hash;
	r_attachment.metadata["normalized"] = true;
	r_attachment.metadata["normalized_from_blob_id"] = blob_ref;
	r_attachment.metadata["image_width"] = target_width;
	r_attachment.metadata["image_height"] = target_height;
	r_error = AIError::none();
	return true;
}

Dictionary AIImageNormalizer::normalize(const Dictionary &p_attachment) {
	AIFileAttachment attachment = AIFileAttachment::from_dictionary(p_attachment);
	AIFileAttachment normalized;
	AIError error;
	if (!normalize_struct(attachment, normalized, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result = normalized.to_dictionary();
	result["success"] = true;
	return result;
}

Dictionary AIModelPartBuilder::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Array AIModelPartBuilder::_array_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::ARRAY) {
		return Array(p_value).duplicate(true);
	}
	return Array();
}

bool AIModelPartBuilder::_array_contains_string(const Array &p_array, const String &p_value) {
	const String value = p_value.strip_edges().to_lower();
	for (int i = 0; i < p_array.size(); i++) {
		const String item = String(p_array[i]).strip_edges().to_lower();
		if (item == value || item == "*" || (item == "multimodal" && value == "image")) {
			return true;
		}
	}
	return false;
}

Array AIModelPartBuilder::_provider_input_modalities(const Dictionary &p_provider_config) {
	Array input = _array_from_variant(p_provider_config.get("input_modalities", p_provider_config.get("inputModalities", Variant())));
	if (!input.is_empty()) {
		return input;
	}

	const Dictionary modalities = _dictionary_from_variant(p_provider_config.get("modalities", Dictionary()));
	input = _array_from_variant(modalities.get("input", modalities.get("inputs", Variant())));
	if (!input.is_empty()) {
		return input;
	}

	const Dictionary capabilities = _dictionary_from_variant(p_provider_config.get("capabilities", Dictionary()));
	const Dictionary capability_modalities = _dictionary_from_variant(capabilities.get("modalities", Dictionary()));
	input = _array_from_variant(capability_modalities.get("input", capability_modalities.get("inputs", Variant())));
	if (!input.is_empty()) {
		return input;
	}
	input = _array_from_variant(capabilities.get("input_modalities", capabilities.get("inputModalities", Variant())));
	return input;
}

AIModelPartType AIModelPartBuilder::_part_type_for_modality(const String &p_modality) {
	const String modality = p_modality.strip_edges().to_lower();
	if (modality == "image") {
		return AI_MODEL_PART_IMAGE;
	}
	if (modality == "audio") {
		return AI_MODEL_PART_AUDIO;
	}
	if (modality == "file") {
		return AI_MODEL_PART_FILE;
	}
	return AI_MODEL_PART_TEXT;
}

String AIModelPartBuilder::_blob_ref_from_attachment(const AIFileAttachment &p_attachment) {
	const String metadata_blob = String(p_attachment.metadata.get("blob_id", p_attachment.metadata.get("blobID", String()))).strip_edges();
	if (!metadata_blob.is_empty()) {
		return metadata_blob;
	}
	if (p_attachment.path.strip_edges().begins_with("blob_")) {
		return p_attachment.path.strip_edges();
	}
	return p_attachment.id.strip_edges();
}

String AIModelPartBuilder::_bytes_to_data_url(const String &p_mime, const PackedByteArray &p_bytes, AIError &r_error) {
	CoreBind::Marshalls *marshalls = CoreBind::Marshalls::get_singleton();
	if (!marshalls) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Marshalls singleton is not available for base64 encoding.");
		return String();
	}

	r_error = AIError::none();
	const String mime = p_mime.strip_edges().is_empty() ? String("application/octet-stream") : p_mime.strip_edges().to_lower();
	return "data:" + mime + ";base64," + marshalls->raw_to_base64(p_bytes);
}

Dictionary AIModelPartBuilder::_safe_part_metadata(const AIFileAttachment &p_attachment, const String &p_modality, const String &p_blob_ref) {
	Dictionary metadata;
	metadata["attachment_id"] = p_attachment.id;
	metadata["blob_id"] = p_blob_ref;
	metadata["name"] = p_attachment.name;
	metadata["mime"] = p_attachment.mime;
	metadata["size_bytes"] = p_attachment.size_bytes;
	metadata["modality"] = p_modality;
	if (p_attachment.metadata.has("blob_hash")) {
		metadata["blob_hash"] = p_attachment.metadata["blob_hash"];
	}
	if (p_attachment.metadata.has("image_width")) {
		metadata["image_width"] = p_attachment.metadata["image_width"];
	}
	if (p_attachment.metadata.has("image_height")) {
		metadata["image_height"] = p_attachment.metadata["image_height"];
	}
	if (p_attachment.metadata.has("normalized")) {
		metadata["normalized"] = p_attachment.metadata["normalized"];
	}
	return metadata;
}

void AIModelPartBuilder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_blob_store", "blob_store"), &AIModelPartBuilder::set_blob_store);
	ClassDB::bind_method(D_METHOD("get_blob_store"), &AIModelPartBuilder::get_blob_store);
	ClassDB::bind_method(D_METHOD("set_file_preprocessor", "file_preprocessor"), &AIModelPartBuilder::set_file_preprocessor);
	ClassDB::bind_method(D_METHOD("get_file_preprocessor"), &AIModelPartBuilder::get_file_preprocessor);
	ClassDB::bind_method(D_METHOD("set_image_normalizer", "image_normalizer"), &AIModelPartBuilder::set_image_normalizer);
	ClassDB::bind_method(D_METHOD("get_image_normalizer"), &AIModelPartBuilder::get_image_normalizer);
	ClassDB::bind_method(D_METHOD("set_max_inline_bytes", "max_inline_bytes"), &AIModelPartBuilder::set_max_inline_bytes);
	ClassDB::bind_method(D_METHOD("get_max_inline_bytes"), &AIModelPartBuilder::get_max_inline_bytes);
	ClassDB::bind_method(D_METHOD("provider_supports_modality", "provider_config", "modality"), &AIModelPartBuilder::provider_supports_modality);
	ClassDB::bind_method(D_METHOD("build_attachment_part", "attachment", "provider_config"), &AIModelPartBuilder::build_attachment_part);
}

AIModelPartBuilder::AIModelPartBuilder() {
	blob_store.instantiate();
	file_preprocessor.instantiate();
	image_normalizer.instantiate();
	image_normalizer->set_blob_store(blob_store);
}

bool AIModelPartBuilder::provider_supports_modality_static(const Dictionary &p_provider_config, const String &p_modality) {
	const String modality = p_modality.strip_edges().to_lower();
	if (modality == "text" || modality.is_empty()) {
		return true;
	}

	const Array input = _provider_input_modalities(p_provider_config);
	if (!input.is_empty()) {
		return _array_contains_string(input, modality);
	}

	const Dictionary capabilities = _dictionary_from_variant(p_provider_config.get("capabilities", Dictionary()));
	if (modality == "image") {
		return bool(p_provider_config.get("supports_multimodal", false)) || bool(p_provider_config.get("supports_images", false)) || bool(p_provider_config.get("supports_image", false)) || bool(capabilities.get("image", capabilities.get("images", capabilities.get("vision", false))));
	}
	if (modality == "audio") {
		return bool(p_provider_config.get("supports_audio", false)) || bool(capabilities.get("audio", false));
	}
	if (modality == "file") {
		return bool(p_provider_config.get("supports_files", false)) || bool(p_provider_config.get("supports_file", false)) || bool(p_provider_config.get("supports_pdf", false)) || bool(capabilities.get("file", capabilities.get("files", capabilities.get("pdf", false))));
	}
	return false;
}

void AIModelPartBuilder::set_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store) {
	blob_store = p_blob_store;
	if (blob_store.is_null()) {
		blob_store.instantiate();
	}
	if (image_normalizer.is_valid()) {
		image_normalizer->set_blob_store(blob_store);
	}
}

Ref<AIAttachmentBlobStore> AIModelPartBuilder::get_blob_store() const {
	return blob_store;
}

void AIModelPartBuilder::set_file_preprocessor(const Ref<AIFilePreprocessor> &p_file_preprocessor) {
	file_preprocessor = p_file_preprocessor;
	if (file_preprocessor.is_null()) {
		file_preprocessor.instantiate();
	}
}

Ref<AIFilePreprocessor> AIModelPartBuilder::get_file_preprocessor() const {
	return file_preprocessor;
}

void AIModelPartBuilder::set_image_normalizer(const Ref<AIImageNormalizer> &p_image_normalizer) {
	image_normalizer = p_image_normalizer;
	if (image_normalizer.is_null()) {
		image_normalizer.instantiate();
	}
	image_normalizer->set_blob_store(blob_store);
}

Ref<AIImageNormalizer> AIModelPartBuilder::get_image_normalizer() const {
	return image_normalizer;
}

void AIModelPartBuilder::set_max_inline_bytes(int64_t p_max_inline_bytes) {
	max_inline_bytes = p_max_inline_bytes < 1024 ? 1024 : p_max_inline_bytes;
}

int64_t AIModelPartBuilder::get_max_inline_bytes() const {
	return max_inline_bytes;
}

bool AIModelPartBuilder::_ensure_dependencies(AIError &r_error) {
	if (blob_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ModelPartBuilder has no AttachmentBlobStore.");
		return false;
	}
	if (file_preprocessor.is_null()) {
		file_preprocessor.instantiate();
	}
	if (image_normalizer.is_null()) {
		image_normalizer.instantiate();
		image_normalizer->set_blob_store(blob_store);
	}
	r_error = AIError::none();
	return true;
}

bool AIModelPartBuilder::_read_attachment_bytes(const AIFileAttachment &p_attachment, PackedByteArray &r_bytes, String &r_blob_ref, AIError &r_error) const {
	if (blob_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ModelPartBuilder has no AttachmentBlobStore.");
		return false;
	}
	r_blob_ref = _blob_ref_from_attachment(p_attachment);
	if (r_blob_ref.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment has no blob reference.");
		return false;
	}
	return blob_store->read_bytes_struct(r_blob_ref, r_bytes, r_error);
}

bool AIModelPartBuilder::_build_text_part(const AIFileAttachment &p_attachment, AIModelPart &r_part, AIError &r_error) {
	PackedByteArray bytes;
	String blob_ref;
	if (!_read_attachment_bytes(p_attachment, bytes, blob_ref, r_error)) {
		return false;
	}

	String text;
	bool truncated = false;
	if (!file_preprocessor->bytes_to_text_context_struct(p_attachment, bytes, text, truncated, r_error)) {
		return false;
	}

	r_part = AIModelPart::text_part(text);
	r_part.metadata = _safe_part_metadata(p_attachment, "text", blob_ref);
	r_part.metadata["derived_from_attachment"] = true;
	r_part.metadata["truncated"] = truncated;
	r_error = AIError::none();
	return true;
}

bool AIModelPartBuilder::_build_unsupported_modality_notice_part(const AIFileAttachment &p_attachment, const String &p_modality, AIModelPart &r_part, AIError &r_error) const {
	const String modality = p_modality.strip_edges().to_lower();
	const String display_modality = modality.is_empty() ? String("file") : modality;
	const String name = p_attachment.name.strip_edges().is_empty() ? (p_attachment.id.strip_edges().is_empty() ? String("attachment") : p_attachment.id.strip_edges()) : p_attachment.name.strip_edges();
	const String mime = p_attachment.mime.strip_edges().is_empty() ? String("application/octet-stream") : p_attachment.mime.strip_edges();
	const String blob_ref = _blob_ref_from_attachment(p_attachment);

	String text;
	text += "Attachment " + name + " (" + mime + ", " + itos(p_attachment.size_bytes) + " bytes) was uploaded as " + display_modality + " input, but the selected model does not support " + display_modality + " input.\n";
	text += "The attachment binary content was omitted from this model request. Use the user's text prompt and this attachment notice only.";

	r_part = AIModelPart::text_part(text);
	r_part.metadata = _safe_part_metadata(p_attachment, display_modality, blob_ref);
	r_part.metadata["derived_from_attachment"] = true;
	r_part.metadata["modality_omitted"] = true;
	r_part.metadata["omitted_reason"] = "unsupported_model_modality";
	r_error = AIError::none();
	return true;
}

bool AIModelPartBuilder::_build_binary_part(const AIFileAttachment &p_attachment, const Dictionary &p_provider_config, const String &p_modality, AIModelPart &r_part, AIError &r_error) {
	if (!provider_supports_modality_static(p_provider_config, p_modality)) {
		return _build_unsupported_modality_notice_part(p_attachment, p_modality, r_part, r_error);
	}

	AIFileAttachment attachment = p_attachment;
	if (p_modality == "image" && image_normalizer.is_valid()) {
		if (!image_normalizer->normalize_struct(p_attachment, attachment, r_error)) {
			return false;
		}
	}

	PackedByteArray bytes;
	String blob_ref;
	if (!_read_attachment_bytes(attachment, bytes, blob_ref, r_error)) {
		return false;
	}
	if (bytes.size() > max_inline_bytes) {
		Dictionary details;
		details["size_bytes"] = bytes.size();
		details["max_bytes"] = max_inline_bytes;
		details["attachment_id"] = attachment.id;
		r_error = AIError::make(AI_ERROR_VALIDATION, "Attachment exceeds provider inline byte limit.", details);
		return false;
	}

	const String data_url = _bytes_to_data_url(attachment.mime, bytes, r_error);
	if (data_url.is_empty()) {
		return false;
	}

	r_part = AIModelPart::data_part(_part_type_for_modality(p_modality), attachment.mime, data_url, attachment.name);
	r_part.metadata = _safe_part_metadata(attachment, p_modality, blob_ref);
	r_error = AIError::none();
	return true;
}

bool AIModelPartBuilder::build_attachment_part_struct(const AIFileAttachment &p_attachment, const Dictionary &p_provider_config, AIModelPart &r_part, AIError &r_error) {
	if (!_ensure_dependencies(r_error)) {
		return false;
	}

	const String modality = AIAttachmentResolver::mime_to_modality_static(p_attachment.mime);
	if (modality == "text") {
		return _build_text_part(p_attachment, r_part, r_error);
	}
	return _build_binary_part(p_attachment, p_provider_config, modality, r_part, r_error);
}

bool AIModelPartBuilder::append_attachment_parts_struct(const Vector<AIFileAttachment> &p_attachments, const Dictionary &p_provider_config, Vector<AIModelPart> &r_parts, AIError &r_error) {
	for (int i = 0; i < p_attachments.size(); i++) {
		AIModelPart part;
		if (!build_attachment_part_struct(p_attachments[i], p_provider_config, part, r_error)) {
			return false;
		}
		r_parts.push_back(part);
	}
	r_error = AIError::none();
	return true;
}

bool AIModelPartBuilder::provider_supports_modality(const Dictionary &p_provider_config, const String &p_modality) const {
	return provider_supports_modality_static(p_provider_config, p_modality);
}

Dictionary AIModelPartBuilder::build_attachment_part(const Dictionary &p_attachment, const Dictionary &p_provider_config) {
	AIFileAttachment attachment = AIFileAttachment::from_dictionary(p_attachment);
	AIModelPart part;
	AIError error;
	if (!build_attachment_part_struct(attachment, p_provider_config, part, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Dictionary result = part.to_dictionary();
	result["success"] = true;
	return result;
}
