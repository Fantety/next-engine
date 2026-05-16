/**************************************************************************/
/*  ai_tool.cpp                                                           */
/**************************************************************************/

#include "ai_tool.h"

#include "core/variant/variant.h"

void AITool::_bind_methods() {
}

bool AIToolResult::is_error() const {
	return !error.is_empty();
}

Dictionary AIToolResult::to_dict() const {
	Dictionary dict;
	dict["content"] = content;
	dict["error"] = error;
	dict["metadata"] = metadata;
	dict["truncated"] = truncated;
	return dict;
}

AIToolResult AIToolResult::from_dict(const Dictionary &p_dict) {
	AIToolResult result;
	result.content = p_dict.get("content", String());
	result.error = p_dict.get("error", String());
	if (p_dict.has("metadata") && Variant(p_dict["metadata"]).get_type() == Variant::DICTIONARY) {
		result.metadata = p_dict["metadata"];
	}
	result.truncated = p_dict.get("truncated", false);
	return result;
}

Dictionary AITool::get_openai_schema() const {
	Dictionary function;
	function["name"] = get_name();
	function["description"] = get_description();
	function["parameters"] = get_parameters_schema();

	Dictionary schema;
	schema["type"] = "function";
	schema["function"] = function;
	return schema;
}
