/**************************************************************************/
/*  ai_session_history.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/projection/ai_session_projector.h"

#include "core/object/ref_counted.h"

class AISessionHistory : public RefCounted {
	GDCLASS(AISessionHistory, RefCounted);

protected:
	static void _bind_methods();

public:
	static Vector<AISessionMessage> entries_for_runner(const Vector<AISessionMessage> &p_messages, int64_t p_baseline_seq);

	Array entries_for_runner_from_messages(const Array &p_messages, int64_t p_baseline_seq) const;
	Array entries_for_runner_from_projector(const Ref<AISessionProjector> &p_projector, const String &p_session_id, int64_t p_baseline_seq) const;
};
