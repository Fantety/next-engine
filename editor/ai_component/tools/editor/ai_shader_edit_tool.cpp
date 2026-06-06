/**************************************************************************/
/*  ai_shader_edit_tool.cpp                                               */
/**************************************************************************/

#include "ai_shader_edit_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

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
	Dictionary properties;
	properties["path"] = AIToolHelpers::make_string_property("Existing .gdshader path under res://.");
	properties["shader_code"] = AIToolHelpers::make_string_property("Complete Godot shader source. Include the shader_type line.");

	Array required;
	required.push_back("path");
	required.push_back("shader_code");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIShaderEditTool::execute(const Dictionary &p_arguments) {
	const String path = AIToolHelpers::get_stripped_string(p_arguments, "path");
	const String shader_code = String(p_arguments.get("shader_code", ""));
	print_line(vformat("[AI Agent][Tool:shader.edit] Start. path=%s source_chars=%d", path, shader_code.length()));

	if (path.is_empty()) {
		print_line("[AI Agent][Tool:shader.edit] Failed: missing required path.");
		return AIToolHelpers::make_missing_required_error("path");
	}
	if (shader_code.strip_edges().is_empty()) {
		print_line("[AI Agent][Tool:shader.edit] Failed: missing required shader_code.");
		return AIToolHelpers::make_missing_required_error("shader_code");
	}

	AIShaderEditingResult edit_result = service->edit_shader(path, shader_code);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to edit shader.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:shader.edit] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:shader.edit] Completed. %s", result.content));
	return result;
}
