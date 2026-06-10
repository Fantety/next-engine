/**************************************************************************/
/*  ai_prompt_promoter.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/session/admission/ai_session_input_store.h"

#include "core/object/ref_counted.h"

class AIPromptPromoter : public RefCounted {
	GDCLASS(AIPromptPromoter, RefCounted);

	Ref<AISessionInputStore> input_store;

protected:
	static void _bind_methods();

public:
	void set_input_store(const Ref<AISessionInputStore> &p_input_store);
	Ref<AISessionInputStore> get_input_store() const;

	bool promote_eligible_struct(const String &p_session_id, const String &p_mode, Vector<AISessionInputRecord> &r_promoted, AIError &r_error);
	Dictionary promote_eligible(const String &p_session_id, const String &p_mode = "new-activity");
};
