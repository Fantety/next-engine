/**************************************************************************/
/*  ai_tool_registry.h                                                    */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"

#include "editor/ai_component/tools/ai_tool.h"

class AIToolRegistry : public RefCounted {
	GDCLASS(AIToolRegistry, RefCounted);

	HashMap<String, Ref<AITool>> tools;

protected:
	static void _bind_methods();

public:
	bool register_tool(const Ref<AITool> &p_tool);
	bool has_tool(const String &p_name) const;
	Ref<AITool> get_tool(const String &p_name) const;
	Array get_tool_schemas() const;
};
