/**************************************************************************/
/*  ai_mcp_service.h                                                       */
/**************************************************************************/

#pragma once

#include "editor/ai_component/agent/ai_mcp_tool_discovery.h"
#include "editor/ai_component/agent/ai_mcp_tool_discovery_runner.h"
#include "editor/ai_component/tools/ai_tool_registry.h"

#include "core/object/ref_counted.h"

class AIMCPService : public RefCounted {
	GDCLASS(AIMCPService, RefCounted);

	static inline Ref<AIMCPService> singleton;

	Ref<AIMCPToolDiscovery> discovery;
	Ref<AIMCPToolDiscoveryRunner> discovery_runner;
	Vector<AIMCPDiscoveredTool> discovered_tools;
	bool refresh_pending = false;
	uint64_t next_refresh_request_id = 1;
	uint64_t active_refresh_request_id = 0;

	void _ensure_runtime();
	void _emit_status_changed();
	void _on_discovery_finished();

protected:
	static void _bind_methods();

public:
	static Ref<AIMCPService> get_singleton();

	AIMCPService();

	void refresh();
	bool is_refreshing() const;
	void register_discovered_tools(const Ref<AIToolRegistry> &p_tool_registry, AIToolPermission p_permission = AI_TOOL_PERMISSION_ALLOW) const;
	Array get_statuses() const;
	Dictionary get_status_summary() const;
	bool has_failures() const;
	Vector<AIMCPDiscoveredTool> get_discovered_tools() const;
};
