/**************************************************************************/
/*  ai_session_runner.h                                                   */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
#include "editor/agent_v1/domain/projection/ai_session_history.h"
#include "editor/agent_v1/runtime/ai_llm_runtime_registry.h"
#include "editor/agent_v1/session/admission/ai_prompt_promoter.h"
#include "editor/agent_v1/session/runner/ai_session_drain_runner.h"

class AISessionRunner : public AISessionDrainRunner {
	GDCLASS(AISessionRunner, AISessionDrainRunner);

	Ref<AIPromptPromoter> prompt_promoter;
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;
	Ref<AIContextEpochStore> context_epoch_store;
	Ref<AIConfigService> config_service;
	Ref<AILLMRuntimeRegistry> runtime_registry;

	static String _message_text(const AISessionMessage &p_message);
	static AIModelMessage _message_to_model(const AISessionMessage &p_message);
	static String _system_baseline_from_array(const Array &p_system);
	static Vector<AIModelPart> _system_parts_from_array(const Array &p_system);
	static Dictionary _make_error_result(const AIError &p_error);

	bool _append_event(const String &p_session_id, const String &p_type, const Dictionary &p_data, bool p_live_only, AIEventRow &r_row, AIError &r_error);
	bool _ensure_context_epoch(const String &p_session_id, const String &p_agent_id, const Array &p_system, const String &p_provider, const String &p_model, AIContextEpoch &r_epoch, AIError &r_error);
	bool _build_request(const String &p_session_id, const String &p_agent_id, int64_t p_wake_seq, AIModelRequest &r_request, AIError &r_error);
	bool _run_provider_turn(const String &p_session_id, const AIModelRequest &p_request, const Ref<AICancelToken> &p_cancel_token, AIError &r_error);

protected:
	static void _bind_methods();

public:
	AISessionRunner();

	void set_prompt_promoter(const Ref<AIPromptPromoter> &p_prompt_promoter);
	Ref<AIPromptPromoter> get_prompt_promoter() const;
	void set_event_store(const Ref<AIEventStore> &p_event_store);
	Ref<AIEventStore> get_event_store() const;
	void set_projector(const Ref<AISessionProjector> &p_projector);
	Ref<AISessionProjector> get_projector() const;
	void set_context_epoch_store(const Ref<AIContextEpochStore> &p_store);
	Ref<AIContextEpochStore> get_context_epoch_store() const;
	void set_config_service(const Ref<AIConfigService> &p_config_service);
	Ref<AIConfigService> get_config_service() const;
	void set_runtime_registry(const Ref<AILLMRuntimeRegistry> &p_registry);
	Ref<AILLMRuntimeRegistry> get_runtime_registry() const;

	virtual bool drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) override;
	Dictionary run(const String &p_session_id, bool p_force = false);
};
