/**************************************************************************/
/*  ai_session_execution.cpp                                              */
/**************************************************************************/

#include "ai_session_execution.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AISessionExecution::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_runner", "runner"), &AISessionExecution::set_runner);
	ClassDB::bind_method(D_METHOD("get_runner"), &AISessionExecution::get_runner);
	ClassDB::bind_method(D_METHOD("wake", "session_id", "seq"), &AISessionExecution::wake, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("settle", "session_id"), &AISessionExecution::settle);
	ClassDB::bind_method(D_METHOD("interrupt", "session_id", "reason"), &AISessionExecution::interrupt, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_state", "session_id"), &AISessionExecution::get_state);
	ClassDB::bind_method(D_METHOD("clear"), &AISessionExecution::clear);

	ADD_SIGNAL(MethodInfo("drain_requested", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::STRING, "run_id"), PropertyInfo(Variant::INT, "wake_seq")));
	ADD_SIGNAL(MethodInfo("drain_settled", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::STRING, "run_id"), PropertyInfo(Variant::BOOL, "interrupted")));
	ADD_SIGNAL(MethodInfo("interrupt_requested", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::STRING, "reason")));
}

void AISessionExecution::_merge_wake_seq(AISessionExecutionState &r_state, int64_t p_seq) {
	if (p_seq > r_state.wake_seq) {
		r_state.wake_seq = p_seq;
	}
}

void AISessionExecution::_emit_drain_requested(const AISessionExecutionState &p_state) {
	call_deferred("emit_signal", SNAME("drain_requested"), p_state.session_id, p_state.active_run_id, p_state.wake_seq);
}

bool AISessionExecution::_start_task(const String &p_session_id, const Ref<AITaskRunner> &p_task_runner, const String &p_run_id) {
	if (p_task_runner.is_null()) {
		return false;
	}

	Dictionary argument;
	argument["session_id"] = p_session_id;
	argument["run_id"] = p_run_id;
	return p_task_runner->start(callable_mp(this, &AISessionExecution::_drain_task), argument);
}

Dictionary AISessionExecution::_drain_task(const Ref<AICancelToken> &p_cancel_token, const Variant &p_argument) {
	const Dictionary argument = p_argument.get_type() == Variant::DICTIONARY ? Dictionary(p_argument) : Dictionary();
	const String session_id = String(argument.get("session_id", String())).strip_edges();
	const String run_id = String(argument.get("run_id", String())).strip_edges();

	Dictionary result;
	result["session_id"] = session_id;
	result["run_id"] = run_id;
	result["success"] = false;
	result["promoted_count"] = 0;

	if (session_id.is_empty() || run_id.is_empty()) {
		result["error"] = AIError::make(AI_ERROR_VALIDATION, "Session drain task requires session_id and run_id.").to_dictionary();
		return result;
	}

	int64_t promoted_count = 0;
	while (true) {
		if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
			const AISessionExecutionState final_state = _finish_run(session_id, run_id, true, p_cancel_token->get_cancel_message("Session drain interrupted."));
			call_deferred("emit_signal", SNAME("drain_settled"), session_id, run_id, final_state.interrupted);
			result["success"] = false;
			result["promoted_count"] = promoted_count;
			result["error"] = AIError::make(AI_ERROR_INTERRUPTED, final_state.interrupt_reason).to_dictionary();
			return result;
		}

		Ref<AISessionDrainRunner> active_runner;
		AISessionExecutionState drain_state;
		bool should_return = false;
		bool return_success = true;
		bool emit_settled = false;
		bool settled_interrupted = false;
		AIError return_error;
		{
			MutexLock lock(mutex);
			if (!slots.has(session_id)) {
				should_return = true;
				return_success = true;
			} else {
				RunSlot slot = slots[session_id];
				if (!slot.state.active || slot.state.active_run_id != run_id) {
					should_return = true;
					return_success = true;
				} else if (slot.state.interrupted) {
					const String reason = slot.state.interrupt_reason;
					slot.state.active = false;
					slot.state.wake_pending = false;
					slot.state.active_run_id = String();
					slots[session_id] = slot;
					should_return = true;
					return_success = false;
					emit_settled = true;
					settled_interrupted = true;
					return_error = AIError::make(AI_ERROR_INTERRUPTED, reason.is_empty() ? String("Session drain interrupted.") : reason);
				} else if (!slot.state.wake_pending) {
					slot.state.active = false;
					slot.state.active_run_id = String();
					slot.state.interrupted = false;
					slot.state.interrupt_reason = String();
					slot.state.wake_seq = 0;
					slots[session_id] = slot;
					should_return = true;
					return_success = true;
					emit_settled = true;
					settled_interrupted = false;
				} else {
					slot.state.wake_pending = false;
					drain_state = slot.state;
					active_runner = runner;
					slots[session_id] = slot;
				}
			}
		}

		if (should_return) {
			if (emit_settled) {
				call_deferred("emit_signal", SNAME("drain_settled"), session_id, run_id, settled_interrupted);
			}
			result["success"] = return_success;
			result["promoted_count"] = promoted_count;
			if (!return_success) {
				result["error"] = return_error.to_dictionary();
			}
			return result;
		}

		_emit_drain_requested(drain_state);

		if (active_runner.is_null()) {
			const AIError error = AIError::make(AI_ERROR_UNAVAILABLE, "SessionExecution has no runner.");
			_finish_run(session_id, run_id, false, String());
			call_deferred("emit_signal", SNAME("drain_settled"), session_id, run_id, false);
			result["success"] = false;
			result["promoted_count"] = promoted_count;
			result["error"] = error.to_dictionary();
			return result;
		}

		Vector<AISessionInputRecord> promoted;
		AIError error;
		if (!active_runner->drain_struct(session_id, p_cancel_token, drain_state.wake_seq, promoted, error)) {
			const bool interrupted = error.kind == AI_ERROR_INTERRUPTED || (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested());
			const String reason = interrupted ? error.message : String();
			_finish_run(session_id, run_id, interrupted, reason);
			call_deferred("emit_signal", SNAME("drain_settled"), session_id, run_id, interrupted);
			result["success"] = false;
			result["promoted_count"] = promoted_count;
			result["error"] = error.to_dictionary();
			return result;
		}

		promoted_count += promoted.size();
	}
}

