/**************************************************************************/
/*  ai_provider_tool_exposure_policy.cpp                                  */
/**************************************************************************/

#include "ai_provider_tool_exposure_policy.h"

#include "core/variant/variant.h"

static const char *MULTIMODAL_FILE_TOOL_NAME = "project.attach_multimodal_file";

bool AIProviderToolExposurePolicy::should_expose_tool_schema(const Dictionary &p_tool_schema, const AIProviderConfig &p_config) {
	if (!p_tool_schema.has("function") || Variant(p_tool_schema["function"]).get_type() != Variant::DICTIONARY) {
		return true;
	}

	Dictionary function = p_tool_schema["function"];
	const String tool_name = String(function.get("name", ""));
	if (tool_name == MULTIMODAL_FILE_TOOL_NAME && !p_config.supports_multimodal) {
		return false;
	}

	return true;
}
