/**************************************************************************/
/*  ai_mcp_tool_discovery_runner.cpp                                      */
/**************************************************************************/

#include "ai_mcp_tool_discovery_runner.h"

#include "core/os/mutex.h"

void AIMCPToolDiscoveryRunner::_bind_methods() {
	ADD_SIGNAL(MethodInfo("discovery_finished"));
}

AIMCPToolDiscoveryRunner::AIMCPToolDiscoveryRunner() {
	running.clear();
}

AIMCPToolDiscoveryRunner::~AIMCPToolDiscoveryRunner() {
	wait_to_finish();
}

bool AIMCPToolDiscoveryRunner::start(const Vector<AIMCPServerConfig> &p_servers, uint64_t p_request_id, int p_timeout_msec) {
	if (running.is_set()) {
		return false;
	}
	if (thread.is_started()) {
		thread.wait_to_finish();
	}

	ThreadParams *params = memnew(ThreadParams);
	params->runner = Ref<AIMCPToolDiscoveryRunner>(this);
	params->servers = p_servers;
	params->timeout_msec = p_timeout_msec;
	params->request_id = p_request_id;

	running.set();
	thread.start(_thread_func, params);
	return true;
}

void AIMCPToolDiscoveryRunner::wait_to_finish() {
	if (thread.is_started()) {
		thread.wait_to_finish();
	}
}

bool AIMCPToolDiscoveryRunner::is_running() const {
	return running.is_set();
}

Vector<AIMCPServerDiscoveryResult> AIMCPToolDiscoveryRunner::get_last_results() const {
	MutexLock lock(result_mutex);
	return last_results;
}

uint64_t AIMCPToolDiscoveryRunner::get_last_finished_request_id() const {
	MutexLock lock(result_mutex);
	return last_finished_request_id;
}

void AIMCPToolDiscoveryRunner::_thread_func(void *p_userdata) {
	ThreadParams *params = static_cast<ThreadParams *>(p_userdata);
	Ref<AIMCPToolDiscoveryRunner> runner = params->runner;
	Vector<AIMCPServerConfig> servers = params->servers;
	const int timeout_msec = params->timeout_msec;
	const uint64_t request_id = params->request_id;
	memdelete(params);

	Vector<AIMCPServerDiscoveryResult> results = AIMCPToolDiscovery::discover_servers(servers, timeout_msec);
	if (runner.is_valid()) {
		runner->_set_last_results(results, request_id);
		runner->running.clear();
		runner->call_deferred("emit_signal", SNAME("discovery_finished"));
	}
}

void AIMCPToolDiscoveryRunner::_set_last_results(const Vector<AIMCPServerDiscoveryResult> &p_results, uint64_t p_request_id) {
	MutexLock lock(result_mutex);
	last_results = p_results;
	last_finished_request_id = p_request_id;
}
