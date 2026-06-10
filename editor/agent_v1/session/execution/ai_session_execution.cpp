/**************************************************************************/
/*  ai_session_execution.cpp                                              */
/**************************************************************************/

#include "ai_session_execution.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/object/class_db.h"

void AISessionExecution::_bind_methods() {
	ClassDB::bind_method(D_METHOD("wake", "session_id", "seq"), &AISessionExecution::wake, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("settle", "session_id"), &AISessionExecution::settle);
	ClassDB::bind_method(D_METHOD("interrupt", "session_id", "reason"), &AISessionExecution::interrupt, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_state", "session_id"), &AISessionExecution::get_state);
	ClassDB::bind_method(D_METHOD("clear"), &AISessionExecution::clear);

	ADD_SIGNAL(MethodInfo("drain_requested", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::STRING, "run_id"), PropertyInfo(Variant::INT, "wake_seq")));
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

bool AISessionExecution::wake_struct(const String &p_session_id, int64_t p_seq, AISessionExecutionState &r_state) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	bool started = false;
	{
		MutexLock lock(mutex);
		AISessionExecutionState state = slots.has(session_id) ? slots[session_id] : AISessionExecutionState();
		state.session_id = session_id;
		_merge_wake_seq(state, p_seq);
		if (state.active) {
			state.wake_pending = true;
			slots[session_id] = state;
			r_state = state;
			return false;
		}

		state.active = true;
		state.wake_pending = false;
		state.interrupted = false;
		state.interrupt_reason = String();
		state.active_run_id = AIId::make("run");
		state.drain_start_count++;
		slots[session_id] = state;
		r_state = state;
		started = true;
	}

	if (started) {
		_emit_drain_requested(r_state);
	}
	return started;
}

bool AISessionExecution::settle_struct(const String &p_session_id, AISessionExecutionState &r_state) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	bool started_next = false;
	{
		MutexLock lock(mutex);
		AISessionExecutionState state = slots.has(session_id) ? slots[session_id] : AISessionExecutionState();
		state.session_id = session_id;
		if (!state.active) {
			slots[session_id] = state;
			r_state = state;
			return false;
		}

		if (state.wake_pending) {
			state.wake_pending = false;
			state.interrupted = false;
			state.interrupt_reason = String();
			state.active = true;
			state.active_run_id = AIId::make("run");
			state.drain_start_count++;
			started_next = true;
		} else {
			state.active = false;
			state.active_run_id = String();
			state.interrupted = false;
			state.interrupt_reason = String();
			state.wake_seq = 0;
		}

		slots[session_id] = state;
		r_state = state;
	}

	if (started_next) {
		_emit_drain_requested(r_state);
	}
	return started_next;
}

bool AISessionExecution::interrupt_struct(const String &p_session_id, const String &p_reason, AISessionExecutionState &r_state) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	bool interrupted = false;
	{
		MutexLock lock(mutex);
		AISessionExecutionState state = slots.has(session_id) ? slots[session_id] : AISessionExecutionState();
		state.session_id = session_id;
		if (state.active) {
			state.interrupted = true;
			state.interrupt_reason = p_reason;
			interrupted = true;
		}
		slots[session_id] = state;
		r_state = state;
	}

	if (interrupted) {
		call_deferred("emit_signal", SNAME("interrupt_requested"), session_id, p_reason);
	}
	return interrupted;
}

AISessionExecutionState AISessionExecution::get_state_struct(const String &p_session_id) const {
	const String session_id = p_session_id.strip_edges();
	MutexLock lock(mutex);
	if (slots.has(session_id)) {
		return slots[session_id];
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
	MutexLock lock(mutex);
	slots.clear();
}
