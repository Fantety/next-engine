/**************************************************************************/
/*  ai_shader_set_parameters_tool.cpp                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "ai_shader_set_parameters_tool.h"

#include "core/variant/variant.h"
#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/next_file_logger.h"

AIV1ShaderSetParametersTool::AIV1ShaderSetParametersTool() {
	service.instantiate();
}

String AIV1ShaderSetParametersTool::get_name() const {
	return "shader.set_parameters";
}

String AIV1ShaderSetParametersTool::get_description() const {
	return "Sets uniform values on an existing ShaderMaterial node property and saves the current scene.";
}

Dictionary AIV1ShaderSetParametersTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIV1ToolHelpers::make_string_property("Target node path relative to the edited scene root, or . for the root node.");
	properties["target_property"] = AIV1ToolHelpers::make_string_property("ShaderMaterial property path on the node, for example material, material_override, or surface_material_override/0.");

	Dictionary parameters_property;
	parameters_property["type"] = "object";
	parameters_property["description"] = "Shader uniform values keyed by uniform name. Basic values can use JSON scalars/arrays or typed values such as {\"type\":\"Color\",\"args\":[1,0,0,1]}. Resource uniforms such as textures/noise accept {\"resource_path\":\"res://...\"} or {\"resource_type\":\"NoiseTexture2D\",\"properties\":{\"noise\":{\"resource_type\":\"FastNoiseLite\",\"properties\":{\"seed\":1}}}}.";
	properties["shader_parameters"] = parameters_property;

	Array required;
	required.push_back("node_path");
	required.push_back("target_property");
	required.push_back("shader_parameters");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ShaderSetParametersTool::execute_tool(const Dictionary &p_arguments) {
	const String node_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "node_path");
	const String target_property = AIV1ToolHelpers::get_stripped_string(p_arguments, "target_property");
	Dictionary shader_parameters;
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.set_parameters] Start. node_path=%s target_property=%s", node_path, target_property));

	if (node_path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.set_parameters] Failed: missing required node_path.");
		return AIV1ToolHelpers::make_missing_required_error("node_path");
	}
	if (target_property.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.set_parameters] Failed: missing required target_property.");
		return AIV1ToolHelpers::make_missing_required_error("target_property");
	}
	if (!p_arguments.has("shader_parameters")) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.set_parameters] Failed: missing required shader_parameters.");
		return AIV1ToolHelpers::make_missing_required_error("shader_parameters");
	}
	if (Variant(p_arguments["shader_parameters"]).get_type() != Variant::DICTIONARY) {
		AIV1EditorToolResult result;
		result.error = "shader_parameters must be an object.";
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.set_parameters] Failed: shader_parameters is not an object.");
		return result;
	}
	shader_parameters = p_arguments["shader_parameters"];
	if (shader_parameters.is_empty()) {
		AIV1EditorToolResult result;
		result.error = "shader_parameters must include at least one uniform value.";
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.set_parameters] Failed: shader_parameters is empty.");
		return result;
	}

	AIV1ShaderEditingResult edit_result = service->set_parameters(node_path, target_property, shader_parameters);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to set shader parameters.");
	if (result.is_error()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.set_parameters] Failed: %s", result.error));
		return result;
	}

	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.set_parameters] Completed. %s", result.content));
	return result;
}
