/**************************************************************************/
/*  ai_tool_helpers.h                                                     */
/**************************************************************************/

#pragma once

#include "editor/ai_component/tools/ai_tool.h"

namespace AIToolHelpers {

Dictionary make_string_property(const String &p_description);
Dictionary make_boolean_property(const String &p_description);
Dictionary make_object_schema(const Dictionary &p_properties, const Array &p_required = Array());
String get_stripped_string(const Dictionary &p_arguments, const String &p_key, const String &p_default = String());
bool get_bool(const Dictionary &p_arguments, const String &p_key, bool p_default = false);
AIToolResult make_missing_required_error(const String &p_key);

template <typename TEditingResult>
AIToolResult from_editing_result(const TEditingResult &p_editing_result, const String &p_fallback_error) {
	AIToolResult result;
	result.metadata = p_editing_result.metadata;
	if (!p_editing_result.success) {
		result.error = p_editing_result.error.is_empty() ? p_fallback_error : p_editing_result.error;
		return result;
	}

	result.content = p_editing_result.message;
	return result;
}

} // namespace AIToolHelpers
