/**************************************************************************/
/*  ai_mcp_tool_discovery_runner.h                                        */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_mcp_tool_discovery.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/safe_binary_mutex.h"
#include "core/os/thread.h"

class AIMCPToolDiscoveryRunner : public RefCounted {
	GDCLASS(AIMCPToolDiscoveryRunner, RefCounted);

	struct ThreadParams {
		Ref<AIMCPToolDiscoveryRunner> runner;
		Vector<AIMCPServerConfig> servers;
		int timeout_msec = 3000;
		uint64_t request_id = 0;
	};

	Thread thread;
	SafeFlag running;
	mutable Mutex result_mutex;
	Vector<AIMCPServerDiscoveryResult> last_results;
	uint64_t last_finished_request_id = 0;

	static void _thread_func(void *p_userdata);
	void _set_last_results(const Vector<AIMCPServerDiscoveryResult> &p_results, uint64_t p_request_id);

protected:
	static void _bind_methods();

public:
	AIMCPToolDiscoveryRunner();
	~AIMCPToolDiscoveryRunner();

	bool start(const Vector<AIMCPServerConfig> &p_servers, uint64_t p_request_id, int p_timeout_msec = 3000);
	void wait_to_finish();
	bool is_running() const;
	Vector<AIMCPServerDiscoveryResult> get_last_results() const;
	uint64_t get_last_finished_request_id() const;
};
