/**************************************************************************/
/*  ai_openai_compatible_codec.h                                          */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/variant.h"
#include "core/variant/array.h"

struct AIAgentRuntimeResponse;

class AIOpenAICompatibleCodec {
	static PackedByteArray _build_body(const Array &p_messages, const String &p_model, const Array &p_tool_schemas = Array(), bool p_stream = false);
	static bool _parse_chat_completion(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error);
	static bool _extract_delta(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error);

public:
	static String build_request_path(const String &p_base_path);
	static PackedByteArray build_body(const Array &p_messages, const String &p_model, const Array &p_tool_schemas = Array(), bool p_stream = false);
	static bool parse_chat_completion(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error);
	static bool extract_delta(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error);
};
