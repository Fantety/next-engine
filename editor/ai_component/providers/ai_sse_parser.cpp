/**************************************************************************/
/*  ai_sse_parser.cpp                                                      */
/**************************************************************************/

#include "ai_sse_parser.h"

#include "core/object/class_db.h"

void AISSEParser::_bind_methods() {
	ClassDB::bind_method(D_METHOD("push_chunk", "chunk"), &AISSEParser::push_chunk);
	ClassDB::bind_method(D_METHOD("reset"), &AISSEParser::reset);
}

Array AISSEParser::push_chunk(const String &p_chunk) {
	Array events;
	pending_data += p_chunk;

	while (true) {
		int separator = pending_data.find("\n\n");
		int separator_length = 2;
		if (separator == -1) {
			separator = pending_data.find("\r\n\r\n");
			separator_length = 4;
		}
		if (separator == -1) {
			break;
		}

		String raw_event = pending_data.substr(0, separator);
		pending_data = pending_data.substr(separator + separator_length);

		PackedStringArray lines = raw_event.split("\n", false);
		String data;
		for (int i = 0; i < lines.size(); i++) {
			String line = lines[i].strip_edges();
			if (line.begins_with("data:")) {
				if (!data.is_empty()) {
					data += "\n";
				}
				data += line.substr(5).strip_edges();
			}
		}

		if (!data.is_empty()) {
			events.push_back(data);
		}
	}

	return events;
}

void AISSEParser::reset() {
	pending_data.clear();
}