AISessionExecutionState AISessionExecution::_finish_run(const String &p_session_id, const String &p_run_id, bool p_interrupted, const String &p_interrupt_reason) {
	MutexLock lock(mutex);
	if (!slots.has(p_session_id)) {
		AISessionExecutionState state;
		state.session_id = p_session_id;
		return state;
	}

	RunSlot slot = slots[p_session_id];
	if (slot.state.active_run_id != p_run_id) {
		return slot.state;
	}

	slot.state.active = false;
	slot.state.wake_pending = false;
	slot.state.active_run_id = String();
	slot.state.interrupted = p_interrupted;
	slot.state.interrupt_reason = p_interrupted ? p_interrupt_reason : String();
	if (!p_interrupted) {
		slot.state.wake_seq = 0;
	}
	slots[p_session_id] = slot;
	return slot.state;
}

AISessionExecution::~AISessionExecution() {
	clear();
}

void AISessionExecution::set_runner(const Ref<AISessionDrainRunner> &p_runner) {
	MutexLock lock(mutex);
	runner = p_runner;
}

Ref<AISessionDrainRunner> AISessionExecution::get_runner() const {
	MutexLock lock(mutex);
	return runner;
}

bool AISessionExecution::wake_struct(const String &p_session_id, int64_t p_seq, AISessionExecutionState &r_state) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	Ref<AITaskRunner> task_runner;
	String run_id;
	{
		MutexLock lock(mutex);
		RunSlot slot = slots.has(session_id) ? slots[session_id] : RunSlot();
		slot.state.session_id = session_id;
		_merge_wake_seq(slot.state, p_seq);
		slot.state.wake_pending = true;
		if (slot.state.active) {
			slots[session_id] = slot;
			r_state = slot.state;
			return false;
		}

		if (slot.task_runner.is_null()) {
			slot.task_runner.instantiate();
		}

		slot.state.active = true;
		slot.state.interrupted = false;
		slot.state.interrupt_reason = String();
		slot.state.active_run_id = AIId::make("run");
		slot.state.drain_start_count++;
		task_runner = slot.task_runner;
		run_id = slot.state.active_run_id;
		slots[session_id] = slot;
		r_state = slot.state;
	}

	if (!_start_task(session_id, task_runner, run_id)) {
		if (task_runner.is_valid() && task_runner->is_running()) {
			task_runner->wait_to_finish();
			if (_start_task(session_id, task_runner, run_id)) {
				return true;
			}
		}
		r_state = _finish_run(session_id, run_id, false, String());
		return false;
	}
	return true;
}

