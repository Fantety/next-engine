/**************************************************************************/
/*  ai_session_runner.h                                                   */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/agents/ai_agent_service_v1.h"
#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_model_part_builder.h"
#include "editor/agent_v1/domain/compaction/ai_compaction_service.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_service.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
#include "editor/agent_v1/domain/context/ai_context_source_registry.h"
#include "editor/agent_v1/domain/projection/ai_session_history.h"
#include "editor/agent_v1/runtime/ai_llm_runtime_registry.h"
#include "editor/agent_v1/session/admission/ai_prompt_promoter.h"
#include "editor/agent_v1/session/runner/ai_session_drain_runner.h"
#include "editor/agent_v1/session/service/ai_session_store.h"
#include "editor/agent_v1/skills/ai_skill_service_v1.h"
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
	Ref<AICompactionService> compaction_service;
	Ref<AIModelPartBuilder> model_part_builder;
	Ref<AIV1SkillService> skill_service;
	Ref<AIAgentService> agent_service;

	static String _message_text(const AISessionMessage &p_message);
	static String _assistant_model_text(const AISessionMessage &p_message);
	static String _tool_result_text(const AIAssistantContent &p_content);
	static String _latest_user_prompt_text(const Vector<AISessionMessage> &p_messages);
	static Vector<AIModelPart> _system_parts_from_baseline(const String &p_baseline);
	static int64_t _history_token_budget_from_config(const Dictionary &p_config);
	static bool _is_rebuild_request_conflict(const AIError &p_error);
	static AIError _error_from_result(const Dictionary &p_result, const String &p_fallback_message);
	static Array _permission_rules_from_config(const Dictionary &p_config);
	static Dictionary _model_part_for_event_log(const AIModelPart &p_part);
	static Dictionary _model_message_for_event_log(const AIModelMessage &p_message);
	static Dictionary _request_for_event_log(const AIModelRequest &p_request);
	static Dictionary _make_error_result(const AIError &p_error);

	bool _append_event(const String &p_session_id, const String &p_type, const Dictionary &p_data, bool p_live_only, AIEventRow &r_row, AIError &r_error);
	bool _resolve_session(const String &p_session_id, AISessionRow &r_session, AIError &r_error) const;
	bool _resolve_agent_for_session(const AISessionRow &p_session, AIAgentConfig &r_agent, AIError &r_error) const;
	Array _permission_rules_for_agent(const Dictionary &p_config, const AIAgentConfig &p_agent) const;
	bool _project_durable_history(const String &p_session_id);
	bool _configure_skill_service_from_config(const Dictionary &p_config, AIError &r_error) const;
	bool _select_skills_for_prompt(const Dictionary &p_config, const String &p_prompt, Array &r_selected_skills, AIError &r_error) const;
	bool _append_selected_skill_sources(const Array &p_selected_skills, Vector<AISystemContextSource> &r_sources, AIError &r_error) const;
	bool _load_system_context(const AISessionRow &p_session, const String &p_agent_id, const String &p_provider, const String &p_model, const Array &p_selected_skills, AISystemContext &r_context, AIError &r_error) const;
	bool _prepare_context_epoch(const AISessionRow &p_session, const String &p_agent_id, const String &p_provider, const String &p_model, const Array &p_selected_skills, AIContextEpoch &r_epoch, AIError &r_error);
	bool _verify_context_epoch_current(const String &p_session_id, const String &p_agent_id, const AIModelRequest &p_request, AIError &r_error) const;
	bool _append_message_to_model_messages(const AISessionMessage &p_message, const Dictionary &p_provider_config, Vector<AIModelMessage> &r_messages, AIError &r_error);
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
	void set_compaction_service(const Ref<AICompactionService> &p_service);
	Ref<AICompactionService> get_compaction_service() const;
	void set_model_part_builder(const Ref<AIModelPartBuilder> &p_builder);
	Ref<AIModelPartBuilder> get_model_part_builder() const;
	void set_skill_service(const Ref<AIV1SkillService> &p_service);
	Ref<AIV1SkillService> get_skill_service() const;
	void set_agent_service(const Ref<AIAgentService> &p_service);
	Ref<AIAgentService> get_agent_service() const;

	virtual bool drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) override;
	Dictionary run(const String &p_session_id, bool p_force = false);
};
