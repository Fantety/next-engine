/**************************************************************************/
/*  ai_tool_registry.cpp                                                  */
/**************************************************************************/

#include "ai_tool_registry.h"

void AIToolRegistry::_bind_methods() {
}

bool AIToolRegistry::register_tool(const Ref<AITool> &p_tool) {
	if (p_tool.is_null()) {
		return false;
	}

	String name = p_tool->get_name();
	if (name.is_empty() || tools.has(name)) {
		return false;
	}

	tools.insert(name, p_tool);
	return true;
}

bool AIToolRegistry::has_tool(const String &p_name) const {
	return tools.has(p_name);
}

Ref<AITool> AIToolRegistry::get_tool(const String &p_name) const {
	const Ref<AITool> *tool = tools.getptr(p_name);
	if (!tool) {
		return Ref<AITool>();
	}
	return *tool;
}

Array AIToolRegistry::get_tool_schemas() const {
	Array schemas;
	for (const KeyValue<String, Ref<AITool>> &tool : tools) {
		schemas.push_back(tool.value->get_openai_schema());
	}
	return schemas;
}
