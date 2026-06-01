/**************************************************************************/
/*  ai_agent_runtime_runner.cpp                                           */
/**************************************************************************/

#include "ai_agent_runtime_runner.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

void AIAgentRuntimeRunner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_flush_runtime_message_updates"), &AIAgentRuntimeRunner::_flush_runtime_message_updates);

	ADD_SIGNAL(MethodInfo("runtime_finished"));
	ADD_SIGNAL(MethodInfo("runtime_message_added", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::DICTIONARY, "message")));
	ADD_SIGNAL(MethodInfo("runtime_message_updated", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::DICTIONARY, "message")));
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

bool AIAgentRuntimeRunner::start(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents) {
	if (running.is_set() || runtime.is_null()) {
		return false;
	}
	if (thread.is_started()) {
		thread.wait_to_finish();
	}

	ThreadParams *params = memnew(ThreadParams);
	params->runner = Ref<AIAgentRuntimeRunner>(this);
	params->messages = p_messages;
	params->context_documents = p_context_documents.duplicate(true);

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
	Array context_documents = params->context_documents;
	memdelete(params);

	AIAgentRuntimeResult result;
	if (runner.is_valid() && runner->runtime.is_valid()) {
		runner->runtime->set_progress_callbacks(callable_mp(runner.ptr(), &AIAgentRuntimeRunner::_on_runtime_message_added), callable_mp(runner.ptr(), &AIAgentRuntimeRunner::_on_runtime_message_updated));
		result = runner->runtime->run(messages, context_documents);
		runner->runtime->clear_progress_callbacks();
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

void AIAgentRuntimeRunner::_on_runtime_message_added(int p_index, const Dictionary &p_message) {
	call_deferred("emit_signal", SNAME("runtime_message_added"), p_index, p_message.duplicate(true));
}

void AIAgentRuntimeRunner::_on_runtime_message_updated(int p_index, const Dictionary &p_message) {
	bool queue_flush = false;
	{
		MutexLock lock(progress_mutex);
		int pending_index = -1;
		for (int i = 0; i < pending_runtime_message_updates.size(); i++) {
			if (pending_runtime_message_updates[i].index == p_index) {
				pending_index = i;
				break;
			}
		}

		PendingRuntimeMessageUpdate update;
		update.index = p_index;
		update.message = p_message.duplicate(true);
		if (pending_index >= 0) {
			pending_runtime_message_updates.write[pending_index] = update;
		} else {
			pending_runtime_message_updates.push_back(update);
		}

		if (!runtime_message_update_flush_queued) {
			runtime_message_update_flush_queued = true;
			queue_flush = true;
		}
	}

	if (queue_flush) {
		call_deferred(SNAME("_flush_runtime_message_updates"));
	}
}

void AIAgentRuntimeRunner::_flush_runtime_message_updates() {
	Vector<PendingRuntimeMessageUpdate> updates;
	{
		MutexLock lock(progress_mutex);
		updates = pending_runtime_message_updates;
		pending_runtime_message_updates.clear();
		runtime_message_update_flush_queued = false;
	}

	for (int i = 0; i < updates.size(); i++) {
		emit_signal(SNAME("runtime_message_updated"), updates[i].index, updates[i].message);
	}
}
