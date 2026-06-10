/**************************************************************************/
/*  ai_session_drain_runner.h                                             */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_cancel_token.h"
#include "editor/agent_v1/session/model/ai_session_types.h"

#include "core/object/ref_counted.h"

class AISessionDrainRunner : public RefCounted {
	GDCLASS(AISessionDrainRunner, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual bool drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error);
	Dictionary drain(const String &p_session_id, int64_t p_wake_seq = 0);
};
