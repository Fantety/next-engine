/**************************************************************************/
/*  ai_scene_inspect_node_tool.cpp                                        */
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

#include "ai_scene_inspect_node_tool.h"

#include "core/variant/variant.h"
#include "editor/next_file_logger.h"

AIV1SceneInspectNodeTool::AIV1SceneInspectNodeTool() {
	service.instantiate();
}

String AIV1SceneInspectNodeTool::get_name() const {
	return "scene.inspect_node";
}

String AIV1SceneInspectNodeTool::get_description() const {
	return "Reads exact current values for selected properties on a node in the currently edited scene. Use after scene.apply_patch to verify layout, text, visibility, anchors, offsets, size, position, and other user-visible changes before claiming completion.";
}

Dictionary AIV1SceneInspectNodeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary node_path_property;
	node_path_property["type"] = "string";
	node_path_property["description"] = "Node path relative to the edited scene root. Use . for the scene root.";
	properties["node_path"] = node_path_property;

	Dictionary properties_property;
	properties_property["type"] = "array";
	properties_property["description"] = "Optional list of exact property paths to read. Omit for common layout/visibility/text properties for the node type.";
	Dictionary item_schema;
	item_schema["type"] = "string";
	properties_property["items"] = item_schema;
	properties["properties"] = properties_property;

	Array required;
	required.push_back("node_path");
	schema["required"] = required;
	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1SceneInspectNodeTool::execute_tool(const Dictionary &p_arguments) {
	AIV1EditorToolResult result;
	const String node_path = String(p_arguments.get("node_path", "")).strip_edges();
	Array property_paths;
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:scene.inspect_node] Start. node_path=%s", node_path));

	if (node_path.is_empty()) {
		result.error = "Missing required node_path.";
		NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:scene.inspect_node] Failed: missing required node_path.");
		return result;
	}
	if (p_arguments.has("properties")) {
		if (Variant(p_arguments["properties"]).get_type() != Variant::ARRAY) {
			result.error = "properties must be an array of property path strings.";
			NEXT_FILE_LOG_DEBUG("AI Agent", "[AI Agent][Tool:scene.inspect_node] Failed: properties is not an array.");
			return result;
		}
		property_paths = p_arguments["properties"];
		for (int i = 0; i < property_paths.size(); i++) {
			const Variant::Type item_type = Variant(property_paths[i]).get_type();
			if (item_type != Variant::STRING && item_type != Variant::STRING_NAME && item_type != Variant::NODE_PATH) {
				result.error = vformat("properties[%d] must be a property path string.", i);
				NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:scene.inspect_node] Failed: %s", result.error));
				return result;
			}
			if (String(property_paths[i]).strip_edges().is_empty()) {
				result.error = vformat("properties[%d] must not be empty.", i);
				NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:scene.inspect_node] Failed: %s", result.error));
				return result;
			}
		}
	}

	AIV1SceneEditingResult edit_result = service->inspect_node(node_path, property_paths);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to inspect node.") : edit_result.error;
		result.metadata = edit_result.metadata;
		NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:scene.inspect_node] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	NEXT_FILE_LOG_DEBUG("AI Agent", vformat("[AI Agent][Tool:scene.inspect_node] Completed. properties=%d", int(result.metadata.get("property_count", 0))));
	return result;
}
