/**************************************************************************/
/*  ai_tool_registry.h                                                    */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"

#include "editor/ai_component/tools/ai_tool.h"
#include "editor/ai_component/tools/ai_tool_permission.h"

struct AIToolRegistration {
	Ref<AITool> tool;
	AIToolPermission permission = AI_TOOL_PERMISSION_ALLOW;
	String permission_reason;
};

class AIToolRegistry : public RefCounted {
	GDCLASS(AIToolRegistry, RefCounted);

	HashMap<String, AIToolRegistration> tools;

protected:
	static void _bind_methods();

public:
	bool register_tool(const Ref<AITool> &p_tool, AIToolPermission p_permission = AI_TOOL_PERMISSION_ALLOW, const String &p_permission_reason = String());
	void clear();
	bool has_tool(const String &p_name) const;
	Ref<AITool> get_tool(const String &p_name) const;
	bool set_tool_permission(const String &p_name, AIToolPermission p_permission, const String &p_permission_reason = String());
	AIToolPermission get_tool_permission(const String &p_name) const;
	String get_tool_permission_reason(const String &p_name) const;
	Vector<String> get_tool_names() const;
	Array get_tool_schemas() const;
	Array get_available_tool_schemas() const;
};
