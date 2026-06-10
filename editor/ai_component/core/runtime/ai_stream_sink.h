/**************************************************************************/
/*  ai_stream_sink.h                                                      */
/**************************************************************************/

#pragma once

#include "editor/ai_component/core/runtime/ai_stream_event.h"

#include "core/object/ref_counted.h"
#include "core/variant/callable.h"

class AIStreamSink : public RefCounted {
	GDCLASS(AIStreamSink, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual bool push_event(const AIStreamEvent &p_event, bool &r_stop_requested, String &r_error);
};

class AICallableStreamSink : public AIStreamSink {
	GDCLASS(AICallableStreamSink, AIStreamSink);

	Callable callback;

protected:
	static void _bind_methods();

public:
	void set_callback(const Callable &p_callback);
	Callable get_callback() const;

	virtual bool push_event(const AIStreamEvent &p_event, bool &r_stop_requested, String &r_error) override;
};
