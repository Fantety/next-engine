/**************************************************************************/
/*  ai_scene_set_property_tool.cpp                                        */
/**************************************************************************/

#include "ai_scene_set_property_tool.h"

#include "core/variant/variant.h"

AISceneSetPropertyTool::AISceneSetPropertyTool() {
	service.instantiate();
}

String AISceneSetPropertyTool::get_name() const {
	return "scene.set_property";
}

String AISceneSetPropertyTool::get_description() const {
	return "Sets a property on a node in the current edited scene using editor APIs and saves the scene.";
}

Dictionary AISceneSetPropertyTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root.";
	properties["node_path"] = node_path_property;

	Dictionary property_path_property;
	property_path_property["type"] = "string";
	property_path_property["description"] = "Property path on the node. Prefer exact paths returned by scene.list_properties. Use colon subpaths for indexed values, for example position:x, shape:size, or material_override:shader.";
	properties["property_path"] = property_path_property;

	Dictionary value_property;
	value_property["description"] = "Value to assign. Use matching JSON scalars, arrays, or {\"type\":\"Vector2\",\"args\":[10,20]} for vector-like values. Resource properties accept null, {\"resource_path\":\"res://...\"}, or {\"resource_type\":\"RectangleShape2D\",\"properties\":{\"size\":{\"type\":\"Vector2\",\"args\":[64,32]}}}.";
	properties["value"] = value_property;

	Array required;
	required.push_back("node_path");
	required.push_back("property_path");
	required.push_back("value");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneSetPropertyTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	const String property_path = String(p_arguments.get("property_path", "")).strip_edges();
	Variant value = p_arguments.get("value", Variant());
	print_line(vformat("[AI Agent][Tool:scene.set_property] Start. node_path=%s property_path=%s value_type=%s", node_path, property_path, Variant::get_type_name(value.get_type())));

	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:scene.set_property] Failed: missing required node_path.");
		return result;
	}
	if (property_path.is_empty()) {
		result.error = "Missing required property_path.";
		print_line("[AI Agent][Tool:scene.set_property] Failed: missing required property_path.");
		return result;
	}
	if (!p_arguments.has("value")) {
		result.error = "Missing required value.";
		print_line("[AI Agent][Tool:scene.set_property] Failed: missing required value.");
		return result;
	}

	AISceneEditingResult edit_result = service->set_property(node_path, property_path, value);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to set property.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.set_property] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.set_property] Completed. %s", result.content));
	return result;
}
