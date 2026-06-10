/**************************************************************************/
/*  ai_sse_parser.h                                                       */
/**************************************************************************/

#pragma once

#include "core/templates/vector.h"
#include "core/variant/variant.h"

struct AISSEEvent {
	String event;
	String data;
	String id;
	int retry_msec = -1;

	Dictionary to_dictionary() const;
};

class AISSEParser {
	PackedByteArray buffer;

	static int _find_event_separator(const PackedByteArray &p_buffer, int &r_separator_size);
	static bool _parse_event_block(const String &p_block, AISSEEvent &r_event);

public:
	void clear();
	bool push_chunk(const PackedByteArray &p_chunk, Vector<AISSEEvent> &r_events);
	bool finish(Vector<AISSEEvent> &r_events);

	static bool extract_events_from_text(const String &p_text, bool p_require_complete_events, Vector<AISSEEvent> &r_events);
};
