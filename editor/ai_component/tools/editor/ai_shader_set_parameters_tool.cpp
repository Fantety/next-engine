/**************************************************************************/
/*  ai_shader_set_parameters_tool.cpp                                     */
/**************************************************************************/

#include "ai_shader_set_parameters_tool.h"

#include "core/variant/variant.h"
#include "editor/ai_component/tools/ai_tool_helpers.h"

AIShaderSetParametersTool::AIShaderSetParametersTool() {
	service.instantiate();
}

String AIShaderSetParametersTool::get_name() const {
	return "shader.set_parameters";
}

String AIShaderSetParametersTool::get_description() const {
	return "Sets uniform values on an existing ShaderMaterial node property and saves the current scene.";
}

Dictionary AIShaderSetParametersTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIToolHelpers::make_string_property("Target node path relative to the edited scene root, or . for the root node.");
	properties["target_property"] = AIToolHelpers::make_string_property("ShaderMaterial property path on the node, for example material, material_override, or surface_material_override/0.");

	Dictionary parameters_property;
	parameters_property["type"] = "object";
	parameters_property["description"] = "Shader uniform values keyed by uniform name. Basic values can use JSON scalars/arrays or typed values such as {\"type\":\"Color\",\"args\":[1,0,0,1]}. Resource uniforms such as textures/noise accept {\"resource_path\":\"res://...\"} or {\"resource_type\":\"NoiseTexture2D\",\"properties\":{\"noise\":{\"resource_type\":\"FastNoiseLite\",\"properties\":{\"seed\":1}}}}.";
	properties["shader_parameters"] = parameters_property;

	Array required;
	required.push_back("node_path");
	required.push_back("target_property");
	required.push_back("shader_parameters");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIShaderSetParametersTool::execute(const Dictionary &p_arguments) {
	const String node_path = AIToolHelpers::get_stripped_string(p_arguments, "node_path");
	const String target_property = AIToolHelpers::get_stripped_string(p_arguments, "target_property");
	Dictionary shader_parameters;
	print_line(vformat("[AI Agent][Tool:shader.set_parameters] Start. node_path=%s target_property=%s", node_path, target_property));

	if (node_path.is_empty()) {
		print_line("[AI Agent][Tool:shader.set_parameters] Failed: missing required node_path.");
		return AIToolHelpers::make_missing_required_error("node_path");
	}
	if (target_property.is_empty()) {
		print_line("[AI Agent][Tool:shader.set_parameters] Failed: missing required target_property.");
		return AIToolHelpers::make_missing_required_error("target_property");
	}
	if (!p_arguments.has("shader_parameters")) {
		print_line("[AI Agent][Tool:shader.set_parameters] Failed: missing required shader_parameters.");
		return AIToolHelpers::make_missing_required_error("shader_parameters");
	}
	if (Variant(p_arguments["shader_parameters"]).get_type() != Variant::DICTIONARY) {
		AIToolResult result;
		result.error = "shader_parameters must be an object.";
		print_line("[AI Agent][Tool:shader.set_parameters] Failed: shader_parameters is not an object.");
		return result;
	}
	shader_parameters = p_arguments["shader_parameters"];
	if (shader_parameters.is_empty()) {
		AIToolResult result;
		result.error = "shader_parameters must include at least one uniform value.";
		print_line("[AI Agent][Tool:shader.set_parameters] Failed: shader_parameters is empty.");
		return result;
	}

	AIShaderEditingResult edit_result = service->set_parameters(node_path, target_property, shader_parameters);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to set shader parameters.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:shader.set_parameters] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:shader.set_parameters] Completed. %s", result.content));
	return result;
}
