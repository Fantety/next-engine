/**************************************************************************/
/*  ai_task_runner.h                                                      */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_cancel_token.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/callable.h"

class AITaskRunner : public RefCounted {
	GDCLASS(AITaskRunner, RefCounted);

	struct ThreadParams {
		Ref<AITaskRunner> runner;
		Callable task;
		Variant argument;
		Ref<AICancelToken> cancel_token;
	};

	Thread thread;
	SafeFlag running;
	Ref<AICancelToken> cancel_token;
	mutable Mutex result_mutex;
	Variant last_result;
	String last_error;
	bool last_success = false;

	static void _thread_func(void *p_userdata);
	void _set_last_result(bool p_success, const Variant &p_result, const String &p_error);

protected:
	static void _bind_methods();

public:
	AITaskRunner();
	~AITaskRunner();

	bool start(const Callable &p_task, const Variant &p_argument = Variant());
	void cancel(const String &p_reason = String());
	void wait_to_finish();
	bool is_running() const;
	Ref<AICancelToken> get_cancel_token() const;

	bool was_last_successful() const;
	Variant get_last_result() const;
	String get_last_error() const;
};
