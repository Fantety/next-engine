/**************************************************************************/
/*  ai_session_service.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/session/admission/ai_prompt_promoter.h"
#include "editor/agent_v1/session/execution/ai_session_execution.h"
#include "editor/agent_v1/session/runner/ai_empty_session_runner.h"
#include "editor/agent_v1/session/service/ai_session_store.h"

#include "core/object/ref_counted.h"

class AISessionService : public RefCounted {
	GDCLASS(AISessionService, RefCounted);

	Ref<AISessionStore> session_store;
	Ref<AISessionInputStore> input_store;
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;
	Ref<AISessionExecution> execution;
	Ref<AIPromptPromoter> prompt_promoter;
	Ref<AIEmptySessionRunner> empty_runner;

	static Array _parts_from_input(const Dictionary &p_input);
	static AIPrompt _prompt_from_input(const Dictionary &p_input, const Array &p_parts);
	static bool _has_location_input(const Dictionary &p_input);
	static Dictionary _make_error_result(const AIError &p_error);

	void _ensure_defaults();
	void _wire_dependencies();
	bool _resolve_session_for_prompt(const Dictionary &p_input, AISessionRow &r_session, bool &r_created, AIError &r_error);
	bool _append_admitted_event(AISessionInputRecord &r_input, AIError &r_error);

protected:
	static void _bind_methods();

public:
	AISessionService();

	void set_session_store(const Ref<AISessionStore> &p_session_store);
	Ref<AISessionStore> get_session_store() const;
	void set_input_store(const Ref<AISessionInputStore> &p_input_store);
	Ref<AISessionInputStore> get_input_store() const;
	void set_event_store(const Ref<AIEventStore> &p_event_store);
	Ref<AIEventStore> get_event_store() const;
	void set_projector(const Ref<AISessionProjector> &p_projector);
	Ref<AISessionProjector> get_projector() const;
	void set_execution(const Ref<AISessionExecution> &p_execution);
	Ref<AISessionExecution> get_execution() const;
	void set_prompt_promoter(const Ref<AIPromptPromoter> &p_prompt_promoter);
	Ref<AIPromptPromoter> get_prompt_promoter() const;
	void set_empty_runner(const Ref<AIEmptySessionRunner> &p_empty_runner);
	Ref<AIEmptySessionRunner> get_empty_runner() const;

	Dictionary create(const Dictionary &p_input);
	Dictionary prompt(const Dictionary &p_input);
	Dictionary interrupt(const Dictionary &p_input);
	Dictionary promote_eligible(const String &p_session_id, const String &p_mode = "new-activity");
};
