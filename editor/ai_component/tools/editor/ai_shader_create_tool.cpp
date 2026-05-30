/**************************************************************************/
/*  ai_shader_create_tool.cpp                                             */
/**************************************************************************/

#include "ai_shader_create_tool.h"

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
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Target .gdshader path under res://.";
	properties["path"] = path_property;

	Dictionary shader_type_property;
	shader_type_property["type"] = "string";
	shader_type_property["description"] = "Shader type used when shader_code is omitted. Defaults to canvas_item.";
	properties["shader_type"] = shader_type_property;

	Dictionary shader_code_property;
	shader_code_property["type"] = "string";
	shader_code_property["description"] = "Optional initial Godot shader source. Include the shader_type line if provided.";
	properties["shader_code"] = shader_code_property;

	Dictionary overwrite_property;
	overwrite_property["type"] = "boolean";
	overwrite_property["description"] = "Whether to overwrite an existing shader. Defaults to false.";
	properties["overwrite"] = overwrite_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIShaderCreateTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	const String shader_type = String(p_arguments.get("shader_type", "canvas_item")).strip_edges();
	const String shader_code = String(p_arguments.get("shader_code", ""));
	const bool overwrite = bool(p_arguments.get("overwrite", false));
	print_line(vformat("[AI Agent][Tool:shader.create] Start. path=%s shader_type=%s overwrite=%s", path, shader_type, overwrite ? "yes" : "no"));

	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:shader.create] Failed: missing required path.");
		return result;
	}

	AIShaderEditingResult edit_result = service->create_shader(path, shader_type, shader_code, overwrite);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to create shader.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:shader.create] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:shader.create] Completed. %s", result.content));
	return result;
}
