/**************************************************************************/
/*  ai_scene_apply_patch_tool.cpp                                         */
/**************************************************************************/

#include "ai_scene_apply_patch_tool.h"

#include "core/variant/variant.h"

namespace {

bool _patch_contains_delete_node(const Dictionary &p_arguments, String &r_error) {
	if (!p_arguments.has("ops")) {
		return false;
	}

	const Array ops = p_arguments["ops"];
	for (int i = 0; i < ops.size(); i++) {
		if (Variant(ops[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary op_dict = ops[i];
		if (String(op_dict.get("op", "")).strip_edges() == "delete_node") {
			r_error = vformat("ops[%d].op: delete_node is not supported by scene.apply_patch. Use scene.delete_node so delete permission is evaluated separately.", i);
			return true;
		}
	}

	return false;
}

} // namespace

AISceneApplyPatchTool::AISceneApplyPatchTool() {
	service.instantiate();
}

String AISceneApplyPatchTool::get_name() const {
	return "scene.apply_patch";
}

String AISceneApplyPatchTool::get_description() const {
	return "Applies a validated scene patch through Godot editor APIs. Use it for scene creation, node add/instantiate/rename/move, and batch property edits. Use scene.delete_node for node deletion. It never writes scene files directly.";
}

Dictionary AISceneApplyPatchTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary create_scene_property;
	create_scene_property["type"] = "object";
	create_scene_property["description"] = "Optional scene creation object. Fields: path (res://... .tscn/.scn), root_type, optional root_name, id, and properties. Omit to patch the currently edited saved scene.";
	Dictionary create_scene_properties;
	Dictionary scene_path_create_property;
	scene_path_create_property["type"] = "string";
	scene_path_create_property["description"] = "Target scene path inside the project, ending with .tscn or .scn.";
	create_scene_properties["path"] = scene_path_create_property;
	Dictionary root_type_property;
	root_type_property["type"] = "string";
	root_type_property["description"] = "Root node class, for example Node2D, Control, Node3D, or Node.";
	create_scene_properties["root_type"] = root_type_property;
	Dictionary root_name_property;
	root_name_property["type"] = "string";
	root_name_property["description"] = "Optional root node name.";
	create_scene_properties["root_name"] = root_name_property;
	Dictionary root_id_property;
	root_id_property["type"] = "string";
	root_id_property["description"] = "Optional stable id for the root node. Use letters, numbers, _, -, or . only.";
	create_scene_properties["id"] = root_id_property;
	Dictionary root_properties_property;
	root_properties_property["type"] = "object";
	root_properties_property["description"] = "Optional property map for the root node. Keys must be real writable property paths for the instantiated root type.";
	create_scene_properties["properties"] = root_properties_property;
	Array create_scene_required;
	create_scene_required.push_back("path");
	create_scene_required.push_back("root_type");
	create_scene_property["properties"] = create_scene_properties;
	create_scene_property["required"] = create_scene_required;
	properties["create_scene"] = create_scene_property;

	Dictionary ops_property;
	ops_property["type"] = "array";
	ops_property["description"] = "Ordered operations. Supported op values: add_node, instantiate_scene, set_property, set_properties, rename_node, move_node. Use scene.delete_node for node deletion. New nodes may define id and later ops can reference that id instead of a NodePath.";
	Dictionary item_schema;
	item_schema["type"] = "object";
	Dictionary item_properties;
	Dictionary op_property;
	op_property["type"] = "string";
	Array op_enum;
	op_enum.push_back("add_node");
	op_enum.push_back("instantiate_scene");
	op_enum.push_back("set_property");
	op_enum.push_back("set_properties");
	op_enum.push_back("rename_node");
	op_enum.push_back("move_node");
	op_property["enum"] = op_enum;
	item_properties["op"] = op_property;

	Dictionary id_property;
	id_property["type"] = "string";
	id_property["description"] = "Optional stable id for a newly created node. Use letters, numbers, _, -, or . only.";
	item_properties["id"] = id_property;

	Dictionary parent_property;
	parent_property["type"] = "string";
	parent_property["description"] = "Parent node path or a prior patch id. Use . or root for the scene root.";
	item_properties["parent"] = parent_property;

	Dictionary node_property;
	node_property["type"] = "string";
	node_property["description"] = "Target node path or prior patch id.";
	item_properties["node"] = node_property;

	Dictionary type_property;
	type_property["type"] = "string";
	type_property["description"] = "Node class for add_node, for example Node2D, Sprite2D, Label, Control, or Timer.";
	item_properties["type"] = type_property;

	Dictionary name_property;
	name_property["type"] = "string";
	name_property["description"] = "Optional node name for add_node or instantiate_scene.";
	item_properties["name"] = name_property;

	Dictionary scene_path_property;
	scene_path_property["type"] = "string";
	scene_path_property["description"] = "PackedScene path for instantiate_scene.";
	item_properties["scene_path"] = scene_path_property;

	Dictionary properties_property;
	properties_property["type"] = "object";
	properties_property["description"] = "Property map for add_node, instantiate_scene, or set_properties. Keys must be real property paths. Values must match property types; vector-like values can use {\"type\":\"Vector2\",\"args\":[10,20]}. Resource-backed properties such as CollisionShape2D.shape, material, texture, or script accept {\"resource_type\":\"...\",\"properties\":{...}} or {\"resource_path\":\"res://...\"}; for a new shape, set shape to {\"resource_type\":\"RectangleShape2D\",\"properties\":{\"size\":{\"type\":\"Vector2\",\"args\":[64,32]}}} instead of editing shape:size while shape is null.";
	item_properties["properties"] = properties_property;

	Dictionary property_property;
	property_property["type"] = "string";
	property_property["description"] = "Single property path for set_property. Prefer exact paths returned by scene.list_properties. Nested Resource subpaths such as shape:size work only after the parent Resource exists; otherwise set the parent Resource with a resource_type/resource_path dictionary and nested properties.";
	item_properties["property"] = property_property;

	Dictionary value_property;
	value_property["description"] = "Single property value for set_property.";
	item_properties["value"] = value_property;

	Dictionary new_name_property;
	new_name_property["type"] = "string";
	new_name_property["description"] = "New node name for rename_node.";
	item_properties["new_name"] = new_name_property;

	Dictionary new_parent_property;
	new_parent_property["type"] = "string";
	new_parent_property["description"] = "New parent node path for move_node.";
	item_properties["new_parent"] = new_parent_property;

	Dictionary position_property;
	position_property["type"] = "integer";
	position_property["description"] = "Target child index. Use -1 or omit to append.";
	item_properties["position"] = position_property;

	Array item_required;
	item_required.push_back("op");
	item_schema["required"] = item_required;
	item_schema["properties"] = item_properties;
	ops_property["items"] = item_schema;
	properties["ops"] = ops_property;

	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneApplyPatchTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	print_line("[AI Agent][Tool:scene.apply_patch] Start.");

	if (!p_arguments.has("ops") && !p_arguments.has("create_scene")) {
		result.error = "Missing required ops or create_scene.";
		print_line("[AI Agent][Tool:scene.apply_patch] Failed: missing patch payload.");
		return result;
	}
	if (p_arguments.has("ops") && Variant(p_arguments["ops"]).get_type() != Variant::ARRAY) {
		result.error = "ops must be an array.";
		print_line("[AI Agent][Tool:scene.apply_patch] Failed: ops is not an array.");
		return result;
	}
	String delete_node_error;
	if (_patch_contains_delete_node(p_arguments, delete_node_error)) {
		result.error = delete_node_error;
		print_line(vformat("[AI Agent][Tool:scene.apply_patch] Failed: %s", result.error));
		return result;
	}
	if (p_arguments.has("create_scene") && Variant(p_arguments["create_scene"]).get_type() != Variant::DICTIONARY) {
		result.error = "create_scene must be an object.";
		print_line("[AI Agent][Tool:scene.apply_patch] Failed: create_scene is not an object.");
		return result;
	}

	AISceneEditingResult edit_result = service->apply_patch(p_arguments);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to apply scene patch.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.apply_patch] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.apply_patch] Completed. %s", result.content));
	return result;
}
