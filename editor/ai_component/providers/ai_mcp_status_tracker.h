/**************************************************************************/
/*  ai_mcp_status_tracker.h                                               */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_mcp_settings.h"

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIMCPStatusTracker : public RefCounted {
	GDCLASS(AIMCPStatusTracker, RefCounted);

	Array statuses;
	HashMap<String, int> status_indices;

	static String _get_endpoint(const AIMCPServerConfig &p_server);
	int _find_status_index(const AIMCPServerConfig &p_server);
	void _set_status(const AIMCPServerConfig &p_server, const String &p_status, int p_tool_count, const String &p_error);

protected:
	static void _bind_methods();

public:
	void begin_refresh(const Vector<AIMCPServerConfig> &p_servers);
	void record_success(const AIMCPServerConfig &p_server, int p_tool_count);
	void record_failure(const AIMCPServerConfig &p_server, const String &p_error);
	void record_disabled(const AIMCPServerConfig &p_server);
	Array get_statuses() const;
	Dictionary get_summary() const;
	bool has_failures() const;
};
