/**************************************************************************/
/*  ai_error.h                                                            */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

enum AIErrorKind {
	AI_ERROR_NONE,
	AI_ERROR_CANCELLED,
	AI_ERROR_TIMEOUT,
	AI_ERROR_NETWORK,
	AI_ERROR_PROVIDER,
	AI_ERROR_PROTOCOL,
	AI_ERROR_VALIDATION,
	AI_ERROR_PERMISSION,
	AI_ERROR_INTERRUPTED,
	AI_ERROR_CONFLICT,
	AI_ERROR_UNAVAILABLE,
	AI_ERROR_INTERNAL,
};

struct AIError {
	AIErrorKind kind = AI_ERROR_NONE;
	String message;
	Dictionary details;

	bool is_error() const;
	Dictionary to_dictionary() const;

	static String kind_to_string(AIErrorKind p_kind);
	static AIErrorKind string_to_kind(const String &p_kind);
	static AIError none();
	static AIError make(AIErrorKind p_kind, const String &p_message, const Dictionary &p_details = Dictionary());
};

struct AIResult {
	bool success = true;
	Variant value;
	AIError error;

	bool is_ok() const;
	Dictionary to_dictionary() const;

	static AIResult ok(const Variant &p_value = Variant());
	static AIResult fail(const AIError &p_error);
	static AIResult fail(AIErrorKind p_kind, const String &p_message, const Dictionary &p_details = Dictionary());
};
