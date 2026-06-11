/**************************************************************************/
/*  ai_fake_llm_runtime.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/runtime/ai_llm_runtime.h"

class AIFakeLLMRuntime : public AILLMRuntime {
	GDCLASS(AIFakeLLMRuntime, AILLMRuntime);

	String response_text;
	String tool_name;
	Variant tool_input;
	bool fail_next = false;
	int64_t stream_call_count = 0;
	Dictionary last_request;

	static String _last_user_text(const AIModelRequest &p_request);

protected:
	static void _bind_methods();

public:
	virtual String get_runtime_type() const override;
	virtual bool configure(const Dictionary &p_config, AIError &r_error) override;
	virtual bool stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error) override;

	void set_response_text(const String &p_text);
	String get_response_text() const;
	void set_tool_call(const String &p_name, const Variant &p_input = Dictionary());
	void clear_tool_call();
	String get_tool_name() const;
	void set_fail_next(bool p_fail);
	bool get_fail_next() const;
	int64_t get_stream_call_count() const;
	Dictionary get_last_request() const;
	void reset();
};
