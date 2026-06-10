/**************************************************************************/
/*  ai_cancel_token.h                                                     */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/safe_refcount.h"

class AICancelToken : public RefCounted {
	GDCLASS(AICancelToken, RefCounted);

	SafeFlag cancel_requested;
	mutable Mutex reason_mutex;
	String cancel_reason;

protected:
	static void _bind_methods();

public:
	AICancelToken();

	void request_cancel(const String &p_reason = String());
	void clear_cancel();
	bool is_cancel_requested() const;
	String get_cancel_reason() const;
	String get_cancel_message(const String &p_fallback = "Operation cancelled.") const;
};
