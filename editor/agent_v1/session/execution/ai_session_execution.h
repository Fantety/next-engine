/**************************************************************************/
/*  ai_session_execution.h                                                */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/threading/ai_task_runner.h"
#include "editor/agent_v1/session/model/ai_session_types.h"
#include "editor/agent_v1/session/runner/ai_session_drain_runner.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"

class AISessionExecution : public RefCounted {
	GDCLASS(AISessionExecution, RefCounted);

	struct RunSlot {
		AISessionExecutionState state;
		Ref<AITaskRunner> task_runner;
	};

	HashMap<String, RunSlot> slots;
	Ref<AISessionDrainRunner> runner;
	mutable Mutex mutex;

	static void _merge_wake_seq(AISessionExecutionState &r_state, int64_t p_seq);
	void _emit_drain_requested(const AISessionExecutionState &p_state);
	bool _start_task(const String &p_session_id, const Ref<AITaskRunner> &p_task_runner, const String &p_run_id);
	Dictionary _drain_task(const Ref<AICancelToken> &p_cancel_token, const Variant &p_argument);
	AISessionExecutionState _finish_run(const String &p_session_id, const String &p_run_id, bool p_interrupted, const String &p_interrupt_reason);

protected:
	static void _bind_methods();

public:
	~AISessionExecution();

	void set_runner(const Ref<AISessionDrainRunner> &p_runner);
	Ref<AISessionDrainRunner> get_runner() const;

	bool wake_struct(const String &p_session_id, int64_t p_seq, AISessionExecutionState &r_state);
	bool settle_struct(const String &p_session_id, AISessionExecutionState &r_state);
	bool interrupt_struct(const String &p_session_id, const String &p_reason, AISessionExecutionState &r_state);
	AISessionExecutionState get_state_struct(const String &p_session_id) const;

	Dictionary wake(const String &p_session_id, int64_t p_seq = 0);
	Dictionary settle(const String &p_session_id);
	Dictionary interrupt(const String &p_session_id, const String &p_reason = String());
	Dictionary get_state(const String &p_session_id) const;
	void clear();
};
