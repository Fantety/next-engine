/**************************************************************************/
/*  ai_tool_execution_context.h                                            */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class AIToolExecutionContext : public RefCounted {
	GDCLASS(AIToolExecutionContext, RefCounted);

	String session_id;
	String agent_profile_id;
	String tool_call_id;
	bool review_changes = false;

	static thread_local Ref<AIToolExecutionContext> current;

protected:
	static void _bind_methods();

public:
	static void set_current(const Ref<AIToolExecutionContext> &p_context);
	static Ref<AIToolExecutionContext> get_current();
	static void clear_current();

	void set_session_id(const String &p_session_id);
	String get_session_id() const;

	void set_agent_profile_id(const String &p_agent_profile_id);
	String get_agent_profile_id() const;

	void set_tool_call_id(const String &p_tool_call_id);
	String get_tool_call_id() const;

	void set_review_changes(bool p_review_changes);
	bool should_review_changes() const;
};
