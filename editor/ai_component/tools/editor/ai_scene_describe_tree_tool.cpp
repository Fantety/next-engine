/**************************************************************************/
/*  ai_scene_describe_tree_tool.cpp                                       */
/**************************************************************************/

#include "ai_scene_describe_tree_tool.h"

#include "core/variant/variant.h"

AISceneDescribeTreeTool::AISceneDescribeTreeTool() {
	service.instantiate();
}

String AISceneDescribeTreeTool::get_name() const {
	return "scene.describe_tree";
}

String AISceneDescribeTreeTool::get_description() const {
	return "Returns a compact tree of the currently edited scene with relative node paths, names, types, child counts, and ai_node_id metadata when present.";
}

Dictionary AISceneDescribeTreeTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary root_path_property;
	root_path_property["type"] = "string";
	root_path_property["description"] = "Optional node path relative to the edited scene root. Use . for the root. Defaults to .";
	properties["root_path"] = root_path_property;

	Dictionary max_depth_property;
	max_depth_property["type"] = "integer";
	max_depth_property["description"] = "Maximum depth to return. Defaults to 8 and is clamped to 0..32.";
	properties["max_depth"] = max_depth_property;

	Dictionary max_nodes_property;
	max_nodes_property["type"] = "integer";
	max_nodes_property["description"] = "Maximum nodes to return. Defaults to 200 and is clamped to 1..1000.";
	properties["max_nodes"] = max_nodes_property;

	Dictionary include_internal_property;
	include_internal_property["type"] = "boolean";
	include_internal_property["description"] = "Whether to include internal children. Defaults to false.";
	properties["include_internal"] = include_internal_property;

	schema["properties"] = properties;
	return schema;
}

AIToolResult AISceneDescribeTreeTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	const String root_path = String(p_arguments.get("root_path", ".")).strip_edges();
	const int max_depth = int(p_arguments.get("max_depth", 8));
	const int max_nodes = int(p_arguments.get("max_nodes", 200));
	const bool include_internal = bool(p_arguments.get("include_internal", false));
	print_line(vformat("[AI Agent][Tool:scene.describe_tree] Start. root_path=%s max_depth=%d max_nodes=%d include_internal=%s", root_path, max_depth, max_nodes, include_internal ? "yes" : "no"));

	AISceneEditingResult edit_result = service->describe_tree(root_path, max_depth, max_nodes, include_internal);
	if (!edit_result.success) {
		result.error = edit_result.error.is_empty() ? String("Failed to describe scene tree.") : edit_result.error;
		result.metadata = edit_result.metadata;
		print_line(vformat("[AI Agent][Tool:scene.describe_tree] Failed: %s", result.error));
		return result;
	}

	result.content = edit_result.message;
	result.metadata = edit_result.metadata;
	print_line(vformat("[AI Agent][Tool:scene.describe_tree] Completed. nodes=%d", int(result.metadata.get("node_count", 0))));
	return result;
}
