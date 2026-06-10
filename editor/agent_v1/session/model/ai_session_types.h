/**************************************************************************/
/*  ai_session_types.h                                                    */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/model/ai_domain_types.h"

#include "core/variant/array.h"
#include "core/variant/dictionary.h"

enum AISessionInputStatus {
	AI_SESSION_INPUT_STATUS_ADMITTED,
	AI_SESSION_INPUT_STATUS_PROMOTED,
	AI_SESSION_INPUT_STATUS_CANCELED,
};

String ai_session_input_status_to_string(AISessionInputStatus p_status);
AISessionInputStatus ai_session_input_status_from_string(const String &p_status);

struct AISessionRow {
	String id;
	String agent_id;
	AILocationRef location;
	String title;
	Dictionary metadata;
	uint64_t created_at = 0;
	uint64_t updated_at = 0;

	Dictionary to_dictionary() const;
	static AISessionRow from_dictionary(const Dictionary &p_dict);
};

struct AISessionInputRecord {
	String id;
	String session_id;
	String message_id;
	String role = "user";
	Array parts;
	AIPrompt prompt;
	AISessionInputDelivery delivery = AI_SESSION_INPUT_DELIVERY_STEER;
	AISessionInputStatus status = AI_SESSION_INPUT_STATUS_ADMITTED;
	bool resume = true;
	uint64_t created_at = 0;
	uint64_t promoted_at = 0;
	String idempotency_key;
	String cancel_reason;
	int64_t admitted_seq = 0;
	int64_t promoted_seq = 0;
	Dictionary metadata;

	bool is_admitted() const;
	bool is_promoted() const;
	Dictionary to_dictionary() const;
	static AISessionInputRecord from_dictionary(const Dictionary &p_dict);
};

struct AISessionInputAdmission {
	AISessionInputRecord input;
	bool created = false;
	bool retry = false;
	bool synthesized = false;

	Dictionary to_dictionary() const;
};

struct AISessionPromptResult {
	AISessionRow session;
	AISessionInputRecord prompt;
	bool wake_scheduled = false;
	bool input_created = false;
	bool retry = false;
	bool synthesized = false;

	Dictionary to_dictionary() const;
};

struct AISessionExecutionState {
	String session_id;
	bool active = false;
	bool wake_pending = false;
	String active_run_id;
	bool interrupted = false;
	String interrupt_reason;
	int64_t wake_seq = 0;
	int64_t drain_start_count = 0;

	Dictionary to_dictionary() const;
	static AISessionExecutionState from_dictionary(const Dictionary &p_dict);
};
