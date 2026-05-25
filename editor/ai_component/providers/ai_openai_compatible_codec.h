/**************************************************************************/
/*  ai_openai_compatible_codec.h                                          */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"

struct AIOpenAIStreamToolCallState {
	String id;
	String tool_name;
	String arguments_json;
};

struct AIOpenAIStreamParseResult {
	bool has_delta = false;
	bool done = false;
	String error;
	AIAgentRuntimeResponse response;
};

class AIOpenAICompatibleStreamAccumulator {
	String content;
	String reasoning_content;
	String finish_reason;
	Dictionary metadata;
	Vector<AIOpenAIStreamToolCallState> tool_calls;

	void _ensure_tool_call_index(int p_index);

public:
	bool apply_event(const String &p_event, AIOpenAIStreamParseResult &r_result);
	AIAgentRuntimeResponse get_response(String &r_error) const;
};

class AIOpenAICompatibleCodec {
	static PackedByteArray _build_body(const Array &p_messages, const String &p_model, const Array &p_tool_schemas = Array(), bool p_stream = false, int p_max_output_tokens = 0);
	static bool _parse_chat_completion(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error);
	static bool _extract_delta(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error);

public:
	static String build_request_path(const String &p_base_path);
	static PackedByteArray build_body(const Array &p_messages, const String &p_model, const Array &p_tool_schemas = Array(), bool p_stream = false, int p_max_output_tokens = 0);
	static bool parse_chat_completion(const String &p_response_text, AIAgentRuntimeResponse &r_response, String &r_error);
	static bool extract_delta(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error);
};
