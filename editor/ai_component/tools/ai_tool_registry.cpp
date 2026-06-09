/**************************************************************************/
/*  ai_tool_registry.cpp                                                  */
/**************************************************************************/

#include "ai_tool_registry.h"

#include "core/templates/hash_set.h"

namespace {

const int TOOL_CONTEXT_DESCRIPTION_LIMIT = 140;

String _normalize_tool_description(const String &p_description) {
	String description = p_description.strip_edges().replace("\r", " ").replace("\n", " ");
	while (description.contains("  ")) {
		description = description.replace("  ", " ");
	}
	if (description.length() > TOOL_CONTEXT_DESCRIPTION_LIMIT) {
		description = description.substr(0, TOOL_CONTEXT_DESCRIPTION_LIMIT - 3) + "...";
	}
	return description;
}

String _get_category_description(const String &p_category) {
	if (p_category == "project") {
		return "Project files, folders, markdown, and project tree operations.";
	}
	if (p_category == "scene") {
		return "Scene tree inspection and scene mutation tools.";
	}
	if (p_category == "script") {
		return "GDScript inspection, creation, editing, binding, and deletion tools.";
	}
	if (p_category == "shader") {
		return "Godot shader creation, editing, application, parameter, and deletion tools.";
	}
	if (p_category == "editor_runtime") {
		return "Run, stop, and inspect the current editor play session.";
	}
	if (p_category.begins_with("mcp:")) {
		return "External MCP server tools discovered from " + p_category.substr(4) + ".";
	}
	return "Related tools in the `" + p_category + "` category.";
}

bool _is_unrestricted_registration(const AIToolRegistration &p_registration) {
	return p_registration.pinned || p_registration.category.is_empty();
}

bool _should_expose_registration(const AIToolRegistration &p_registration, const String &p_active_tool_category) {
	if (p_registration.tool.is_null() || p_registration.permission == AI_TOOL_PERMISSION_DENY) {
		return false;
	}
	if (_is_unrestricted_registration(p_registration)) {
		return true;
	}
	return !p_active_tool_category.is_empty() && p_registration.category == p_active_tool_category;
}

} // namespace

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
	active_tool_category = String();
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
	if (!active_tool_category.is_empty() && !has_tool_category(active_tool_category)) {
		active_tool_category = String();
	}
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

bool AIToolRegistry::set_tool_exposure(const String &p_name, const String &p_category, bool p_pinned) {
	AIToolRegistration *registration = tools.getptr(p_name);
	if (!registration) {
		return false;
	}

	registration->category = p_category;
	registration->pinned = p_pinned;
	if (!active_tool_category.is_empty() && !has_tool_category(active_tool_category)) {
		active_tool_category = String();
	}
	return true;
}

String AIToolRegistry::get_tool_category(const String &p_name) const {
	const AIToolRegistration *registration = tools.getptr(p_name);
	if (!registration) {
		return String();
	}
	return registration->category;
}

bool AIToolRegistry::is_tool_pinned(const String &p_name) const {
	const AIToolRegistration *registration = tools.getptr(p_name);
	if (!registration) {
		return false;
	}
	return registration->pinned;
}

bool AIToolRegistry::set_active_tool_category(const String &p_category) {
	const String category = p_category.strip_edges();
	if (category.is_empty() || category == "none") {
		active_tool_category = String();
		return true;
	}
	if (!has_tool_category(category)) {
		return false;
	}

	active_tool_category = category;
	return true;
}

void AIToolRegistry::clear_active_tool_category() {
	active_tool_category = String();
}

String AIToolRegistry::get_active_tool_category() const {
	return active_tool_category;
}

bool AIToolRegistry::has_tool_category(const String &p_category) const {
	if (p_category.is_empty()) {
		return false;
	}

	for (const KeyValue<String, AIToolRegistration> &tool : tools) {
		if (tool.value.tool.is_null() || tool.value.permission == AI_TOOL_PERMISSION_DENY || tool.value.pinned) {
			continue;
		}
		if (tool.value.category == p_category) {
			return true;
		}
	}
	return false;
}

Vector<String> AIToolRegistry::get_tool_categories() const {
	HashSet<String> seen_categories;
	Vector<String> categories;
	for (const KeyValue<String, AIToolRegistration> &tool : tools) {
		if (tool.value.tool.is_null() || tool.value.permission == AI_TOOL_PERMISSION_DENY || tool.value.pinned || tool.value.category.is_empty()) {
			continue;
		}
		if (seen_categories.has(tool.value.category)) {
			continue;
		}

		seen_categories.insert(tool.value.category);
		categories.push_back(tool.value.category);
	}
	categories.sort();
	return categories;
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
		if (!_should_expose_registration(tool.value, active_tool_category)) {
			continue;
		}
		schemas.push_back(tool.value.tool->get_openai_schema());
	}
	return schemas;
}

String AIToolRegistry::get_tool_context_prompt() const {
	String context;
	context += "Tool disclosure context:\n";
	context += "Only always-available tools and tools from the active category are exposed in this provider turn. If the needed tool is not exposed, call `agent.activate_tool_category` with one of the available category names; activating a category replaces the previous active category.\n";

	Vector<String> pinned_names;
	Vector<String> active_names;
	HashMap<String, int> category_counts;
	for (const KeyValue<String, AIToolRegistration> &tool : tools) {
		if (tool.value.tool.is_null() || tool.value.permission == AI_TOOL_PERMISSION_DENY) {
			continue;
		}

		if (_is_unrestricted_registration(tool.value)) {
			pinned_names.push_back(tool.key);
			continue;
		}

		int *count = category_counts.getptr(tool.value.category);
		if (count) {
			(*count)++;
		} else {
			category_counts.insert(tool.value.category, 1);
		}

		if (!active_tool_category.is_empty() && tool.value.category == active_tool_category) {
			active_names.push_back(tool.key);
		}
	}
	pinned_names.sort();
	active_names.sort();

	context += "\nAlways available tools:\n";
	if (pinned_names.is_empty()) {
		context += "- none\n";
	} else {
		for (int i = 0; i < pinned_names.size(); i++) {
			const Ref<AITool> tool = get_tool(pinned_names[i]);
			context += "- " + pinned_names[i];
			if (tool.is_valid()) {
				const String description = _normalize_tool_description(tool->get_description());
				if (!description.is_empty()) {
					context += ": " + description;
				}
			}
			context += "\n";
		}
	}

	Vector<String> categories = get_tool_categories();
	context += "\nAvailable tool categories:\n";
	if (categories.is_empty()) {
		context += "- none\n";
	} else {
		for (int i = 0; i < categories.size(); i++) {
			const String &category = categories[i];
			const int *count = category_counts.getptr(category);
			context += "- " + category + " (" + itos(count ? *count : 0) + " tools): " + _get_category_description(category) + "\n";
		}
	}

	context += "\nActive tool category: ";
	context += active_tool_category.is_empty() ? String("none") : active_tool_category;
	context += "\n";

	context += "Active category tools:\n";
	if (active_names.is_empty()) {
		context += "- none\n";
	} else {
		for (int i = 0; i < active_names.size(); i++) {
			const Ref<AITool> tool = get_tool(active_names[i]);
			context += "- " + active_names[i];
			if (tool.is_valid()) {
				const String description = _normalize_tool_description(tool->get_description());
				if (!description.is_empty()) {
					context += ": " + description;
				}
			}
			context += "\n";
		}
	}

	return context;
}
