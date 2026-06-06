/**************************************************************************/
/*  ai_shader_delete_tool.cpp                                             */
/**************************************************************************/

#include "ai_shader_delete_tool.h"

#include "editor/ai_component/tools/ai_tool_helpers.h"

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
	Dictionary properties;
	properties["path"] = AIToolHelpers::make_string_property(".gdshader path under res:// to delete.");

	Array required;
	required.push_back("path");
	return AIToolHelpers::make_object_schema(properties, required);
}

AIToolResult AIShaderDeleteTool::execute(const Dictionary &p_arguments) {
	const String path = AIToolHelpers::get_stripped_string(p_arguments, "path");
	print_line(vformat("[AI Agent][Tool:shader.delete] Start. path=%s", path));

	if (path.is_empty()) {
		print_line("[AI Agent][Tool:shader.delete] Failed: missing required path.");
		return AIToolHelpers::make_missing_required_error("path");
	}

	AIShaderEditingResult edit_result = service->delete_shader(path);
	AIToolResult result = AIToolHelpers::from_editing_result(edit_result, "Failed to delete shader.");
	if (result.is_error()) {
		print_line(vformat("[AI Agent][Tool:shader.delete] Failed: %s", result.error));
		return result;
	}

	print_line(vformat("[AI Agent][Tool:shader.delete] Completed. %s", result.content));
	return result;
}
