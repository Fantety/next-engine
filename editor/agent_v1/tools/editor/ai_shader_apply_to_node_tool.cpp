/**************************************************************************/
/*  ai_shader_apply_to_node_tool.cpp                                      */
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

#include "ai_shader_apply_to_node_tool.h"

#include "core/variant/variant.h"
#include "editor/agent_v1/tools/ai_editor_tools_v1.h"
#include "editor/next_file_logger.h"

AIV1ShaderApplyToNodeTool::AIV1ShaderApplyToNodeTool() {
	service.instantiate();
}

String AIV1ShaderApplyToNodeTool::get_name() const {
	return "shader.apply_to_node";
}

String AIV1ShaderApplyToNodeTool::get_description() const {
	return "Applies an existing .gdshader to an explicitly specified Shader or ShaderMaterial node property and saves the current scene.";
}

Dictionary AIV1ShaderApplyToNodeTool::get_parameters_schema() const {
	Dictionary properties;
	properties["node_path"] = AIV1ToolHelpers::make_string_property("Target node path relative to the edited scene root, or . for the root node.");
	properties["shader_path"] = AIV1ToolHelpers::make_string_property("Project path for the .gdshader file, for example res://shaders/player_flash.gdshader.");
	properties["target_property"] = AIV1ToolHelpers::make_string_property("Required target property path on the node, for example material, material_override, shader, "
																		  "shader_override, or surface_material_override/0.");

	Dictionary parameters_property;
	parameters_property["type"] = "object";
	parameters_property["description"] = "Optional shader uniform values keyed by uniform name. Basic values can use JSON scalars/arrays or typed values such as {\"type\":\"Color\",\"args\":[1,0,0,1]}. Resource uniforms such as textures/noise accept {\"resource_path\":\"res://...\"} or {\"resource_type\":\"NoiseTexture2D\",\"properties\":{\"noise\":{\"resource_type\":\"FastNoiseLite\",\"properties\":{\"seed\":1}}}}.";
	properties["shader_parameters"] = parameters_property;

	Array required;
	required.push_back("node_path");
	required.push_back("shader_path");
	required.push_back("target_property");
	return AIV1ToolHelpers::make_object_schema(properties, required);
}

AIV1EditorToolResult AIV1ShaderApplyToNodeTool::execute_tool(const Dictionary &p_arguments) {
	const String node_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "node_path");
	const String shader_path = AIV1ToolHelpers::get_stripped_string(p_arguments, "shader_path");
	const String target_property = AIV1ToolHelpers::get_stripped_string(p_arguments, "target_property");
	Dictionary shader_parameters;
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.apply_to_node] Start. node_path=%s shader_path=%s target_property=%s", node_path, shader_path, target_property));

	if (node_path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.apply_to_node] Failed: missing required node_path.");
		return AIV1ToolHelpers::make_missing_required_error("node_path");
	}
	if (shader_path.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.apply_to_node] Failed: missing required shader_path.");
		return AIV1ToolHelpers::make_missing_required_error("shader_path");
	}
	if (target_property.is_empty()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.apply_to_node] Failed: missing required target_property.");
		return AIV1ToolHelpers::make_missing_required_error("target_property");
	}
	if (p_arguments.has("shader_parameters")) {
		if (Variant(p_arguments["shader_parameters"]).get_type() != Variant::DICTIONARY) {
			AIV1EditorToolResult result;
			result.error = "shader_parameters must be an object.";
			NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:shader.apply_to_node] Failed: shader_parameters is not an object.");
			return result;
		}
		shader_parameters = p_arguments["shader_parameters"];
	}

	AIV1ShaderEditingResult edit_result = service->apply_to_node(node_path, shader_path, target_property, shader_parameters);
	AIV1EditorToolResult result = AIV1ToolHelpers::from_editing_result(edit_result, "Failed to apply shader to node.");
	if (result.is_error()) {
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.apply_to_node] Failed: %s", result.error));
		return result;
	}

	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:shader.apply_to_node] Completed. %s", result.content));
	return result;
}
