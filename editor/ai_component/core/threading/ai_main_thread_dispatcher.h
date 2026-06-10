/**************************************************************************/
/*  ai_main_thread_dispatcher.h                                           */
/**************************************************************************/

#pragma once

#include "editor/ai_component/core/base/ai_cancel_token.h"

#include "core/object/message_queue.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"

class AIMainThreadDispatcher {
	struct DispatchItem {
		uint64_t id = 0;
		Callable callable;
		Array arguments;
	};

	struct SyncCallRequest {
		Callable callable;
		Array arguments;
		Variant result;
		String error;
		bool success = false;
		Semaphore done;
	};

	static Mutex dispatch_mutex;
	static Vector<DispatchItem> dispatch_items;
	static uint64_t next_dispatch_id;

	static bool _call_callable(const Callable &p_callable, const Array &p_arguments, Variant &r_result, String &r_error);
	static bool _remove_queued_call(uint64_t p_item_id);
	static void _execute_sync_call(uint64_t p_request_ptr);

public:
	static Error queue_call(const Callable &p_callable, const Array &p_arguments, uint64_t &r_item_id);
	static void flush_pending_calls();
	static bool dispatch_sync(const Callable &p_callable, const Array &p_arguments, Variant &r_result, String &r_error, const Ref<AICancelToken> &p_cancel_token = Ref<AICancelToken>(), int p_wait_usec = 1000);
};
