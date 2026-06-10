/**************************************************************************/
/*  ai_task_runner.cpp                                                    */
/**************************************************************************/

#include "ai_task_runner.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AITaskRunner::_bind_methods() {
	ADD_SIGNAL(MethodInfo("task_finished"));
}

AITaskRunner::AITaskRunner() {
	running.clear();
	cancel_token.instantiate();
}

AITaskRunner::~AITaskRunner() {
	cancel();
	wait_to_finish();
}

bool AITaskRunner::start(const Callable &p_task, const Variant &p_argument) {
	if (running.is_set() || !p_task.is_valid()) {
		return false;
	}
	if (thread.is_started()) {
		thread.wait_to_finish();
	}

	cancel_token.instantiate();
	cancel_token->clear_cancel();
	_set_last_result(false, Variant(), String());

	ThreadParams *params = memnew(ThreadParams);
	params->runner = Ref<AITaskRunner>(this);
	params->task = p_task;
	params->argument = p_argument;
	params->cancel_token = cancel_token;

	running.set();
	thread.start(_thread_func, params);
	return true;
}

void AITaskRunner::cancel(const String &p_reason) {
	if (cancel_token.is_valid()) {
		cancel_token->request_cancel(p_reason);
	}
}

void AITaskRunner::wait_to_finish() {
	if (thread.is_started()) {
		thread.wait_to_finish();
	}
}

bool AITaskRunner::is_running() const {
	return running.is_set();
}

Ref<AICancelToken> AITaskRunner::get_cancel_token() const {
	return cancel_token;
}

bool AITaskRunner::was_last_successful() const {
	MutexLock lock(result_mutex);
	return last_success;
}

Variant AITaskRunner::get_last_result() const {
	MutexLock lock(result_mutex);
	return last_result;
}

String AITaskRunner::get_last_error() const {
	MutexLock lock(result_mutex);
	return last_error;
}

void AITaskRunner::_thread_func(void *p_userdata) {
	ThreadParams *params = static_cast<ThreadParams *>(p_userdata);
	Ref<AITaskRunner> runner = params->runner;
	Callable task = params->task;
	Variant argument = params->argument;
	Ref<AICancelToken> token = params->cancel_token;
	memdelete(params);

	bool success = false;
	Variant result;
	String error;

	if (runner.is_valid()) {
		Variant token_variant = token;
		const Variant *argptrs[2] = { &token_variant, &argument };
		Callable::CallError ce;
		task.callp(argptrs, 2, result, ce);
		if (ce.error == Callable::CallError::CALL_OK) {
			success = true;
		} else {
			error = Variant::get_callable_error_text(task, argptrs, 2, ce);
		}

		if (token.is_valid() && token->is_cancel_requested()) {
			success = false;
			error = token->get_cancel_message("Task cancelled.");
		}

		runner->_set_last_result(success, result, error);
		runner->running.clear();
		runner->call_deferred("emit_signal", SNAME("task_finished"));
	}
}

void AITaskRunner::_set_last_result(bool p_success, const Variant &p_result, const String &p_error) {
	MutexLock lock(result_mutex);
	last_success = p_success;
	last_result = p_result;
	last_error = p_error;
}
