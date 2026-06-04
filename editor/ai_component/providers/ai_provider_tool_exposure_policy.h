/**************************************************************************/
/*  ai_provider_tool_exposure_policy.h                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include "editor/ai_component/providers/ai_provider_config.h"

class AIProviderToolExposurePolicy {
public:
	static bool should_expose_tool_schema(const Dictionary &p_tool_schema, const AIProviderConfig &p_config);
};
