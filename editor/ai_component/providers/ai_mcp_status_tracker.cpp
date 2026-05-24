/**************************************************************************/
/*  ai_mcp_status_tracker.cpp                                             */
/**************************************************************************/

#include "ai_mcp_status_tracker.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"

void AIMCPStatusTracker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_statuses"), &AIMCPStatusTracker::get_statuses);
	ClassDB::bind_method(D_METHOD("get_summary"), &AIMCPStatusTracker::get_summary);
	ClassDB::bind_method(D_METHOD("has_failures"), &AIMCPStatusTracker::has_failures);
}

String AIMCPStatusTracker::_get_endpoint(const AIMCPServerConfig &p_server) {
	if (p_server.transport == "stdio") {
		return p_server.command;
	}
	return p_server.url;
}

int AIMCPStatusTracker::_find_status_index(const AIMCPServerConfig &p_server) {
	if (!p_server.id.is_empty()) {
		const int *index = status_indices.getptr(p_server.id);
		if (index) {
			return *index;
		}
	}
	return -1;
}

void AIMCPStatusTracker::_set_status(const AIMCPServerConfig &p_server, const String &p_status, int p_tool_count, const String &p_error) {
	Dictionary status;
	status["server_id"] = p_server.id;
	status["display_name"] = p_server.display_name;
	status["transport"] = p_server.transport;
	status["endpoint"] = _get_endpoint(p_server);
	status["enabled"] = p_server.enabled;
	status["status"] = p_status;
	status["tool_count"] = p_tool_count;
	status["error"] = p_error;

	const int index = _find_status_index(p_server);
	if (index >= 0 && index < statuses.size()) {
		statuses[index] = status;
		return;
	}

	statuses.push_back(status);
	if (!p_server.id.is_empty()) {
		status_indices.insert(p_server.id, statuses.size() - 1);
	}
}

void AIMCPStatusTracker::begin_refresh(const Vector<AIMCPServerConfig> &p_servers) {
	statuses.clear();
	status_indices.clear();
	for (int i = 0; i < p_servers.size(); i++) {
		const AIMCPServerConfig &server = p_servers[i];
		_set_status(server, server.enabled ? String("checking") : String("disabled"), 0, String());
	}
}

void AIMCPStatusTracker::record_success(const AIMCPServerConfig &p_server, int p_tool_count) {
	_set_status(p_server, "ok", MAX(0, p_tool_count), String());
}

void AIMCPStatusTracker::record_failure(const AIMCPServerConfig &p_server, const String &p_error) {
	const String error = p_error.is_empty() ? String("MCP server initialization failed.") : p_error;
	_set_status(p_server, "failed", 0, error);
}

void AIMCPStatusTracker::record_disabled(const AIMCPServerConfig &p_server) {
	_set_status(p_server, "disabled", 0, String());
}

Array AIMCPStatusTracker::get_statuses() const {
	return statuses.duplicate(true);
}

Dictionary AIMCPStatusTracker::get_summary() const {
	int total = 0;
	int enabled = 0;
	int ok = 0;
	int failed = 0;
	int disabled = 0;
	int checking = 0;
	int tools = 0;

	for (int i = 0; i < statuses.size(); i++) {
		if (Variant(statuses[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary status = statuses[i];
		total++;
		if (bool(status.get("enabled", false))) {
			enabled++;
		}
		tools += (int)status.get("tool_count", 0);

		const String state = String(status.get("status", String()));
		if (state == "ok") {
			ok++;
		} else if (state == "failed") {
			failed++;
		} else if (state == "disabled") {
			disabled++;
		} else if (state == "checking") {
			checking++;
		}
	}

	Dictionary summary;
	summary["total"] = total;
	summary["enabled"] = enabled;
	summary["ok"] = ok;
	summary["failed"] = failed;
	summary["disabled"] = disabled;
	summary["checking"] = checking;
	summary["tool_count"] = tools;
	return summary;
}

bool AIMCPStatusTracker::has_failures() const {
	Dictionary summary = get_summary();
	return (int)summary.get("failed", 0) > 0;
}
