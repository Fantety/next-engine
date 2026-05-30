/**************************************************************************/
/*  ai_shader_apply_to_node_tool.cpp                                      */
/**************************************************************************/

#include "ai_shader_apply_to_node_tool.h"

#include "core/variant/variant.h"

AIShaderApplyToNodeTool::AIShaderApplyToNodeTool() {
	service.instantiate();
}

String AIShaderApplyToNodeTool::get_name() const {
	return "shader.apply_to_node";
}

String AIShaderApplyToNodeTool::get_description() const {
	return "Applies an existing .gdshader to an explicitly specified Shader or ShaderMaterial node property and saves the current scene.";
}

Dictionary AIShaderApplyToNodeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Target node path relative to the edited scene root, or . for the root node.";
	properties["node_path"] = node_path_property;

	Dictionary shader_path_property;
	shader_path_property["type"] = "string";
	shader_path_property["description"] = "Project path for the .gdshader file, for example res://shaders/player_flash.gdshader.";
	properties["shader_path"] = shader_path_property;

	Dictionary target_property_property;
	target_property_property["type"] = "string";
	target_property_property["description"] = "Required target property path on the node, for example material, material_override, shader, "
											 "shader_override, or surface_material_override/0.";
	properties["target_property"] = target_property_property;

	Dictionary parameters_property;
	parameters_property["type"] = "object";
	parameters_property["description"] = "Optional shader uniform values keyed by uniform name. Values use Godot Variant-compatible JSON values.";
	properties["shader_parameters"] = parameters_property;

	Array required;
	required.push_back("node_path");
	required.push_back("shader_path");
	required.push_back("target_property");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIShaderApplyToNodeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	const String shader_path = String(p_arguments.get("shader_path", "")).strip_edges();
	const String target_property = String(p_arguments.get("target_property", "")).strip_edges();
	Dictionary shader_parameters;
	print_line(vformat("[AI Agent][Tool:shader.apply_to_node] Start. node_path=%s shader_path=%s target_property=%s", node_path, shader_path, target_property));

	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		print_line("[AI Agent][Tool:shader.apply_to_node] Failed: missing required node_path.");
		return result;
	}
	if (shader_path.is_empty()) {
		result.error = "Missing required shader_path.";
		print_line("[AI Agent][Tool:shader.apply_to_node] Failed: missing required shader_path.");
		return result;
	}
	if (target_property.is_empty()) {
		result.error = "Missing required target_property.";
		print_line("[AI Agent][Tool:shader.apply_to_node] Failed: missing required target_property.");
		return result;
	}
	if (p_arguments.has("shader_parameters")) {
		if (Variant(p_arguments["shader_parameters"]).get_type() != Variant::DICTIONARY) {
			result.error = "shader_parameters must be an object.";
			print_line("[AI Agent][Tool:shader.apply_to_node] Failed: shader_parameters is not an object.");
			return result;
		}
		shader_parameters = p_arguments["shader_parameters"];
	}

	AIShaderEditingResult edit_result = service->apply_to_node(node_path, shader_path, target_property, shader_parameters);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to apply shader to node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:shader.apply_to_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:shader.apply_to_node] Completed. %s", result.content));
	return result;
}
