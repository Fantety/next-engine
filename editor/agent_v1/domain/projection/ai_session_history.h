/**************************************************************************/
/*  ai_session_history.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/projection/ai_session_projector.h"
#include "editor/agent_v1/domain/projection/ai_token_estimator.h"

#include "core/object/ref_counted.h"

class AISessionHistory : public RefCounted {
	GDCLASS(AISessionHistory, RefCounted);

protected:
	static void _bind_methods();

public:
	static Vector<AISessionMessage> entries_for_runner(const Vector<AISessionMessage> &p_messages, int64_t p_baseline_seq);
	static Vector<AISessionMessage> entries_for_runner(const Vector<AISessionMessage> &p_messages, int64_t p_baseline_seq, int64_t p_token_budget);

	Array entries_for_runner_from_messages(const Array &p_messages, int64_t p_baseline_seq) const;
	Array entries_for_runner_with_budget_from_messages(const Array &p_messages, int64_t p_baseline_seq, int64_t p_token_budget) const;
	Array entries_for_runner_from_projector(const Ref<AISessionProjector> &p_projector, const String &p_session_id, int64_t p_baseline_seq) const;
	Array entries_for_runner_with_budget_from_projector(const Ref<AISessionProjector> &p_projector, const String &p_session_id, int64_t p_baseline_seq, int64_t p_token_budget) const;
};
