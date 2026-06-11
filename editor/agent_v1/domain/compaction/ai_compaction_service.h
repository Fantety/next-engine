/**************************************************************************/
/*  ai_compaction_service.h                                               */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"
#include "editor/agent_v1/domain/context/ai_context_epoch_service.h"
#include "editor/agent_v1/domain/context/ai_context_source_registry.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/domain/projection/ai_session_projector.h"

#include "core/object/ref_counted.h"

class AICompactionService : public RefCounted {
	GDCLASS(AICompactionService, RefCounted);

	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;
	Ref<AIContextSourceRegistry> context_source_registry;
	Ref<AIContextEpochService> context_epoch_service;

	static Dictionary _make_error_result(const AIError &p_error);
	static bool _message_has_open_tool(const AISessionMessage &p_message);
	static bool _message_is_compaction_candidate(const AISessionMessage &p_message);
	static int64_t _estimate_messages(const Vector<AISessionMessage> &p_messages);
	static int64_t _latest_compaction_seq(const Vector<AISessionMessage> &p_messages);
	static String _latest_compaction_summary(const Vector<AISessionMessage> &p_messages);
	static Vector<AISessionMessage> _messages_after_compaction(const Vector<AISessionMessage> &p_messages, int64_t p_compaction_seq);
	static Vector<AISessionMessage> _select_compaction_input(const Vector<AISessionMessage> &p_messages);
	static Array _message_ids(const Vector<AISessionMessage> &p_messages);
	static String _message_summary_line(const AISessionMessage &p_message);
	static void _append_previous_summary_lines(const String &p_previous_summary, String &r_user_goal, String &r_decisions, String &r_tool_results);
	static String _truncate_summary_to_budget(const String &p_summary, int64_t p_target_token_budget);
	static String _build_summary(const Vector<AISessionMessage> &p_messages, const String &p_previous_summary, int64_t p_target_token_budget);

	bool _project_history(const String &p_session_id, AIError &r_error) const;
	void _register_context_source(const String &p_session_id, int64_t p_seq, const String &p_compaction_id, const String &p_summary, int64_t p_token_before, int64_t p_token_after);

protected:
	static void _bind_methods();

public:
	void set_event_store(const Ref<AIEventStore> &p_store);
	Ref<AIEventStore> get_event_store() const;
	void set_projector(const Ref<AISessionProjector> &p_projector);
	Ref<AISessionProjector> get_projector() const;
	void set_context_source_registry(const Ref<AIContextSourceRegistry> &p_registry);
	Ref<AIContextSourceRegistry> get_context_source_registry() const;
	void set_context_epoch_service(const Ref<AIContextEpochService> &p_service);
	Ref<AIContextEpochService> get_context_epoch_service() const;

	bool compact_struct(const String &p_session_id, const String &p_reason, int64_t p_target_token_budget, Dictionary &r_result, AIError &r_error);
	bool maybe_compact_struct(const String &p_session_id, const String &p_reason, int64_t p_target_token_budget, bool &r_compacted, Dictionary &r_result, AIError &r_error);

	Dictionary compact(const Dictionary &p_input);
	Dictionary maybe_compact(const Dictionary &p_input);
};
