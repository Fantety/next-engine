/**************************************************************************/
/*  ai_shader_create_tool.cpp                                             */
/**************************************************************************/

#include "ai_shader_create_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

AIShaderCreateTool::AIShaderCreateTool() {
	service.instantiate();
}

String AIShaderCreateTool::get_name() const {
	return "shader.create";
}

String AIShaderCreateTool::get_description() const {
	return "Creates a .gdshader resource through the editor resource save flow.";
}

Dictionary AIShaderCreateTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIToolHelpers::make_string_property("Target .gdshader path under res://.");
	properties["shader_type"] = AIToolHelpers::make_string_property("Shader type used when shader_code is omitted. Defaults to canvas_item.");
	properties["shader_code"] = AIToolHelpers::make_string_property("Optional initial Godot shader source. Include the shader_type line if provided.");
	properties["overwrite"] = AIToolHelpers::make_boolean_property("Whether to overwrite an existing shader. Defaults to false.");

	Array required;
	required.push_back("path");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIShaderCreateTool::execute(const Dictionary &p_arguments) {
	const String path = AIToolHelpers::get_stripped_string(p_arguments, "path");
	const String shader_type = AIToolHelpers::get_stripped_string(p_arguments, "shader_type", "canvas_item");
	const String shader_code = String(p_arguments.get("shader_code", ""));
	const bool overwrite = AIToolHelpers::get_bool(p_arguments, "overwrite");
	print_line(vformat("[AI Agent][Tool:shader.create] Start. path=%s shader_type=%s overwrite=%s", path, shader_type, overwrite ? "yes" : "no"));

	if (path.is_empty()) {
		print_line("[AI Agent][Tool:shader.create] Failed: missing required path.");
		return AIToolHelpers::make_missing_required_error("path");
	}

	AIShaderEditingResult edit_result = service->create_shader(path, shader_type, shader_code, overwrite);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to create shader.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:shader.create] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:shader.create] Completed. %s", result.content));
	return result;
}
