/**************************************************************************/
/*  ai_scene_inspect_node_tool.cpp                                        */
/**************************************************************************/

#include "ai_scene_inspect_node_tool.h"

#include "core/variant/variant.h"

AISceneInspectNodeTool::AISceneInspectNodeTool() {
	service.instantiate();
}

String AISceneInspectNodeTool::get_name() const {
	return "scene.inspect_node";
}

String AISceneInspectNodeTool::get_description() const {
	return "Reads exact current values for selected properties on a node in the currently edited scene. Use after scene.apply_patch to verify layout, text, visibility, anchors, offsets, size, position, and other user-visible changes before claiming completion.";
}

Dictionary AISceneInspectNodeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root. Use . for the scene root.";
	properties["node_path"] = node_path_property;

	Dictionary properties_property;
	properties_property["type"] = "array";
	properties_property["description"] = "Optional list of exact property paths to read. Omit for common layout/visibility/text properties for the node type.";
	Dictionary item_schema;
	item_schema["type"] = "string";
	properties_property["items"] = item_schema;
	properties["properties"] = properties_property;

	Array required;
	required.push_back("node_path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneInspectNodeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	Array property_paths;
	print_line(vformat("[AI Agent][Tool:scene.inspect_node] Start. node_path=%s", node_path));

	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:scene.inspect_node] Failed: missing required node_path.");
		return result;
	}
	if (p_arguments.has("properties")) {
		if (Variant(p_arguments["properties"]).get_type() != Variant::ARRAY) {
			result.error = "properties must be an array of property path strings.";
			print_line("[AI Agent][Tool:scene.inspect_node] Failed: properties is not an array.");
			return result;
		}
		property_paths = p_arguments["properties"];
		for (int i = 0; i < property_paths.size(); i++) {
			const Variant::Type item_type = Variant(property_paths[i]).get_type();
			if (item_type != Variant::STRING && item_type != Variant::STRING_NAME && item_type != Variant::NODE_PATH) {
				result.error = vformat("properties[%d] must be a property path string.", i);
				print_line(vformat("[AI Agent][Tool:scene.inspect_node] Failed: %s", result.error));
				return result;
			}
			if (String(property_paths[i]).strip_edges().is_empty()) {
				result.error = vformat("properties[%d] must not be empty.", i);
				print_line(vformat("[AI Agent][Tool:scene.inspect_node] Failed: %s", result.error));
				return result;
			}
		}
	}

	AISceneEditingResult edit_result = service->inspect_node(node_path, property_paths);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to inspect node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.inspect_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.inspect_node] Completed. properties=%d", int(result.metadata.get("property_count", 0))));
	return result;
}
