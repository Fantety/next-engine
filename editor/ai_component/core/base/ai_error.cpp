/**************************************************************************/
/*  ai_error.cpp                                                          */
/**************************************************************************/

#include "ai_error.h"

#include "core/variant/variant.h"

bool AIError::is_error() const {
	return kind != AI_ERROR_NONE;
}

Dictionary AIError::to_dictionary() const {
	Dictionary result;
	result["kind"] = kind_to_string(kind);
	result["message"] = message;
	result["details"] = details;
	return result;
}

String AIError::kind_to_string(AIErrorKind p_kind) {
	switch (p_kind) {
		case AI_ERROR_NONE:
			return "none";
		case AI_ERROR_CANCELLED:
			return "cancelled";
		case AI_ERROR_TIMEOUT:
			return "timeout";
		case AI_ERROR_NETWORK:
			return "network";
		case AI_ERROR_PROVIDER:
			return "provider";
		case AI_ERROR_PROTOCOL:
			return "protocol";
		case AI_ERROR_VALIDATION:
			return "validation";
		case AI_ERROR_PERMISSION:
			return "permission";
		case AI_ERROR_INTERRUPTED:
			return "interrupted";
		case AI_ERROR_CONFLICT:
			return "conflict";
		case AI_ERROR_UNAVAILABLE:
			return "unavailable";
		case AI_ERROR_INTERNAL:
			return "internal";
	}
	return "internal";
}

AIErrorKind AIError::string_to_kind(const String &p_kind) {
	const String kind = p_kind.strip_edges().to_lower();
	if (kind == "none") {
		return AI_ERROR_NONE;
	}
	if (kind == "cancelled") {
		return AI_ERROR_CANCELLED;
	}
	if (kind == "timeout") {
		return AI_ERROR_TIMEOUT;
	}
	if (kind == "network") {
		return AI_ERROR_NETWORK;
	}
	if (kind == "provider") {
		return AI_ERROR_PROVIDER;
	}
	if (kind == "protocol") {
		return AI_ERROR_PROTOCOL;
	}
	if (kind == "validation") {
		return AI_ERROR_VALIDATION;
	}
	if (kind == "permission") {
		return AI_ERROR_PERMISSION;
	}
	if (kind == "interrupted") {
		return AI_ERROR_INTERRUPTED;
	}
	if (kind == "conflict") {
		return AI_ERROR_CONFLICT;
	}
	if (kind == "unavailable") {
		return AI_ERROR_UNAVAILABLE;
	}
	return AI_ERROR_INTERNAL;
}

AIError AIError::none() {
	return AIError();
}

AIError AIError::make(AIErrorKind p_kind, const String &p_message, const Dictionary &p_details) {
	AIError error;
	error.kind = p_kind;
	error.message = p_message;
	error.details = p_details.duplicate(true);
	return error;
}

bool AIResult::is_ok() const {
	return success && !error.is_error();
}

Dictionary AIResult::to_dictionary() const {
	Dictionary result;
	result["success"] = is_ok();
	result["value"] = value;
	result["error"] = error.to_dictionary();
	return result;
}

AIResult AIResult::ok(const Variant &p_value) {
	AIResult result;
	result.success = true;
	result.value = p_value;
	result.error = AIError::none();
	return result;
}

AIResult AIResult::fail(const AIError &p_error) {
	AIResult result;
	result.success = false;
	result.error = p_error.is_error() ? p_error : AIError::make(AI_ERROR_INTERNAL, "Operation failed.");
	return result;
}

AIResult AIResult::fail(AIErrorKind p_kind, const String &p_message, const Dictionary &p_details) {
	return fail(AIError::make(p_kind, p_message, p_details));
}
