/**************************************************************************/
/*  ai_scene_save_current_scene_tool.cpp                                  */
/**************************************************************************/

#include "ai_scene_save_current_scene_tool.h"

#include "core/variant/variant.h"

AISceneSaveCurrentSceneTool::AISceneSaveCurrentSceneTool() {
	service.instantiate();
}

String AISceneSaveCurrentSceneTool::get_name() const {
	return "scene.save_current_scene";
}

String AISceneSaveCurrentSceneTool::get_description() const {
	return "Saves the currently edited scene through editor APIs.";
}

Dictionary AISceneSaveCurrentSceneTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";
	schema["properties"] = Dictionary();
	return schema;
}

AIToolResult AISceneSaveCurrentSceneTool::execute(const Dictionary &p_arguments) {
	(void)p_arguments;
	AIToolResult result;
	print_line("[AI Agent][Tool:scene.save_current_scene] Start.");

	AISceneEditingResult edit_result = service->save_current_scene();
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to save current scene.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.save_current_scene] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.save_current_scene] Completed. %s", result.content));
	return result;
}
