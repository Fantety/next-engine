/**************************************************************************/
/*  ai_empty_session_runner.h                                             */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_cancel_token.h"
#include "editor/agent_v1/session/admission/ai_prompt_promoter.h"

#include "core/object/ref_counted.h"

class AIEmptySessionRunner : public RefCounted {
	GDCLASS(AIEmptySessionRunner, RefCounted);

	Ref<AIPromptPromoter> prompt_promoter;

	static Array _records_to_array(const Vector<AISessionInputRecord> &p_records);

protected:
	static void _bind_methods();

public:
	void set_prompt_promoter(const Ref<AIPromptPromoter> &p_prompt_promoter);
	Ref<AIPromptPromoter> get_prompt_promoter() const;

	bool drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error);
	Dictionary drain(const String &p_session_id, int64_t p_wake_seq = 0);
};
