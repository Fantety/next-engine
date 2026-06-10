/**************************************************************************/
/*  ai_operation_context.h                                                */
/**************************************************************************/

#pragma once

#include "editor/ai_component/core/base/ai_cancel_token.h"

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

struct AIOperationContext {
	String operation_id;
	String session_id;
	String agent_id;
	String location_key;
	int timeout_msec = 30000;
	Dictionary metadata;
	Ref<AICancelToken> cancel_token;

	bool is_cancel_requested() const;
	String get_cancel_message(const String &p_fallback = "Operation cancelled.") const;
	Ref<AICancelToken> ensure_cancel_token();
	AIOperationContext make_child(const String &p_operation_prefix) const;
	Dictionary to_dictionary() const;
};
