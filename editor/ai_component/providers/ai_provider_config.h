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
	int max_input_chars = 96000;
	int max_context_chars = 24000;
	int max_history_chars = 64000;
	int max_tool_result_chars = 16000;
	int min_recent_messages = 4;
	int max_provider_turns = 255;
	int max_tool_calls = 60;
	int max_output_tokens = 0;
};
