/**************************************************************************/
/*  ai_event_types.h                                                      */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

class AIDomainEventTypes {
public:
	static String prompt_admitted();
	static String prompt_promoted();

	static String step_started();
	static String step_ended();
	static String step_failed();

	static String text_started();
	static String text_delta();
	static String text_ended();

	static String reasoning_started();
	static String reasoning_delta();
	static String reasoning_ended();

	static String tool_input_started();
	static String tool_input_delta();
	static String tool_input_ended();
	static String tool_called();
	static String tool_progress();
	static String tool_success();
	static String tool_failed();

	static String context_updated();

	static String compaction_started();
	static String compaction_delta();
	static String compaction_ended();

	static String interrupt_requested();

	static String permission_asked();
	static String permission_replied();

	static bool is_live_only_event(const String &p_type);
	static bool is_durable_event(const String &p_type);
	static bool is_session_event(const String &p_type);
	static bool is_permission_event(const String &p_type);
};

struct AIEventRow {
	static constexpr int CURRENT_SCHEMA_VERSION = 1;

	String id;
	String aggregate_id;
	int schema_version = CURRENT_SCHEMA_VERSION;
	int64_t seq = 0;
	String type;
	Dictionary data;
	String idempotency_key;
	uint64_t timestamp = 0;
	bool live_only = false;

	Dictionary to_dictionary() const;
	static AIEventRow from_dictionary(const Dictionary &p_dict);
};
