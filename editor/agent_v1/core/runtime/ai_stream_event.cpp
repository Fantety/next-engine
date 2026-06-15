/**************************************************************************/
/*  ai_stream_event.cpp                                                   */
/**************************************************************************/

#include "ai_stream_event.h"

#include "core/variant/variant.h"

bool AIStreamEvent::is_error() const {
	return type == AI_STREAM_EVENT_TOOL_ERROR || type == AI_STREAM_EVENT_PROVIDER_ERROR || error.is_error();
}

Dictionary AIStreamEvent::to_dictionary() const {
	Dictionary event_dict;
	event_dict["type"] = type_to_string(type);
	event_dict["id"] = id;
	event_dict["name"] = name;
	event_dict["text"] = text;
	event_dict["input"] = input;
	event_dict["result"] = result;
	event_dict["error"] = error.to_dictionary();
	event_dict["provider_executed"] = provider_executed;
	event_dict["usage"] = usage;
	event_dict["provider_metadata"] = provider_metadata;
	event_dict["metadata"] = metadata;
	return event_dict;
}

String AIStreamEvent::type_to_string(AIStreamEventType p_type) {
	switch (p_type) {
		case AI_STREAM_EVENT_STEP_START:
			return "step-start";
		case AI_STREAM_EVENT_STEP_END:
			return "step-end";
		case AI_STREAM_EVENT_TEXT_START:
			return "text-start";
		case AI_STREAM_EVENT_TEXT_DELTA:
			return "text-delta";
		case AI_STREAM_EVENT_TEXT_END:
			return "text-end";
		case AI_STREAM_EVENT_REASONING_START:
			return "reasoning-start";
		case AI_STREAM_EVENT_REASONING_DELTA:
			return "reasoning-delta";
		case AI_STREAM_EVENT_REASONING_END:
			return "reasoning-end";
		case AI_STREAM_EVENT_TOOL_INPUT_START:
			return "tool-input-start";
		case AI_STREAM_EVENT_TOOL_INPUT_DELTA:
			return "tool-input-delta";
		case AI_STREAM_EVENT_TOOL_INPUT_END:
			return "tool-input-end";
		case AI_STREAM_EVENT_TOOL_CALL:
			return "tool-call";
		case AI_STREAM_EVENT_TOOL_RESULT:
			return "tool-result";
		case AI_STREAM_EVENT_TOOL_ERROR:
			return "tool-error";
		case AI_STREAM_EVENT_USAGE:
			return "usage";
		case AI_STREAM_EVENT_PROVIDER_ERROR:
			return "provider-error";
	}
	return "provider-error";
}

AIStreamEvent AIStreamEvent::step_start() {
	AIStreamEvent event;
	event.type = AI_STREAM_EVENT_STEP_START;
	return event;
}

AIStreamEvent AIStreamEvent::text_delta(const String &p_id, const String &p_text) {
	AIStreamEvent event;
	event.type = AI_STREAM_EVENT_TEXT_DELTA;
	event.id = p_id;
	event.text = p_text;
	return event;
}

AIStreamEvent AIStreamEvent::tool_call(const String &p_id, const String &p_name, const Variant &p_input, bool p_provider_executed) {
	AIStreamEvent event;
	event.type = AI_STREAM_EVENT_TOOL_CALL;
	event.id = p_id;
	event.name = p_name;
	event.input = p_input;
	event.provider_executed = p_provider_executed;
	return event;
}

AIStreamEvent AIStreamEvent::usage_event(const Dictionary &p_usage) {
	AIStreamEvent event;
	event.type = AI_STREAM_EVENT_USAGE;
	event.usage = p_usage.duplicate(true);
	return event;
}

AIStreamEvent AIStreamEvent::provider_error(const AIError &p_error) {
	AIStreamEvent event;
	event.type = AI_STREAM_EVENT_PROVIDER_ERROR;
	event.error = p_error;
	return event;
}
