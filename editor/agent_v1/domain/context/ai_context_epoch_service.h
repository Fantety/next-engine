/**************************************************************************/
/*  ai_context_epoch_service.h                                            */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/context/ai_context_epoch_store.h"
#include "editor/agent_v1/domain/context/ai_system_context.h"
#include "editor/agent_v1/domain/events/ai_event_store.h"
#include "editor/agent_v1/domain/projection/ai_session_projector.h"

#include "core/object/ref_counted.h"

class AIContextEpochService : public RefCounted {
	GDCLASS(AIContextEpochService, RefCounted);

	Ref<AIContextEpochStore> epoch_store;
	Ref<AIEventStore> event_store;
	Ref<AISessionProjector> projector;

	static bool _same_snapshot(const Dictionary &p_left, const Dictionary &p_right);
	static Dictionary _make_result(const AIContextEpoch &p_epoch, bool p_changed, bool p_initialized);
	bool _append_context_updated(const String &p_session_id, const String &p_agent_id, const AISystemContext &p_context, int p_next_revision, int64_t p_replacement_seq, AIContextEpoch &r_epoch, AIError &r_error);
	bool _get_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const;
	int64_t _latest_compaction_seq(const String &p_session_id) const;

protected:
	static void _bind_methods();

public:
	AIContextEpochService();

	void set_epoch_store(const Ref<AIContextEpochStore> &p_store);
	Ref<AIContextEpochStore> get_epoch_store() const;
	void set_event_store(const Ref<AIEventStore> &p_store);
	Ref<AIEventStore> get_event_store() const;
	void set_projector(const Ref<AISessionProjector> &p_projector);
	Ref<AISessionProjector> get_projector() const;

	bool initialize_struct(const String &p_session_id, const AILocationRef &p_location, const String &p_agent_id, const AISystemContext &p_context, AIContextEpoch &r_epoch, bool &r_initialized, AIError &r_error);
	bool prepare_struct(const String &p_session_id, const AILocationRef &p_location, const String &p_agent_id, const AISystemContext &p_context, AIContextEpoch &r_epoch, AIError &r_error);
	bool current_struct(const String &p_session_id, const String &p_agent_id, int p_revision, AIContextEpoch &r_epoch, AIError &r_error) const;
	bool current_struct(const String &p_session_id, const String &p_agent_id, int p_revision, const AISystemContext &p_context, AIContextEpoch &r_epoch, AIError &r_error) const;
	bool request_replacement_struct(const String &p_session_id, int64_t p_seq, AIError &r_error);
	bool reset_struct(const String &p_session_id, AIError &r_error);

	Dictionary initialize(const String &p_session_id, const Dictionary &p_location, const String &p_agent_id, const Dictionary &p_context);
	Dictionary prepare(const String &p_session_id, const Dictionary &p_location, const String &p_agent_id, const Dictionary &p_context);
	Dictionary current(const String &p_session_id, const String &p_agent_id, int p_revision) const;
	Dictionary request_replacement(const String &p_session_id, int64_t p_seq);
	Dictionary reset(const String &p_session_id);
};
