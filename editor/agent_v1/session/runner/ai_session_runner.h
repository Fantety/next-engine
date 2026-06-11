/**************************************************************************/
/*  ai_session_runner.h                                                   */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_model_part_builder.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_service.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
#include "editor/agent_v1/domain/context/ai_context_source_registry.h"
#include "editor/agent_v1/domain/projection/ai_session_history.h"
#include "editor/agent_v1/runtime/ai_llm_runtime_registry.h"
#include "editor/agent_v1/session/admission/ai_prompt_promoter.h"
#include "editor/agent_v1/session/runner/ai_session_drain_runner.h"
#include "editor/agent_v1/session/service/ai_session_store.h"
#include "editor/agent_v1/tools/ai_tool_registry_v1.h"

class AISessionRunner : public AISessionDrainRunner {
	GDCLASS(AISessionRunner, AISessionDrainRunner);

	Ref<AIPromptPromoter> prompt_promoter;
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;
	Ref<AIContextEpochStore> context_epoch_store;
	Ref<AIContextSourceRegistry> context_source_registry;
	Ref<AIContextEpochService> context_epoch_service;
	Ref<AIConfigService> config_service;
	Ref<AILLMRuntimeRegistry> runtime_registry;
	Ref<AIV1ToolRegistry> tool_registry;
	Ref<AISessionStore> session_store;
	Ref<AIModelPartBuilder> model_part_builder;

	static String _message_text(const AISessionMessage &p_message);
	static Vector<AIModelPart> _system_parts_from_baseline(const String &p_baseline);
	static int64_t _history_token_budget_from_config(const Dictionary &p_config);
	static bool _is_rebuild_request_conflict(const AIError &p_error);
	static Dictionary _model_part_for_event_log(const AIModelPart &p_part);
	static Dictionary _model_message_for_event_log(const AIModelMessage &p_message);
	static Dictionary _request_for_event_log(const AIModelRequest &p_request);
	static Dictionary _make_error_result(const AIError &p_error);

	bool _message_to_model(const AISessionMessage &p_message, const Dictionary &p_provider_config, AIModelMessage &r_message, AIError &r_error);
	bool _append_event(const String &p_session_id, const String &p_type, const Dictionary &p_data, bool p_live_only, AIEventRow &r_row, AIError &r_error);
	bool _resolve_session(const String &p_session_id, AISessionRow &r_session, AIError &r_error) const;
	bool _project_durable_history(const String &p_session_id);
	bool _load_system_context(const AISessionRow &p_session, const String &p_agent_id, const String &p_provider, const String &p_model, AISystemContext &r_context, AIError &r_error) const;
	bool _prepare_context_epoch(const AISessionRow &p_session, const String &p_agent_id, const String &p_provider, const String &p_model, AIContextEpoch &r_epoch, AIError &r_error);
	bool _verify_context_epoch_current(const String &p_session_id, const String &p_agent_id, const AIModelRequest &p_request, AIError &r_error) const;
	bool _build_request(const AISessionRow &p_session, const String &p_agent_id, const String &p_root_dir, int64_t p_wake_seq, AIModelRequest &r_request, Ref<AIV1ToolMaterialization> &r_tool_materialization, AIError &r_error);
	bool _settle_open_tool_calls(const AISessionRow &p_session, const String &p_agent_id, const String &p_root_dir, const Array &p_permission_rules, const Ref<AICancelToken> &p_cancel_token, bool &r_needs_continuation, bool &r_waiting_permission, AIError &r_error);
	bool _run_provider_turn(const String &p_session_id, const String &p_agent_id, const AIModelRequest &p_request, const Ref<AIV1ToolMaterialization> &p_tool_materialization, const Ref<AICancelToken> &p_cancel_token, bool &r_needs_continuation, bool &r_waiting_permission, AIError &r_error);
	bool _drain_struct_internal(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, bool p_force, Vector<AISessionInputRecord> &r_promoted, AIError &r_error);

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
	void set_context_source_registry(const Ref<AIContextSourceRegistry> &p_registry);
	Ref<AIContextSourceRegistry> get_context_source_registry() const;
	void set_context_epoch_service(const Ref<AIContextEpochService> &p_service);
	Ref<AIContextEpochService> get_context_epoch_service() const;
	void set_config_service(const Ref<AIConfigService> &p_config_service);
	Ref<AIConfigService> get_config_service() const;
	void set_runtime_registry(const Ref<AILLMRuntimeRegistry> &p_registry);
	Ref<AILLMRuntimeRegistry> get_runtime_registry() const;
	void set_tool_registry(const Ref<AIV1ToolRegistry> &p_registry);
	Ref<AIV1ToolRegistry> get_tool_registry() const;
	void set_session_store(const Ref<AISessionStore> &p_store);
	Ref<AISessionStore> get_session_store() const;
	void set_model_part_builder(const Ref<AIModelPartBuilder> &p_builder);
	Ref<AIModelPartBuilder> get_model_part_builder() const;

	virtual bool drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) override;
	Dictionary run(const String &p_session_id, bool p_force = false);
};
