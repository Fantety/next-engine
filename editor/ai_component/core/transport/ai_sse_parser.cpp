/**************************************************************************/
/*  ai_sse_parser.cpp                                                     */
/**************************************************************************/

#include "ai_sse_parser.h"

Dictionary AISSEEvent::to_dictionary() const {
	Dictionary event_dict;
	event_dict["event"] = event;
	event_dict["data"] = data;
	event_dict["id"] = id;
	event_dict["retry_msec"] = retry_msec;
	return event_dict;
}

int AISSEParser::_find_event_separator(const PackedByteArray &p_buffer, int &r_separator_size) {
	for (int i = 0; i < p_buffer.size() - 1; i++) {
		if (i < p_buffer.size() - 3 && p_buffer[i] == '\r' && p_buffer[i + 1] == '\n' && p_buffer[i + 2] == '\r' && p_buffer[i + 3] == '\n') {
			r_separator_size = 4;
			return i;
		}
		if (p_buffer[i] == '\n' && p_buffer[i + 1] == '\n') {
			r_separator_size = 2;
			return i;
		}
	}

	r_separator_size = 0;
	return -1;
}

bool AISSEParser::_parse_event_block(const String &p_block, AISSEEvent &r_event) {
	r_event = AISSEEvent();

	const String normalized = p_block.replace("\r\n", "\n").replace("\r", "\n");
	const Vector<String> lines = normalized.split("\n", false);
	for (int i = 0; i < lines.size(); i++) {
		const String raw_line = lines[i];
		if (raw_line.is_empty() || raw_line.begins_with(":")) {
			continue;
		}

		String field;
		String value;
		const int separator = raw_line.find(":");
		if (separator >= 0) {
			field = raw_line.substr(0, separator).strip_edges();
			value = raw_line.substr(separator + 1);
			if (value.begins_with(" ")) {
				value = value.substr(1);
			}
			value = value.strip_edges();
		} else {
			field = raw_line.strip_edges();
		}

		if (field == "event") {
			r_event.event = value;
		} else if (field == "data") {
			if (!r_event.data.is_empty()) {
				r_event.data += "\n";
			}
			r_event.data += value;
		} else if (field == "id") {
			r_event.id = value;
		} else if (field == "retry" && value.is_valid_int()) {
			r_event.retry_msec = value.to_int();
		}
	}

	return !r_event.data.is_empty() || !r_event.event.is_empty() || !r_event.id.is_empty() || r_event.retry_msec >= 0;
}

void AISSEParser::clear() {
	buffer.clear();
}

bool AISSEParser::push_chunk(const PackedByteArray &p_chunk, Vector<AISSEEvent> &r_events) {
	r_events.clear();
	if (!p_chunk.is_empty()) {
		buffer.append_array(p_chunk);
	}

	while (true) {
		int separator_size = 0;
		const int separator_index = _find_event_separator(buffer, separator_size);
		if (separator_index < 0) {
			break;
		}

		const PackedByteArray raw_event_bytes = buffer.slice(0, separator_index);
		buffer = buffer.slice(separator_index + separator_size);
		const String raw_event = raw_event_bytes.is_empty() ? String() : String::utf8(reinterpret_cast<const char *>(raw_event_bytes.ptr()), raw_event_bytes.size());

		AISSEEvent event;
		if (_parse_event_block(raw_event, event)) {
			r_events.push_back(event);
		}
	}

	return !r_events.is_empty();
}

bool AISSEParser::finish(Vector<AISSEEvent> &r_events) {
	r_events.clear();
	if (buffer.is_empty()) {
		return false;
	}

	const PackedByteArray raw_event_bytes = buffer;
	buffer.clear();
	const String raw_event = String::utf8(reinterpret_cast<const char *>(raw_event_bytes.ptr()), raw_event_bytes.size());

	AISSEEvent event;
	if (_parse_event_block(raw_event, event)) {
		r_events.push_back(event);
	}
	return !r_events.is_empty();
}

bool AISSEParser::extract_events_from_text(const String &p_text, bool p_require_complete_events, Vector<AISSEEvent> &r_events) {
	r_events.clear();

	const String normalized = p_text.replace("\r\n", "\n").replace("\r", "\n");
	const Vector<String> raw_events = normalized.split("\n\n", false);
	for (int i = 0; i < raw_events.size(); i++) {
		if (p_require_complete_events && i == raw_events.size() - 1 && !normalized.ends_with("\n\n")) {
			break;
		}

		AISSEEvent event;
		if (_parse_event_block(raw_events[i], event)) {
			r_events.push_back(event);
		}
	}

	return !r_events.is_empty();
}
