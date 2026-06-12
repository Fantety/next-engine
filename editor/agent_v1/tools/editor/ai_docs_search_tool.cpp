/**************************************************************************/
/*  ai_docs_search_tool.cpp                                               */
/**************************************************************************/

#include "ai_docs_search_tool.h"

#include "core/variant/variant.h"

AIV1DocsSearchTool::AIV1DocsSearchTool() {
	service.instantiate();
}

String AIV1DocsSearchTool::get_name() const {
	return "docs.search";
}

String AIV1DocsSearchTool::get_description() const {
	return "Searches Godot's built-in class documentation and returns structured class, property, method, signal, constant, enum, and theme item results.";
}

Dictionary AIV1DocsSearchTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	Dictionary query_property;
	query_property["type"] = "string";
	query_property["description"] = "Search text. Use exact or approximate Godot API names such as horizontal_alignment, VBoxContainer, pressed, or size_flags_horizontal.";
	properties["query"] = query_property;

	Dictionary class_name_property;
	class_name_property["type"] = "string";
	class_name_property["description"] = "Optional class name to restrict results, for example Label, VBoxContainer, Control, Node2D, or Input.";
	properties["class_name"] = class_name_property;

	Dictionary kind_property;
	kind_property["type"] = "string";
	kind_property["description"] = "Optional result kind: all, class, property, method, signal, constant, theme_item, enum, constructor, operator, or annotation. Defaults to all.";
	Array kind_enum;
	kind_enum.push_back("all");
	kind_enum.push_back("class");
	kind_enum.push_back("property");
	kind_enum.push_back("method");
	kind_enum.push_back("signal");
	kind_enum.push_back("constant");
	kind_enum.push_back("theme_item");
	kind_enum.push_back("enum");
	kind_enum.push_back("constructor");
	kind_enum.push_back("operator");
	kind_enum.push_back("annotation");
	kind_property["enum"] = kind_enum;
	properties["kind"] = kind_property;

	Dictionary max_results_property;
	max_results_property["type"] = "integer";
	max_results_property["description"] = "Maximum number of results to return. Defaults to 20 and is clamped to 1..80.";
	properties["max_results"] = max_results_property;

	Dictionary include_descriptions_property;
	include_descriptions_property["type"] = "boolean";
	include_descriptions_property["description"] = "Whether to include short cleaned documentation descriptions. Defaults to true.";
	properties["include_descriptions"] = include_descriptions_property;

	schema["properties"] = properties;
	return schema;
}

AIV1EditorToolResult AIV1DocsSearchTool::execute_tool(const Dictionary &p_arguments) {
	AIV1EditorToolResult result;
	const String query = String(p_arguments.get("query", "")).strip_edges();
	const String class_name = String(p_arguments.get("class_name", "")).strip_edges();
	const String kind = String(p_arguments.get("kind", "all")).strip_edges();
	const int max_results = int(p_arguments.get("max_results", 20));
	const bool include_descriptions = bool(p_arguments.get("include_descriptions", true));
	print_line(vformat("[AI Agent][Tool:docs.search] Start. query=%s class_name=%s kind=%s max_results=%d include_descriptions=%s", query, class_name, kind, max_results, include_descriptions ? "yes" : "no"));

	if (query.is_empty() && class_name.is_empty()) {
		result.error = "Provide query, class_name, or both.";
		print_line("[AI Agent][Tool:docs.search] Failed: missing query and class_name.");
		return result;
	}

	AIV1DocumentationResult doc_result = service->search(query, class_name, kind, max_results, include_descriptions);
	if (!doc_result.success) {
		result.error = doc_result.error.is_empty() ? String("Failed to search Godot documentation.") : doc_result.error;
		result.metadata = doc_result.metadata;
		print_line(vformat("[AI Agent][Tool:docs.search] Failed: %s", result.error));
		return result;
	}

	result.content = doc_result.message;
	result.metadata = doc_result.metadata;
	print_line(vformat("[AI Agent][Tool:docs.search] Completed. results=%d", int(result.metadata.get("result_count", 0))));
	return result;
}
