/**************************************************************************/
/*  ai_mcp_tool_discovery.cpp                                             */
/**************************************************************************/

#include "ai_mcp_tool_discovery.h"

#include "core/object/class_db.h"
#include "core/templates/hash_set.h"

#include "editor/ai_component/providers/ai_mcp_client.h"
#include "editor/ai_component/providers/ai_mcp_settings.h"

namespace {

String _get_agent_tool_name(const AIMCPServerConfig &p_server, const AIMCPToolDescriptor &p_tool) {
	if (p_tool.name.is_empty()) {
		return String();
	}
	return AIMCPProtocol::make_agent_tool_name(p_server.id, p_tool.name);
}

int _count_unique_tools_for_status(const Vector<AIMCPServerDiscoveryResult> &p_results, int p_result_index, HashSet<String> &r_seen_agent_tool_names) {
	int tool_count = 0;
	const AIMCPServerDiscoveryResult &result = p_results[p_result_index];
	for (int i = 0; i < result.tools.size(); i++) {
		const String agent_tool_name = _get_agent_tool_name(result.server, result.tools[i]);
		if (agent_tool_name.is_empty() || r_seen_agent_tool_names.has(agent_tool_name)) {
			continue;
		}

		r_seen_agent_tool_names.insert(agent_tool_name);
		tool_count++;
	}
	return tool_count;
}

} // namespace

void AIMCPToolDiscovery::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_statuses"), &AIMCPToolDiscovery::get_statuses);
	ClassDB::bind_method(D_METHOD("get_summary"), &AIMCPToolDiscovery::get_summary);
	ClassDB::bind_method(D_METHOD("has_failures"), &AIMCPToolDiscovery::has_failures);
}

AIMCPToolDiscovery::AIMCPToolDiscovery() {
	status_tracker.instantiate();
}

Vector<AIMCPServerConfig> AIMCPToolDiscovery::begin_refresh() {
	Vector<AIMCPServerConfig> all_servers = AIMCPSettings::get_server_status_configs();
	begin_refresh(all_servers);
	return all_servers;
}

void AIMCPToolDiscovery::begin_refresh(const Vector<AIMCPServerConfig> &p_servers) {
	if (status_tracker.is_null()) {
		status_tracker.instantiate();
	}
	status_tracker->begin_refresh(p_servers);
}

Vector<AIMCPServerDiscoveryResult> AIMCPToolDiscovery::discover_servers(const Vector<AIMCPServerConfig> &p_servers, int p_timeout_msec) {
	Vector<AIMCPServerDiscoveryResult> results;
	for (int i = 0; i < p_servers.size(); i++) {
		const AIMCPServerConfig &server = p_servers[i];
		AIMCPServerDiscoveryResult result;
		result.server = server;
		if (!server.enabled) {
			result.status = "disabled";
			results.push_back(result);
			continue;
		}
		String config_error;
		if (!AIMCPSettings::is_server_config_usable(server, &config_error)) {
			result.status = "failed";
			result.error = config_error;
			print_line(vformat("[AI Agent][MCP] Invalid server config. server=%s error=%s", server.display_name, config_error));
			results.push_back(result);
			continue;
		}

		Ref<AIMCPClient> client = AIMCPClientFactory::create_client(server);
		if (client.is_null()) {
			const String error = vformat("MCP client transport is not available. transport=%s", server.transport);
			result.status = "failed";
			result.error = error;
			print_line(vformat("[AI Agent][MCP] Failed to create client. server=%s transport=%s", server.display_name, server.transport));
			results.push_back(result);
			continue;
		}
		client->set_timeout_msec(p_timeout_msec);

		String error;
		if (!client->list_tools(result.tools, error)) {
			result.status = "failed";
			result.error = error;
			print_line(vformat("[AI Agent][MCP] Failed to discover tools. server=%s error=%s", server.display_name, error));
			results.push_back(result);
			continue;
		}

		result.status = "ok";
		results.push_back(result);
	}
	return results;
}

Vector<AIMCPDiscoveredTool> AIMCPToolDiscovery::build_discovered_tools(const Vector<AIMCPServerDiscoveryResult> &p_results) {
	Vector<AIMCPDiscoveredTool> discovered_tools;
	HashSet<String> seen_agent_tool_names;
	for (int i = 0; i < p_results.size(); i++) {
		const AIMCPServerDiscoveryResult &result = p_results[i];
		if (result.status != "ok") {
			continue;
		}
		for (int j = 0; j < result.tools.size(); j++) {
			const String agent_tool_name = _get_agent_tool_name(result.server, result.tools[j]);
			if (agent_tool_name.is_empty()) {
				print_line(vformat("[AI Agent][MCP] Skipped discovered tool with empty name. server=%s", result.server.display_name));
				continue;
			}
			if (seen_agent_tool_names.has(agent_tool_name)) {
				print_line(vformat("[AI Agent][MCP] Skipped duplicate discovered tool. name=%s server=%s source_tool=%s", agent_tool_name, result.server.display_name, result.tools[j].name));
				continue;
			}

			seen_agent_tool_names.insert(agent_tool_name);
			AIMCPDiscoveredTool discovered_tool;
			discovered_tool.server = result.server;
			discovered_tool.descriptor = result.tools[j];
			discovered_tools.push_back(discovered_tool);
		}
	}
	return discovered_tools;
}

void AIMCPToolDiscovery::apply_results(const Vector<AIMCPServerDiscoveryResult> &p_results) {
	if (status_tracker.is_null()) {
		status_tracker.instantiate();
	}

	HashSet<String> seen_agent_tool_names;
	for (int i = 0; i < p_results.size(); i++) {
		const AIMCPServerDiscoveryResult &result = p_results[i];
		const AIMCPServerConfig &server = result.server;
		if (result.status == "disabled") {
			status_tracker->record_disabled(server);
			continue;
		}
		if (result.status != "ok") {
			status_tracker->record_failure(server, result.error);
			continue;
		}

		status_tracker->record_success(server, _count_unique_tools_for_status(p_results, i, seen_agent_tool_names));
	}
}

Array AIMCPToolDiscovery::get_statuses() const {
	if (status_tracker.is_null()) {
		return Array();
	}
	return status_tracker->get_statuses();
}

Dictionary AIMCPToolDiscovery::get_summary() const {
	if (status_tracker.is_null()) {
		return Dictionary();
	}
	return status_tracker->get_summary();
}

bool AIMCPToolDiscovery::has_failures() const {
	return status_tracker.is_valid() && status_tracker->has_failures();
}
