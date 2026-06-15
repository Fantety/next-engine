/**************************************************************************/
/*  ai_stream_event.h                                                     */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"

#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

enum AIStreamEventType {
	AI_STREAM_EVENT_STEP_START,
	AI_STREAM_EVENT_STEP_END,
	AI_STREAM_EVENT_TEXT_START,
	AI_STREAM_EVENT_TEXT_DELTA,
	AI_STREAM_EVENT_TEXT_END,
	AI_STREAM_EVENT_REASONING_START,
	AI_STREAM_EVENT_REASONING_DELTA,
	AI_STREAM_EVENT_REASONING_END,
	AI_STREAM_EVENT_TOOL_INPUT_START,
	AI_STREAM_EVENT_TOOL_INPUT_DELTA,
	AI_STREAM_EVENT_TOOL_INPUT_END,
	AI_STREAM_EVENT_TOOL_CALL,
	AI_STREAM_EVENT_TOOL_RESULT,
	AI_STREAM_EVENT_TOOL_ERROR,
	AI_STREAM_EVENT_USAGE,
	AI_STREAM_EVENT_PROVIDER_ERROR,
};

struct AIStreamEvent {
	AIStreamEventType type = AI_STREAM_EVENT_STEP_START;
	String id;
	String name;
	String text;
	Variant input;
	Variant result;
	AIError error;
	bool provider_executed = false;
	Dictionary usage;
	Dictionary provider_metadata;
	Dictionary metadata;

	bool is_error() const;
	Dictionary to_dictionary() const;

	static String type_to_string(AIStreamEventType p_type);
	static AIStreamEvent step_start();
	static AIStreamEvent text_delta(const String &p_id, const String &p_text);
	static AIStreamEvent tool_call(const String &p_id, const String &p_name, const Variant &p_input, bool p_provider_executed = false);
	static AIStreamEvent usage_event(const Dictionary &p_usage);
	static AIStreamEvent provider_error(const AIError &p_error);
};
