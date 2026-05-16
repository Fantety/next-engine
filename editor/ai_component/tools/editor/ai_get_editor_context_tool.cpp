/**************************************************************************/
/*  ai_get_editor_context_tool.cpp                                        */
/**************************************************************************/

#include "ai_get_editor_context_tool.h"

#include "core/config/project_settings.h"
#include "core/version.h"
#include "core/variant/variant.h"

String AIGetEditorContextTool::get_name() const {
	return "editor.get_context";
}

String AIGetEditorContextTool::get_description() const {
	return "Returns safe, read-only metadata about the current Godot editor project.";
}

Dictionary AIGetEditorContextTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	schema["properties"] = properties;
	return schema;
}

AIToolResult AIGetEditorContextTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	Dictionary context_metadata;

	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	String resource_path;
	String application_name;
	if (project_settings) {
		resource_path = project_settings->get_resource_path();
		application_name = GLOBAL_GET("application/config/name");
	}

	context_metadata["resource_path"] = resource_path;
	context_metadata["application_name"] = application_name;
	context_metadata["engine_name"] = String(VERSION_NAME);
	context_metadata["engine_version"] = String(VERSION_FULL_CONFIG);
	context_metadata["capabilities"] = "read-only";

	String content;
	content += "Editor Context\n";
	content += "Engine: " + String(VERSION_NAME) + " " + String(VERSION_FULL_CONFIG) + "\n";
	content += "Project name: " + (application_name.is_empty() ? String("<unnamed>") : application_name) + "\n";
	content += "Project path: " + (resource_path.is_empty() ? String("<unknown>") : resource_path) + "\n";
	content += "Agent capabilities: read-only\n";

	result.content = content;
	result.metadata = context_metadata;
	return result;
}
