/**************************************************************************/
/*  ai_provider_config.h                                                   */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

#include "editor/ai_component/providers/ai_model_runtime_options.h"

struct AIProviderConfig : public AIModelRuntimeOptions {
	String provider_name = "DeepSeek";
	String base_url = "https://api.deepseek.com";
	String api_key;
	String model = "deepseek-chat";
};
