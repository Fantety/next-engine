/**************************************************************************/
/*  ai_scene_list_properties_tool.cpp                                     */
/**************************************************************************/

#include "ai_scene_list_properties_tool.h"

#include "core/variant/variant.h"

AISceneListPropertiesTool::AISceneListPropertiesTool() {
	service.instantiate();
}

String AISceneListPropertiesTool::get_name() const {
	return "scene.list_properties";
}

String AISceneListPropertiesTool::get_description() const {
	return "Lists editable properties, types, hints, Resource expectations, current value previews, and property paths for scene.apply_patch.";
}

Dictionary AISceneListPropertiesTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root. Use . for the scene root.";
	properties["node_path"] = node_path_property;

	Dictionary filter_property;
	filter_property["type"] = "string";
	filter_property["description"] = "Optional case-insensitive filter matched against property path or type, for example position, transform, color, text, or script.";
	properties["filter"] = filter_property;

	Dictionary max_property;
	max_property["type"] = "integer";
	max_property["description"] = "Maximum number of properties to return. Defaults to 120 and is clamped to 1..300.";
	properties["max_properties"] = max_property;

	Dictionary include_read_only_property;
	include_read_only_property["type"] = "boolean";
	include_read_only_property["description"] = "Whether to include read-only properties. Defaults to false.";
	properties["include_read_only"] = include_read_only_property;

	Dictionary include_current_values_property;
	include_current_values_property["type"] = "boolean";
	include_current_values_property["description"] = "Whether to include current value previews. Defaults to true.";
	properties["include_current_values"] = include_current_values_property;

	Array required;
	required.push_back("node_path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneListPropertiesTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	const String filter = String(p_arguments.get("filter", "")).strip_edges();
	const int max_properties = p_arguments.get("max_properties", 120);
	const bool include_read_only = p_arguments.get("include_read_only", false);
	const bool include_current_values = p_arguments.get("include_current_values", true);
	print_line(vformat("[AI Agent][Tool:scene.list_properties] Start. node_path=%s filter=%s max_properties=%d include_read_only=%s include_current_values=%s", node_path, filter, max_properties, include_read_only ? "yes" : "no", include_current_values ? "yes" : "no"));

	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:scene.list_properties] Failed: missing required node_path.");
		return result;
	}

	AISceneEditingResult edit_result = service->list_properties(node_path, filter, max_properties, include_read_only, include_current_values);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to list node properties.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.list_properties] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.list_properties] Completed. properties=%d omitted=%d", int(result.metadata.get("matched_count", 0)), int(result.metadata.get("omitted_count", 0))));
	return result;
}
