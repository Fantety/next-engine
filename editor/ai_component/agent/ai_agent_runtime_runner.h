/**************************************************************************/
/*  ai_agent_runtime_runner.h                                             */
/**************************************************************************/

#pragma once

#include "core/os/mutex.h"
#include "core/os/safe_binary_mutex.h"
#include "core/os/thread.h"
#include "core/templates/vector.h"

#include "editor/ai_component/agent/ai_agent_runtime.h"

class AIAgentRuntimeRunner : public RefCounted {
	GDCLASS(AIAgentRuntimeRunner, RefCounted);

	struct PendingRuntimeMessageUpdate {
		int index = -1;
		Dictionary message;
	};

	struct ThreadParams {
		Ref<AIAgentRuntimeRunner> runner;
		Vector<AIAgentMessage> messages;
		Array context_documents;
	};

	Ref<AIAgentRuntime> runtime;
	Thread thread;
	SafeFlag running;
	AIAgentRuntimeResult last_result;
	Mutex progress_mutex;
	Vector<PendingRuntimeMessageUpdate> pending_runtime_message_updates;
	bool runtime_message_update_flush_queued = false;

	static void _thread_func(void *p_userdata);
	void _set_last_result(const AIAgentRuntimeResult &p_result);
	void _on_runtime_message_added(int p_index, const Dictionary &p_message);
	void _on_runtime_message_updated(int p_index, const Dictionary &p_message);
	void _flush_runtime_message_updates();

protected:
	static void _bind_methods();

public:
	AIAgentRuntimeRunner();
	~AIAgentRuntimeRunner();

	void set_runtime(const Ref<AIAgentRuntime> &p_runtime);
	Ref<AIAgentRuntime> get_runtime() const;

	bool start(const Vector<AIAgentMessage> &p_messages, const Array &p_context_documents = Array());
	void wait_to_finish();
	bool is_running() const;
	AIAgentRuntimeResult get_last_result() const;
};
