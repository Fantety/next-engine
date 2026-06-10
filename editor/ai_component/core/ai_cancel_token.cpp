/**************************************************************************/
/*  ai_cancel_token.cpp                                                   */
/**************************************************************************/

#include "ai_cancel_token.h"

void AICancelToken::_bind_methods() {
}

AICancelToken::AICancelToken() {
	cancel_requested.clear();
}

void AICancelToken::request_cancel(const String &p_reason) {
	{
		MutexLock lock(reason_mutex);
		cancel_reason = p_reason;
	}
	cancel_requested.set();
}

void AICancelToken::clear_cancel() {
	cancel_requested.clear();
	MutexLock lock(reason_mutex);
	cancel_reason = String();
}

bool AICancelToken::is_cancel_requested() const {
	return cancel_requested.is_set();
}

String AICancelToken::get_cancel_reason() const {
	MutexLock lock(reason_mutex);
	return cancel_reason;
}

String AICancelToken::get_cancel_message(const String &p_fallback) const {
	const String reason = get_cancel_reason();
	return reason.is_empty() ? p_fallback : reason;
}
