/**************************************************************************/
/*  ai_openai_compatible_runtime.h                                        */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/transport/ai_http_client.h"
#include "editor/agent_v1/runtime/ai_llm_runtime.h"

class AIOpenAICompatibleRuntime : public AILLMRuntime {
	GDCLASS(AIOpenAICompatibleRuntime, AILLMRuntime);

	String base_url = "https://api.openai.com/v1";
	String api_key;
	String organization;
	int timeout_msec = 120000;
	Ref<AIHTTPClient> http_client;

	static String _chat_completions_url(const String &p_base_url);
	static String _message_text(const AIModelMessage &p_message);
	static String _data_url_payload(const String &p_data_url);
	static String _audio_format_from_mime(const String &p_mime);
	static Dictionary _part_to_openai(const AIModelPart &p_part);
	static Dictionary _message_to_openai(const AIModelMessage &p_message);
	static Array _messages_to_openai(const AIModelRequest &p_request);
	static Array _tools_to_openai(const AIModelRequest &p_request);
	static PackedByteArray _body_to_bytes(const AIModelRequest &p_request);

protected:
	static void _bind_methods();

public:
	AIOpenAICompatibleRuntime();

	virtual String get_runtime_type() const override;
	virtual bool configure(const Dictionary &p_config, AIError &r_error) override;
	virtual bool stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) override;

	void set_base_url(const String &p_base_url);
	String get_base_url() const;
	void set_api_key(const String &p_api_key);
	String get_api_key() const;
};
