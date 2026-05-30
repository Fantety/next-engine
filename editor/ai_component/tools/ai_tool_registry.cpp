/**************************************************************************/
/*  ai_tool_registry.cpp                                                  */
/**************************************************************************/

#include "ai_tool_registry.h"

void AIToolRegistry::_bind_methods() {
}

bool AIToolRegistry::register_tool(const Ref<AITool> &p_tool, AIToolPermission p_permission, const String &p_permission_reason) {
	if (p_tool.is_null()) {
		return false;
	}

	String name = p_tool->get_name();
	if (name.is_empty() || tools.has(name)) {
		return false;
	}

	AIToolRegistration registration;
	registration.tool = p_tool;
	registration.permission = p_permission;
	registration.permission_reason = p_permission_reason;
	tools.insert(name, registration);
	return true;
}

void AIToolRegistry::clear() {
	tools.clear();
}

bool AIToolRegistry::has_tool(const String &p_name) const {
	return tools.has(p_name);
}

Ref<AITool> AIToolRegistry::get_tool(const String &p_name) const {
	const AIToolRegistration *registration = tools.getptr(p_name);
	if (!registration) {
		return Ref<AITool>();
	}
	return registration->tool;
}

bool AIToolRegistry::set_tool_permission(const String &p_name, AIToolPermission p_permission, const String &p_permission_reason) {
	AIToolRegistration *registration = tools.getptr(p_name);
	if (!registration) {
		return false;
	}

	registration->permission = p_permission;
	registration->permission_reason = p_permission_reason;
	return true;
}

AIToolPermission AIToolRegistry::get_tool_permission(const String &p_name) const {
	const AIToolRegistration *registration = tools.getptr(p_name);
	if (!registration) {
		return AI_TOOL_PERMISSION_DENY;
	}
	return registration->permission;
}

String AIToolRegistry::get_tool_permission_reason(const String &p_name) const {
	const AIToolRegistration *registration = tools.getptr(p_name);
	if (!registration) {
		return "Tool is not registered.";
	}
	return registration->permission_reason;
}

Vector<String> AIToolRegistry::get_tool_names() const {
	Vector<String> names;
	for (const KeyValue<String, AIToolRegistration> &tool : tools) {
		names.push_back(tool.key);
	}
	return names;
}

Array AIToolRegistry::get_tool_schemas() const {
	Array schemas;
	for (const KeyValue<String, AIToolRegistration> &tool : tools) {
		if (tool.value.tool.is_valid()) {
			schemas.push_back(tool.value.tool->get_openai_schema());
		}
	}
	return schemas;
}

Array AIToolRegistry::get_available_tool_schemas() const {
	Array schemas;
	for (const KeyValue<String, AIToolRegistration> &tool : tools) {
		if (tool.value.tool.is_null() || tool.value.permission == AI_TOOL_PERMISSION_DENY) {
			continue;
		}
		schemas.push_back(tool.value.tool->get_openai_schema());
	}
	return schemas;
}
