/**************************************************************************/
/*  ai_fake_llm_runtime.cpp                                               */
/**************************************************************************/

#include "ai_fake_llm_runtime.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/object/class_db.h"

void AIFakeLLMRuntime::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_response_text", "text"), &AIFakeLLMRuntime::set_response_text);
	ClassDB::bind_method(D_METHOD("get_response_text"), &AIFakeLLMRuntime::get_response_text);
	ClassDB::bind_method(D_METHOD("set_tool_call", "name", "input"), &AIFakeLLMRuntime::set_tool_call, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("clear_tool_call"), &AIFakeLLMRuntime::clear_tool_call);
	ClassDB::bind_method(D_METHOD("get_tool_name"), &AIFakeLLMRuntime::get_tool_name);
	ClassDB::bind_method(D_METHOD("set_fail_next", "fail"), &AIFakeLLMRuntime::set_fail_next);
	ClassDB::bind_method(D_METHOD("get_fail_next"), &AIFakeLLMRuntime::get_fail_next);
	ClassDB::bind_method(D_METHOD("get_stream_call_count"), &AIFakeLLMRuntime::get_stream_call_count);
	ClassDB::bind_method(D_METHOD("reset"), &AIFakeLLMRuntime::reset);
}

String AIFakeLLMRuntime::_last_user_text(const AIModelRequest &p_request) {
	for (int i = p_request.messages.size() - 1; i >= 0; i--) {
		const AIModelMessage &message = p_request.messages[i];
		if (message.role != "user") {
			continue;
		}
		for (int j = 0; j < message.parts.size(); j++) {
			if (message.parts[j].type == AI_MODEL_PART_TEXT && !message.parts[j].text.is_empty()) {
				return message.parts[j].text;
			}
		}
	}
	return String();
}

String AIFakeLLMRuntime::get_runtime_type() const {
	return "fake";
}

bool AIFakeLLMRuntime::configure(const Dictionary &p_config, AIError &r_error) {
	response_text = p_config.get("response_text", response_text);
	tool_name = p_config.get("tool_name", tool_name);
	if (p_config.has("tool_input")) {
		tool_input = p_config["tool_input"];
	}
	fail_next = bool(p_config.get("fail_next", fail_next));
	r_error = AIError::none();
	return true;
}

bool AIFakeLLMRuntime::stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) {
	stream_call_count++;
	if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
		r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("Fake runtime interrupted."));
		return false;
	}
	if (fail_next) {
		fail_next = false;
		r_error = AIError::make(AI_ERROR_PROVIDER, "Fake provider failure.");
		return false;
	}
	if (p_sink.is_null()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Fake runtime requires a stream sink.");
		return false;
	}

	bool stop_requested = false;
	String sink_error;
	AIStreamEvent start = AIStreamEvent::step_start();
	start.id = p_request.request_id;
	if (!p_sink->push_event(start, stop_requested, sink_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
		return false;
	}
	if (stop_requested) {
		r_error = AIError::make(AI_ERROR_CANCELLED, "Stream sink stopped fake runtime.");
		return false;
	}

	const String text = response_text.is_empty() ? String("Fake response: ") + _last_user_text(p_request) : response_text;
	AIStreamEvent delta = AIStreamEvent::text_delta(AIId::make("text"), text);
	if (!p_sink->push_event(delta, stop_requested, sink_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
		return false;
	}
	if (stop_requested) {
		r_error = AIError::make(AI_ERROR_CANCELLED, "Stream sink stopped fake runtime.");
		return false;
	}

	if (!tool_name.is_empty()) {
		AIStreamEvent tool_call = AIStreamEvent::tool_call(AIId::make("call"), tool_name, tool_input);
		tool_call.provider_metadata["provider"] = "fake";
		if (!p_sink->push_event(tool_call, stop_requested, sink_error)) {
			r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
			return false;
		}
		if (stop_requested) {
			r_error = AIError::make(AI_ERROR_CANCELLED, "Stream sink stopped fake runtime.");
			return false;
		}
	}

	AIStreamEvent end;
	end.type = AI_STREAM_EVENT_STEP_END;
	end.id = p_request.request_id;
	if (!p_sink->push_event(end, stop_requested, sink_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, sink_error);
		return false;
	}

	r_error = AIError::none();
	return !stop_requested;
}

void AIFakeLLMRuntime::set_response_text(const String &p_text) {
	response_text = p_text;
}

String AIFakeLLMRuntime::get_response_text() const {
	return response_text;
}

void AIFakeLLMRuntime::set_tool_call(const String &p_name, const Variant &p_input) {
	tool_name = p_name.strip_edges();
	tool_input = p_input;
}

void AIFakeLLMRuntime::clear_tool_call() {
	tool_name = String();
	tool_input = Variant();
}

String AIFakeLLMRuntime::get_tool_name() const {
	return tool_name;
}

void AIFakeLLMRuntime::set_fail_next(bool p_fail) {
	fail_next = p_fail;
}

bool AIFakeLLMRuntime::get_fail_next() const {
	return fail_next;
}

int64_t AIFakeLLMRuntime::get_stream_call_count() const {
	return stream_call_count;
}

void AIFakeLLMRuntime::reset() {
	response_text = String();
	clear_tool_call();
	fail_next = false;
	stream_call_count = 0;
}
