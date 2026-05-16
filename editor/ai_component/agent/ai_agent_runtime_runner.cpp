/**************************************************************************/
/*  ai_agent_runtime_runner.cpp                                           */
/**************************************************************************/

#include "ai_agent_runtime_runner.h"

void AIAgentRuntimeRunner::_bind_methods() {
	ADD_SIGNAL(MethodInfo("runtime_finished"));
}

AIAgentRuntimeRunner::AIAgentRuntimeRunner() {
	running.clear();
}

AIAgentRuntimeRunner::~AIAgentRuntimeRunner() {
	wait_to_finish();
}

void AIAgentRuntimeRunner::set_runtime(const Ref<AIAgentRuntime> &p_runtime) {
	runtime = p_runtime;
}

Ref<AIAgentRuntime> AIAgentRuntimeRunner::get_runtime() const {
	return runtime;
}

bool AIAgentRuntimeRunner::start(const Vector<AIAgentMessage> &p_messages) {
	if (running.is_set() || thread.is_started() || runtime.is_null()) {
		return false;
	}

	ThreadParams *params = memnew(ThreadParams);
	params->runner = Ref<AIAgentRuntimeRunner>(this);
	params->messages = p_messages;

	running.set();
	thread.start(_thread_func, params);
	return true;
}

void AIAgentRuntimeRunner::wait_to_finish() {
	if (thread.is_started()) {
		thread.wait_to_finish();
	}
}

bool AIAgentRuntimeRunner::is_running() const {
	return running.is_set();
}

AIAgentRuntimeResult AIAgentRuntimeRunner::get_last_result() const {
	return last_result;
}

void AIAgentRuntimeRunner::_thread_func(void *p_userdata) {
	ThreadParams *params = static_cast<ThreadParams *>(p_userdata);
	Ref<AIAgentRuntimeRunner> runner = params->runner;
	Vector<AIAgentMessage> messages = params->messages;
	memdelete(params);

	AIAgentRuntimeResult result;
	if (runner.is_valid() && runner->runtime.is_valid()) {
		result = runner->runtime->run(messages);
	} else {
		result.error = "Agent runtime is not configured.";
	}

	if (runner.is_valid()) {
		runner->_set_last_result(result);
		runner->running.clear();
		runner->call_deferred("emit_signal", SNAME("runtime_finished"));
	}
}

void AIAgentRuntimeRunner::_set_last_result(const AIAgentRuntimeResult &p_result) {
	last_result = p_result;
}
