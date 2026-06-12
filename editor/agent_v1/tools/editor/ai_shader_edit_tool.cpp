/**************************************************************************/
/*  ai_shader_edit_tool.cpp                                               */
/**************************************************************************/

#include "ai_shader_edit_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1ShaderEditTool::AIV1ShaderEditTool() {
	service.instantiate();
}

String AIV1ShaderEditTool::get_name() const {
	return "shader.edit";
}

String AIV1ShaderEditTool::get_description() const {
	return "Edits an existing .gdshader resource through the editor resource save flow.";
}

Dictionary AIV1ShaderEditTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property("Existing .gdshader path under res://.");
	properties["shader_code"] = AIV1ToolHelpers::make_string_property("Complete Godot shader source. Include the shader_type line.");

	Array required;
	required.push_back("path");
	required.push_back("shader_code");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ShaderEditTool::execute_tool(const Dictionary &p_arguments) {
	const String path = AIV1ToolHelpers::get_stripped_string(p_arguments, "path");
	const String shader_code = String(p_arguments.get("shader_code", ""));
	print_line(vformat("[AI Agent][Tool:shader.edit] Start. path=%s source_chars=%d", path, shader_code.length()));

	if (path.is_empty()) {
		print_line("[AI Agent][Tool:shader.edit] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}
	if (shader_code.strip_edges().is_empty()) {
		print_line("[AI Agent][Tool:shader.edit] Failed: missing required shader_code.");
		return AIV1ToolHelpers::make_missing_required_error("shader_code");
	}

	AIV1ShaderEditingResult edit_result = service->edit_shader(path, shader_code);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to edit shader.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:shader.edit] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:shader.edit] Completed. %s", result.content));
	return result;
}
