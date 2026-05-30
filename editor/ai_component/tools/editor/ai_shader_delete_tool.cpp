/**************************************************************************/
/*  ai_shader_delete_tool.cpp                                             */
/**************************************************************************/

#include "ai_shader_delete_tool.h"

AIShaderDeleteTool::AIShaderDeleteTool() {
	service.instantiate();
}

String AIShaderDeleteTool::get_name() const {
	return "shader.delete";
}

String AIShaderDeleteTool::get_description() const {
	return "Deletes a .gdshader resource under res://. This tool must be explicitly approved by the user.";
}

Dictionary AIShaderDeleteTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary path_property;
	path_property["type"] = "string";
	path_property["description"] = ".gdshader path under res:// to delete.";
	properties["path"] = path_property;

	Array required;
	required.push_back("path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIShaderDeleteTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String path = String(p_arguments.get("path", "")).strip_edges();
	print_line(vformat("[AI Agent][Tool:shader.delete] Start. path=%s", path));

	if (path.is_empty()) {
		result.error = "Missing required path.";
		print_line("[AI Agent][Tool:shader.delete] Failed: missing required path.");
		return result;
	}

	AIShaderEditingResult edit_result = service->delete_shader(path);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to delete shader.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:shader.delete] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:shader.delete] Completed. %s", result.content));
	return result;
}
