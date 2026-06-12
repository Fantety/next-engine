/**************************************************************************/
/*  ai_shader_delete_tool.cpp                                             */
/**************************************************************************/

#include "ai_shader_delete_tool.h"

#include "editor/agent_v1/tools/ai_editor_tools_v1.h"

AIV1ShaderDeleteTool::AIV1ShaderDeleteTool() {
	service.instantiate();
}

String AIV1ShaderDeleteTool::get_name() const {
	return "shader.delete";
}

String AIV1ShaderDeleteTool::get_description() const {
	return "Deletes a .gdshader resource under res://. This tool must be explicitly approved by the user.";
}

Dictionary AIV1ShaderDeleteTool::get_parameters_schema() const {
	Dictionary properties;
	properties["path"] = AIV1ToolHelpers::make_string_property(".gdshader path under res:// to delete.");

	Array required;
	required.push_back("path");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ShaderDeleteTool::execute_tool(const Dictionary &p_arguments) {
	const String path = AIV1ToolHelpers::get_stripped_string(p_arguments, "path");
	print_line(vformat("[AI Agent][Tool:shader.delete] Start. path=%s", path));

	if (path.is_empty()) {
		print_line("[AI Agent][Tool:shader.delete] Failed: missing required path.");
		return AIV1ToolHelpers::make_missing_required_error("path");
	}

	AIV1ShaderEditingResult edit_result = service->delete_shader(path);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to delete shader.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:shader.delete] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:shader.delete] Completed. %s", result.content));
	return result;
}
