/**************************************************************************/
/*  ai_next_event_log.h                                                   */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AINextEventLog : public RefCounted {
	GDCLASS(AINextEventLog, RefCounted);

	Array events;

protected:
	static void _bind_methods();

public:
	void clear();
	void record_event(const String &p_event_type, const String &p_milestone_id, const String &p_task_id, const String &p_agent_id, const String &p_message, const Dictionary &p_metadata = Dictionary());
	Array get_events() const;
	Array to_array() const;
	void load_from_array(const Array &p_events);
};
