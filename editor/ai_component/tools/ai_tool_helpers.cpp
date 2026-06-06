/**************************************************************************/
/*  ai_tool_helpers.cpp                                                   */
/**************************************************************************/

#include "ai_tool_helpers.h"

namespace AIToolHelpers {

Dictionary make_string_property(const String &p_description) {
	Dictionary property;
	property["type"] = "string";
	property["description"] = p_description;
	return property;
}

Dictionary make_boolean_property(const String &p_description) {
	Dictionary property;
	property["type"] = "boolean";
	property["description"] = p_description;
	return property;
}

Dictionary make_object_schema(const Dictionary &p_properties, const Array &p_required) {
	Dictionary schema;
	schema["type"] = "object";
	schema["properties"] = p_properties;
	if (!p_required.is_empty()) {
		schema["required"] = p_required;
	}
	return schema;
}

String get_stripped_string(const Dictionary &p_arguments, const String &p_key, const String &p_default) {
	return String(p_arguments.get(p_key, p_default)).strip_edges();
}

bool get_bool(const Dictionary &p_arguments, const String &p_key, bool p_default) {
	return bool(p_arguments.get(p_key, p_default));
}

AIToolResult make_missing_required_error(const String &p_key) {
	AIToolResult result;
	result.error = "Missing required " + p_key + ".";
	return result;
}

} // namespace AIToolHelpers
