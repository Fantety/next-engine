/**************************************************************************/
/*  ai_next_event_log.cpp                                                 */
/**************************************************************************/

#include "ai_next_event_log.h"

#include "core/object/class_db.h"
#include "core/os/time.h"

void AINextEventLog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear"), &AINextEventLog::clear);
	ClassDB::bind_method(D_METHOD("record_event", "event_type", "milestone_id", "task_id", "agent_id", "message", "metadata"), &AINextEventLog::record_event, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_events"), &AINextEventLog::get_events);
	ClassDB::bind_method(D_METHOD("to_array"), &AINextEventLog::to_array);
	ClassDB::bind_method(D_METHOD("load_from_array", "events"), &AINextEventLog::load_from_array);
}

void AINextEventLog::clear() {
	events.clear();
}

void AINextEventLog::record_event(const String &p_event_type, const String &p_milestone_id, const String &p_task_id, const String &p_agent_id, const String &p_message, const Dictionary &p_metadata) {
	Dictionary event;
	event["timestamp"] = Time::get_singleton()->get_unix_time_from_system();
	event["event_type"] = p_event_type;
	event["milestone_id"] = p_milestone_id;
	event["task_id"] = p_task_id;
	event["agent_id"] = p_agent_id;
	event["message"] = p_message;
	event["metadata"] = p_metadata.duplicate(true);
	events.push_back(event);
}

Array AINextEventLog::get_events() const {
	return to_array();
}

Array AINextEventLog::to_array() const {
	return events.duplicate(true);
}

void AINextEventLog::load_from_array(const Array &p_events) {
	events = p_events.duplicate(true);
}
