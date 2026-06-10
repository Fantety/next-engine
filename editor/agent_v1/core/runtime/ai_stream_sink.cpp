/**************************************************************************/
/*  ai_stream_sink.cpp                                                    */
/**************************************************************************/

#include "ai_stream_sink.h"

#include "core/variant/variant.h"

void AIStreamSink::_bind_methods() {
}

bool AIStreamSink::push_event(const AIStreamEvent &p_event, bool &r_stop_requested, String &r_error) {
	(void)p_event;
	r_stop_requested = false;
	r_error = String();
	return true;
}

void AICallableStreamSink::_bind_methods() {
}

void AICallableStreamSink::set_callback(const Callable &p_callback) {
	callback = p_callback;
}

Callable AICallableStreamSink::get_callback() const {
	return callback;
}

bool AICallableStreamSink::push_event(const AIStreamEvent &p_event, bool &r_stop_requested, String &r_error) {
	r_stop_requested = false;
	r_error = String();
	if (!callback.is_valid()) {
		return true;
	}

	Dictionary event_dict = p_event.to_dictionary();
	Variant event_variant = event_dict;
	const Variant *argptrs[1] = { &event_variant };
	Variant ret;
	Callable::CallError ce;
	callback.callp(argptrs, 1, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		r_error = Variant::get_callable_error_text(callback, argptrs, 1, ce);
		return false;
	}

	if (ret.get_type() == Variant::BOOL && bool(ret)) {
		r_stop_requested = true;
	}
	return true;
}
