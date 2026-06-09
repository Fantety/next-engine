/**************************************************************************/
/*  ai_activate_tool_category_tool.cpp                                    */
/**************************************************************************/

#include "ai_activate_tool_category_tool.h"

#include "editor/ai_component/tools/ai_tool_registry.h"

void AIActivateToolCategoryTool::_bind_methods() {
}

void AIActivateToolCategoryTool::setup(AIToolRegistry *p_registry) {
	registry = p_registry;
}

String AIActivateToolCategoryTool::get_name() const {
	return "agent.activate_tool_category";
}

String AIActivateToolCategoryTool::get_description() const {
	return "Activates one available tool category for progressive tool disclosure. The selected category replaces the previous active category; always-available tools remain exposed. Use category `none` to clear the active category.";
}

Dictionary AIActivateToolCategoryTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary category_property;
	category_property["type"] = "string";
	category_property["description"] = "The category name from the Tool disclosure context, such as scene, script, shader, project, editor_runtime, or mcp:<server_id>. Use `none` to clear the active category.";
	properties["category"] = category_property;

	Array required;
	required.push_back("category");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIActivateToolCategoryTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	if (!registry) {
		result.error = "Tool registry is not available.";
		return result;
	}

	const String category = String(p_arguments.get("category", String())).strip_edges();
	if (category.is_empty()) {
		result.error = "Missing required category.";
		return result;
	}

	if (!registry->set_active_tool_category(category)) {
		result.error = "Unknown or unavailable tool category: `" + category + "`.";
		result.metadata["requested_category"] = category;
		return result;
	}

	const String active_category = registry->get_active_tool_category();
	result.content = active_category.is_empty() ? String("Cleared the active tool category.") : "Activated tool category: `" + active_category + "`.";
	result.content += "\nThe next provider turn will expose always-available tools plus the active category tools.";
	result.metadata["requested_category"] = category;
	result.metadata["active_tool_category"] = active_category;
	result.metadata["tool_origin"] = "tool_disclosure";
	return result;
}
