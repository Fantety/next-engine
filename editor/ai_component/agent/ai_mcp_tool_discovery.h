/**************************************************************************/
/*  ai_mcp_tool_discovery.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_mcp_protocol.h"
#include "editor/ai_component/providers/ai_mcp_status_tracker.h"

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AIMCPServerDiscoveryResult {
	AIMCPServerConfig server;
	String status;
	String error;
	Vector<AIMCPToolDescriptor> tools;
};

struct AIMCPDiscoveredTool {
	AIMCPServerConfig server;
	AIMCPToolDescriptor descriptor;
};

class AIMCPToolDiscovery : public RefCounted {
	GDCLASS(AIMCPToolDiscovery, RefCounted);

	Ref<AIMCPStatusTracker> status_tracker;

protected:
	static void _bind_methods();

public:
	AIMCPToolDiscovery();

	Vector<AIMCPServerConfig> begin_refresh();
	void begin_refresh(const Vector<AIMCPServerConfig> &p_servers);
	static Vector<AIMCPServerDiscoveryResult> discover_servers(const Vector<AIMCPServerConfig> &p_servers, int p_timeout_msec);
	static Vector<AIMCPDiscoveredTool> build_discovered_tools(const Vector<AIMCPServerDiscoveryResult> &p_results);
	void apply_results(const Vector<AIMCPServerDiscoveryResult> &p_results);
	Array get_statuses() const;
	Dictionary get_summary() const;
	bool has_failures() const;
};
