/**************************************************************************/
/*  ai_llm_runtime.h                                                      */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_cancel_token.h"
#include "editor/agent_v1/core/runtime/ai_model_request.h"
#include "editor/agent_v1/core/runtime/ai_stream_sink.h"

#include "core/object/ref_counted.h"

class AILLMRuntime : public RefCounted {
	GDCLASS(AILLMRuntime, RefCounted);

protected:
	static void _bind_methods();

public:
	static AIModelRequest request_from_dictionary(const Dictionary &p_dict);

	virtual String get_runtime_type() const;
	virtual bool configure(const Dictionary &p_config, AIError &r_error);
	virtual bool stream_struct(const AIModelRequest &p_request, const Ref<AIStreamSink> &p_sink, const Ref<AICancelToken> &p_cancel_token, AIError &r_error);

	Dictionary configure_runtime(const Dictionary &p_config);
	Dictionary stream(const Dictionary &p_request, const Callable &p_event_callback = Callable());
};
