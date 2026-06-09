/**************************************************************************/
/*  ai_shader_apply_to_node_tool.cpp                                      */
/**************************************************************************/

#include "ai_shader_apply_to_node_tool.h"

#include "core/variant/variant.h"
#include "editor/ai_component/tools/ai_tool_helpers.h"

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
	Dictionary properties;
	properties["node_path"] = AIToolHelpers::make_string_property("Target node path relative to the edited scene root, or . for the root node.");
	properties["shader_path"] = AIToolHelpers::make_string_property("Project path for the .gdshader file, for example res://shaders/player_flash.gdshader.");
	properties["target_property"] = AIToolHelpers::make_string_property("Required target property path on the node, for example material, material_override, shader, "
																		"shader_override, or surface_material_override/0.");

	Dictionary parameters_property;
	parameters_property["type"] = "object";
	parameters_property["description"] = "Optional shader uniform values keyed by uniform name. Basic values can use JSON scalars/arrays or typed values such as {\"type\":\"Color\",\"args\":[1,0,0,1]}. Resource uniforms such as textures/noise accept {\"resource_path\":\"res://...\"} or {\"resource_type\":\"NoiseTexture2D\",\"properties\":{\"noise\":{\"resource_type\":\"FastNoiseLite\",\"properties\":{\"seed\":1}}}}.";
	properties["shader_parameters"] = parameters_property;

	Array required;
	required.push_back("node_path");
	required.push_back("shader_path");
	required.push_back("target_property");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIShaderApplyToNodeTool::execute(const Dictionary &p_arguments) {
	const String node_path = AIToolHelpers::get_stripped_string(p_arguments, "node_path");
	const String shader_path = AIToolHelpers::get_stripped_string(p_arguments, "shader_path");
	const String target_property = AIToolHelpers::get_stripped_string(p_arguments, "target_property");
	Dictionary shader_parameters;
	print_line(vformat("[AI Agent][Tool:shader.apply_to_node] Start. node_path=%s shader_path=%s target_property=%s", node_path, shader_path, target_property));

	if (node_path.is_empty()) {
		print_line("[AI Agent][Tool:shader.apply_to_node] Failed: missing required node_path.");
		return AIToolHelpers::make_missing_required_error("node_path");
	}
	if (shader_path.is_empty()) {
		print_line("[AI Agent][Tool:shader.apply_to_node] Failed: missing required shader_path.");
		return AIToolHelpers::make_missing_required_error("shader_path");
	}
	if (target_property.is_empty()) {
		print_line("[AI Agent][Tool:shader.apply_to_node] Failed: missing required target_property.");
		return AIToolHelpers::make_missing_required_error("target_property");
	}
	if (p_arguments.has("shader_parameters")) {
		if (Variant(p_arguments["shader_parameters"]).get_type() != Variant::DICTIONARY) {
			AIToolResult result;
			result.error = "shader_parameters must be an object.";
			print_line("[AI Agent][Tool:shader.apply_to_node] Failed: shader_parameters is not an object.");
			return result;
		}
		shader_parameters = p_arguments["shader_parameters"];
	}

	AIShaderEditingResult edit_result = service->apply_to_node(node_path, shader_path, target_property, shader_parameters);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to apply shader to node.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:shader.apply_to_node] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:shader.apply_to_node] Completed. %s", result.content));
	return result;
}
