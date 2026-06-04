/**************************************************************************/
/*  ai_model_runtime_options.h                                            */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

struct AIModelRuntimeOptions {
	String api_format = "openai_chat_completions";
	bool supports_multimodal = false;
	int timeout_seconds = 180;
	int max_input_chars = 96000;
	int max_context_chars = 24000;
	int max_history_chars = 64000;
	int max_tool_result_chars = 16000;
	int max_multimodal_files = 4;
	int max_multimodal_file_bytes = 20 * 1024 * 1024;
	int min_recent_messages = 4;
	int max_provider_turns = 255;
	int max_tool_calls = 60;
	int max_output_tokens = 0;
};
