/**************************************************************************/
/*  ai_mcp_service.cpp                                                     */
/**************************************************************************/

#include "ai_mcp_service.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

#include "editor/ai_component/providers/ai_mcp_settings.h"
#include "editor/ai_component/tools/ai_mcp_tool.h"

void AIMCPService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh"), &AIMCPService::refresh);
	ClassDB::bind_method(D_METHOD("is_refreshing"), &AIMCPService::is_refreshing);
	ClassDB::bind_method(D_METHOD("get_statuses"), &AIMCPService::get_statuses);
	ClassDB::bind_method(D_METHOD("get_status_summary"), &AIMCPService::get_status_summary);
	ClassDB::bind_method(D_METHOD("has_failures"), &AIMCPService::has_failures);

	ADD_SIGNAL(MethodInfo("status_changed", PropertyInfo(Variant::ARRAY, "statuses"), PropertyInfo(Variant::DICTIONARY, "summary")));
	ADD_SIGNAL(MethodInfo("tools_changed"));
}

Ref<AIMCPService> AIMCPService::get_singleton() {
	if (singleton.is_null()) {
		singleton.instantiate();
	}
	return singleton;
}

void AIMCPService::clear_singleton_for_test() {
	singleton.unref();
}

AIMCPService::AIMCPService() {
	_ensure_runtime();
}

void AIMCPService::_ensure_runtime() {
	if (discovery.is_null()) {
		discovery.instantiate();
	}
	if (discovery_runner.is_null()) {
		discovery_runner.instantiate();
		discovery_runner->connect("discovery_finished", callable_mp(this, &AIMCPService::_on_discovery_finished), CONNECT_DEFERRED);
	}
}

void AIMCPService::_emit_status_changed() {
	emit_signal(SNAME("status_changed"), get_statuses(), get_status_summary());
}

void AIMCPService::refresh() {
	_ensure_runtime();
	if (discovery_runner->is_running()) {
		refresh_pending = true;
		active_refresh_request_id = 0;
		const bool had_discovered_tools = !discovered_tools.is_empty();
		Vector<AIMCPServerConfig> servers = AIMCPSettings::get_server_status_configs();
		discovery->begin_refresh(servers);
		discovered_tools.clear();
		_emit_status_changed();
		if (had_discovered_tools) {
			emit_signal(SNAME("tools_changed"));
		}
		print_line("[AI Agent][MCP] Discovery refresh coalesced because a refresh is already running.");
		return;
	}

	refresh_pending = false;
	Vector<AIMCPServerConfig> servers = AIMCPSettings::get_server_status_configs();
	discovery->begin_refresh(servers);
	const bool had_discovered_tools = !discovered_tools.is_empty();
	discovered_tools.clear();
	active_refresh_request_id = next_refresh_request_id++;
	_emit_status_changed();
	if (had_discovered_tools) {
		emit_signal(SNAME("tools_changed"));
	}

	if (servers.is_empty()) {
		active_refresh_request_id = 0;
		if (!had_discovered_tools) {
			emit_signal(SNAME("tools_changed"));
		}
		return;
	}

	if (!discovery_runner->start(servers, active_refresh_request_id, 3000)) {
		refresh_pending = true;
		print_line("[AI Agent][MCP] Discovery refresh deferred because the runner refused to start.");
	}
}

bool AIMCPService::is_refreshing() const {
	return discovery_runner.is_valid() && discovery_runner->is_running();
}

void AIMCPService::_on_discovery_finished() {
	_ensure_runtime();
	const uint64_t finished_request_id = discovery_runner->get_last_finished_request_id();
	if (finished_request_id != active_refresh_request_id) {
		print_line(vformat("[AI Agent][MCP] Ignored stale discovery result. finished=%d active=%d", finished_request_id, active_refresh_request_id));
		if (refresh_pending && !discovery_runner->is_running()) {
			refresh();
		}
		return;
	}

	Vector<AIMCPServerDiscoveryResult> results = discovery_runner->get_last_results();
	discovery->apply_results(results);
	discovered_tools = AIMCPToolDiscovery::build_discovered_tools(results);
	active_refresh_request_id = 0;
	_emit_status_changed();
	emit_signal(SNAME("tools_changed"));

	if (refresh_pending) {
		refresh();
	}
}

void AIMCPService::register_discovered_tools(const Ref<AIToolRegistry> &p_tool_registry, AIToolPermission p_permission) const {
	ERR_FAIL_COND(p_tool_registry.is_null());
	for (int i = 0; i < discovered_tools.size(); i++) {
		Ref<AIMCPTool> tool;
		tool.instantiate();
		tool->setup(discovered_tools[i].server, discovered_tools[i].descriptor);
		const String tool_name = tool->get_name();
		if (p_tool_registry->has_tool(tool_name)) {
			print_line(vformat("[AI Agent][MCP] Skipped cached tool because it is already registered: %s server=%s source_tool=%s", tool_name, discovered_tools[i].server.display_name, discovered_tools[i].descriptor.name));
			continue;
		}

		if (!p_tool_registry->register_tool(tool, p_permission)) {
			print_line(vformat("[AI Agent][MCP] Skipped cached tool because registration failed: %s server=%s source_tool=%s", tool_name, discovered_tools[i].server.display_name, discovered_tools[i].descriptor.name));
			continue;
		}
		p_tool_registry->set_tool_exposure(tool_name, "mcp:" + discovered_tools[i].server.id, false);

		print_line(vformat("[AI Agent][MCP] Registered tool from service cache: %s server=%s source_tool=%s", tool_name, discovered_tools[i].server.display_name, discovered_tools[i].descriptor.name));
	}
}

Array AIMCPService::get_statuses() const {
	if (discovery.is_null()) {
		return Array();
	}
	return discovery->get_statuses();
}

Dictionary AIMCPService::get_status_summary() const {
	if (discovery.is_null()) {
		return Dictionary();
	}
	return discovery->get_summary();
}

bool AIMCPService::has_failures() const {
	return discovery.is_valid() && discovery->has_failures();
}

Vector<AIMCPDiscoveredTool> AIMCPService::get_discovered_tools() const {
	return discovered_tools;
}
