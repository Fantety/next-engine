/**************************************************************************/
/*  ai_openai_compatible_provider.h                                       */
/**************************************************************************/

#pragma once

#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/os/safe_binary_mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"

#include "editor/ai_component/providers/ai_agent_provider.h"
#include "editor/ai_component/providers/ai_sse_parser.h"

class AIOpenAICompatibleProvider : public AIAgentProvider {
	GDCLASS(AIOpenAICompatibleProvider, AIAgentProvider);

	struct ThreadParams {
		AIOpenAICompatibleProvider *self = nullptr;
		Array messages;
		AIProviderConfig config;
	};

	Thread thread;
	SafeFlag cancel_requested;
	SafeFlag running;
	Ref<JSON> json;

	static void _thread_func(void *p_userdata);
	static PackedByteArray _build_body(const Array &p_messages, const String &p_model);
	static bool _extract_delta(const String &p_event, String &r_delta, String &r_finish_reason, String &r_error);
	static bool _is_valid_utf8(const uint8_t *p_data, int p_length);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIOpenAICompatibleProvider();
	~AIOpenAICompatibleProvider();

	static String build_request_path(const String &p_base_path);

	bool start_chat(const Array &p_messages) override;
	void cancel() override;
};
