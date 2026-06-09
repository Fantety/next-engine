/**************************************************************************/
/*  ai_multimodal_message_adapter.h                                       */
/**************************************************************************/

#pragma once

#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#include "editor/ai_component/providers/ai_provider_config.h"

class AIMultimodalMessageAdapter {
	static String _get_message_text(const Dictionary &p_message);
	static Array _get_message_attachments(const Dictionary &p_message);
	static String _get_image_mime_type(const Dictionary &p_attachment);
	static String _get_image_data_url(const Dictionary &p_attachment, const AIProviderConfig &p_config, String &r_error);
	static String _build_text_attachment_context(const Array &p_attachments, const AIProviderConfig &p_config);
	static String _build_text_only_attachment_note(const Array &p_attachments);
	static Variant _build_chat_completions_user_content(const Dictionary &p_message, const AIProviderConfig &p_config);

public:
	static Variant build_chat_message_content(const Dictionary &p_message, const AIProviderConfig &p_config);
	static void append_tool_attachment_user_message(Array &r_chat_messages, const Dictionary &p_tool_message, const AIProviderConfig &p_config);
	static Array build_chat_messages(const Array &p_messages, const AIProviderConfig &p_config);
};