bool AISessionExecution::settle_struct(const String &p_session_id, AISessionExecutionState &r_state) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	bool still_running = false;
	{
		MutexLock lock(mutex);
		RunSlot slot = slots.has(session_id) ? slots[session_id] : RunSlot();
		slot.state.session_id = session_id;
		still_running = slot.task_runner.is_valid() && slot.task_runner->is_running();
		if (!slot.state.active || still_running) {
			slots[session_id] = slot;
			r_state = slot.state;
			return false;
		}

		slot.state.active = false;
		slot.state.wake_pending = false;
		slot.state.active_run_id = String();
		slot.state.interrupted = false;
		slot.state.interrupt_reason = String();
		slot.state.wake_seq = 0;
		slots[session_id] = slot;
		r_state = slot.state;
	}
	return false;
}

bool AISessionExecution::interrupt_struct(const String &p_session_id, const String &p_reason, AISessionExecutionState &r_state) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	bool interrupted = false;
	Ref<AITaskRunner> task_runner;
	{
		MutexLock lock(mutex);
		RunSlot slot = slots.has(session_id) ? slots[session_id] : RunSlot();
		slot.state.session_id = session_id;
		if (slot.state.active) {
			slot.state.interrupted = true;
			slot.state.interrupt_reason = p_reason;
			slot.state.wake_pending = false;
			task_runner = slot.task_runner;
			interrupted = true;
		}
		slots[session_id] = slot;
		r_state = slot.state;
	}

	if (interrupted) {
		if (task_runner.is_valid()) {
			task_runner->cancel(p_reason);
		}
		call_deferred("emit_signal", SNAME("interrupt_requested"), session_id, p_reason);
	}
	return interrupted;
}

AISessionExecutionState AISessionExecution::get_state_struct(const String &p_session_id) const {
	const String session_id = p_session_id.strip_edges();
	MutexLock lock(mutex);
	if (slots.has(session_id)) {
		return slots[session_id].state;
	}

	AISessionExecutionState state;
	state.session_id = session_id;
	return state;
}

Dictionary AISessionExecution::wake(const String &p_session_id, int64_t p_seq) {
	AISessionExecutionState state;
	const bool started = wake_struct(p_session_id, p_seq, state);
	Dictionary result;
	result["success"] = !state.session_id.is_empty();
	result["started"] = started;
	result["state"] = state.to_dictionary();
	return result;
}

Dictionary AISessionExecution::settle(const String &p_session_id) {
	AISessionExecutionState state;
	const bool started_next = settle_struct(p_session_id, state);
	Dictionary result;
	result["success"] = !state.session_id.is_empty();
	result["started_next"] = started_next;
	result["state"] = state.to_dictionary();
	return result;
}

Dictionary AISessionExecution::interrupt(const String &p_session_id, const String &p_reason) {
	AISessionExecutionState state;
	const bool interrupted = interrupt_struct(p_session_id, p_reason, state);
	Dictionary result;
	result["success"] = !state.session_id.is_empty();
	result["interrupted"] = interrupted;
	result["state"] = state.to_dictionary();
	return result;
}

Dictionary AISessionExecution::get_state(const String &p_session_id) const {
	return get_state_struct(p_session_id).to_dictionary();
}

void AISessionExecution::clear() {
	Vector<Ref<AITaskRunner>> task_runners;
	{
		MutexLock lock(mutex);
		for (const KeyValue<String, RunSlot> &slot_pair : slots) {
			if (slot_pair.value.task_runner.is_valid()) {
				task_runners.push_back(slot_pair.value.task_runner);
			}
		}
		slots.clear();
	}

	for (int i = 0; i < task_runners.size(); i++) {
		task_runners[i]->cancel("Session execution cleared.");
	}
	for (int i = 0; i < task_runners.size(); i++) {
		task_runners[i]->wait_to_finish();
	}
}
