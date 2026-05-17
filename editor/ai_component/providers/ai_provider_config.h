/**************************************************************************/
/*  ai_provider_config.h                                                   */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

struct AIProviderConfig {
	String provider_name = "DeepSeek";
	String base_url = "https://api.deepseek.com";
	String api_key;
	String model = "deepseek-chat";
	int timeout_seconds = 180;
};
