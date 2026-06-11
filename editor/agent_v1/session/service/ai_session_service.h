/**************************************************************************/
/*  ai_session_service.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/config/ai_config_service.h"
#include "editor/agent_v1/agents/ai_agent_service_v1.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_model_part_builder.h"
#include "editor/agent_v1/domain/attachments/ai_attachment_resolver.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_service.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
#include "editor/agent_v1/domain/context/ai_context_source_registry.h"
#include "editor/agent_v1/permission/ai_permission_service.h"
#include "editor/agent_v1/runtime/ai_llm_runtime_registry.h"
#include "editor/agent_v1/session/admission/ai_prompt_promoter.h"
#include "editor/agent_v1/session/execution/ai_session_execution.h"
#include "editor/agent_v1/session/runner/ai_empty_session_runner.h"
#include "editor/agent_v1/session/runner/ai_session_runner.h"
#include "editor/agent_v1/session/service/ai_session_store.h"
#include "editor/agent_v1/skills/ai_skill_service_v1.h"
#include "editor/agent_v1/tools/ai_tool_registry_v1.h"

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
	Ref<AISessionRunner> session_runner;
	Ref<AIContextEpochStore> context_epoch_store;
	Ref<AIContextSourceRegistry> context_source_registry;
	Ref<AIContextEpochService> context_epoch_service;
	Ref<AIConfigService> config_service;
	Ref<AILLMRuntimeRegistry> runtime_registry;
	Ref<AIPermissionService> permission_service;
	Ref<AIV1ToolRegistry> tool_registry;
	Ref<AIAttachmentBlobStore> attachment_blob_store;
	Ref<AIAttachmentResolver> attachment_resolver;
	Ref<AIModelPartBuilder> model_part_builder;
	Ref<AIV1SkillService> skill_service;
	Ref<AIAgentService> agent_service;
	Ref<AIV1TaskTool> task_tool;

	static Array _parts_from_input(const Dictionary &p_input);
	static AIPrompt _prompt_from_input(const Dictionary &p_input, const Array &p_parts);
	static Array _parts_from_prompt(const AIPrompt &p_prompt);
	static bool _part_is_attachment(const Dictionary &p_part);
	static bool _has_location_input(const Dictionary &p_input);
	static Dictionary _make_error_result(const AIError &p_error);

	void _ensure_defaults();
	void _wire_dependencies();
	bool _resolve_session_for_prompt(const Dictionary &p_input, AISessionRow &r_session, bool &r_created, AIError &r_error);
	bool _resolve_prompt_attachments(const Dictionary &p_input, const AISessionRow &p_session, const Array &p_parts, AIPrompt &r_prompt, AIError &r_error);
	bool _append_admitted_event(AISessionInputRecord &r_input, AIError &r_error);
	bool _append_interrupted_tool_events(const String &p_session_id, const String &p_reason, AIError &r_error);

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
	void set_session_runner(const Ref<AISessionRunner> &p_session_runner);
	Ref<AISessionRunner> get_session_runner() const;
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
	void set_permission_service(const Ref<AIPermissionService> &p_permission_service);
	Ref<AIPermissionService> get_permission_service() const;
	void set_tool_registry(const Ref<AIV1ToolRegistry> &p_tool_registry);
	Ref<AIV1ToolRegistry> get_tool_registry() const;
	void set_attachment_blob_store(const Ref<AIAttachmentBlobStore> &p_blob_store);
	Ref<AIAttachmentBlobStore> get_attachment_blob_store() const;
	void set_attachment_resolver(const Ref<AIAttachmentResolver> &p_resolver);
	Ref<AIAttachmentResolver> get_attachment_resolver() const;
	void set_model_part_builder(const Ref<AIModelPartBuilder> &p_builder);
	Ref<AIModelPartBuilder> get_model_part_builder() const;
	void set_skill_service(const Ref<AIV1SkillService> &p_service);
	Ref<AIV1SkillService> get_skill_service() const;
	void set_agent_service(const Ref<AIAgentService> &p_service);
	Ref<AIAgentService> get_agent_service() const;

	Dictionary create(const Dictionary &p_input);
	Dictionary prompt(const Dictionary &p_input);
	Dictionary reply_permission(const Dictionary &p_input);
	Dictionary interrupt(const Dictionary &p_input);
	Dictionary promote_eligible(const String &p_session_id, const String &p_mode = "new-activity");
};
