/**************************************************************************/
/*  ai_operation_context.cpp                                              */
/**************************************************************************/

#include "ai_operation_context.h"

#include "editor/ai_component/core/base/ai_id.h"

#include "core/variant/variant.h"

bool AIOperationContext::is_cancel_requested() const {
	return cancel_token.is_valid() && cancel_token->is_cancel_requested();
}

String AIOperationContext::get_cancel_message(const String &p_fallback) const {
	return cancel_token.is_valid() ? cancel_token->get_cancel_message(p_fallback) : p_fallback;
}

Ref<AICancelToken> AIOperationContext::ensure_cancel_token() {
	if (cancel_token.is_null()) {
		cancel_token.instantiate();
	}
	return cancel_token;
}

AIOperationContext AIOperationContext::make_child(const String &p_operation_prefix) const {
	AIOperationContext child;
	child.operation_id = AIId::make(p_operation_prefix);
	child.session_id = session_id;
	child.agent_id = agent_id;
	child.location_key = location_key;
	child.timeout_msec = timeout_msec;
	child.metadata = metadata.duplicate(true);
	child.cancel_token = cancel_token;
	return child;
}

Dictionary AIOperationContext::to_dictionary() const {
	Dictionary result;
	result["operation_id"] = operation_id;
	result["session_id"] = session_id;
	result["agent_id"] = agent_id;
	result["location_key"] = location_key;
	result["timeout_msec"] = timeout_msec;
	result["metadata"] = metadata;
	result["cancelled"] = is_cancel_requested();
	return result;
}
