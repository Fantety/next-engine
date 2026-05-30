/**************************************************************************/
/*  ai_shader_edit_tool.cpp                                               */
/**************************************************************************/

#include "ai_shader_edit_tool.h"

AIShaderEditTool::AIShaderEditTool() {
	service.instantiate();
}

String AIShaderEditTool::get_name() const {
	return "shader.edit";
}

String AIShaderEditTool::get_description() const {
	return "Edits an existing .gdshader resource through the editor resource save flow.";
}

Dictionary AIShaderEditTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = "Existing .gdshader path under res://.";
	properties["path"] = path_property;

	Dictionary shader_code_property;
	shader_code_property["type"] = "string";
	shader_code_property["description"] = "Complete Godot shader source. Include the shader_type line.";
	properties["shader_code"] = shader_code_property;

	Array required;
	required.push_back("path");
	required.push_back("shader_code");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIShaderEditTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	const String shader_code = String(p_arguments.get("shader_code", ""));
	print_line(vformat("[AI Agent][Tool:shader.edit] Start. path=%s source_chars=%d", path, shader_code.length()));

	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:shader.edit] Failed: missing required path.");
		return result;
	}
	if (shader_code.strip_edges().is_empty()) {
		result.error = "Missing required shader_code.";
		print_line("[AI Agent][Tool:shader.edit] Failed: missing required shader_code.");
		return result;
	}

	AIShaderEditingResult edit_result = service->edit_shader(path, shader_code);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to edit shader.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:shader.edit] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:shader.edit] Completed. %s", result.content));
	return result;
}
