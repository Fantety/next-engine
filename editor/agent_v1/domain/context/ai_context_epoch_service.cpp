/**************************************************************************/
/*  ai_context_epoch_service.cpp                                          */
/**************************************************************************/

#include "ai_context_epoch_service.h"

#include "editor/agent_v1/core/base/ai_id.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AIContextEpochService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_epoch_store", "store"), &AIContextEpochService::set_epoch_store);
	ClassDB::bind_method(D_METHOD("get_epoch_store"), &AIContextEpochService::get_epoch_store);
	ClassDB::bind_method(D_METHOD("set_event_store", "store"), &AIContextEpochService::set_event_store);
	ClassDB::bind_method(D_METHOD("get_event_store"), &AIContextEpochService::get_event_store);
	ClassDB::bind_method(D_METHOD("set_projector", "projector"), &AIContextEpochService::set_projector);
	ClassDB::bind_method(D_METHOD("get_projector"), &AIContextEpochService::get_projector);
	ClassDB::bind_method(D_METHOD("initialize", "session_id", "location", "agent_id", "context"), &AIContextEpochService::initialize);
	ClassDB::bind_method(D_METHOD("prepare", "session_id", "location", "agent_id", "context"), &AIContextEpochService::prepare);
	ClassDB::bind_method(D_METHOD("current", "session_id", "agent_id", "revision"), &AIContextEpochService::current);
	ClassDB::bind_method(D_METHOD("request_replacement", "session_id", "seq"), &AIContextEpochService::request_replacement);
	ClassDB::bind_method(D_METHOD("reset", "session_id"), &AIContextEpochService::reset);
}

AIContextEpochService::AIContextEpochService() {
	epoch_store.instantiate();
}

bool AIContextEpochService::_same_snapshot(const Dictionary &p_left, const Dictionary &p_right) {
	const String left_hash = String(p_left.get("snapshot_hash", String())).strip_edges();
	const String right_hash = String(p_right.get("snapshot_hash", String())).strip_edges();
	if (!left_hash.is_empty() || !right_hash.is_empty()) {
		return left_hash == right_hash;
	}
	return Variant(p_left).stringify() == Variant(p_right).stringify();
}

Dictionary AIContextEpochService::_make_result(const AIContextEpoch &p_epoch, bool p_changed, bool p_initialized) {
	Dictionary result;
	result["success"] = true;
	result["epoch"] = p_epoch.to_dictionary();
	result["changed"] = p_changed;
	result["initialized"] = p_initialized;
	return result;
}

bool AIContextEpochService::_append_context_updated(const String &p_session_id, const String &p_agent_id, const AISystemContext &p_context, int p_next_revision, int64_t p_replacement_seq, AIContextEpoch &r_epoch, AIError &r_error) {
	if (event_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ContextEpochService has no EventStore.");
		return false;
	}
	if (epoch_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ContextEpochService has no ContextEpochStore.");
		return false;
	}
	if (!p_context.is_available()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, p_context.blocked_reason.is_empty() ? String("System context is unavailable.") : p_context.blocked_reason);
		return false;
	}

	AIContextEpoch epoch;
	epoch.session_id = p_session_id;
	epoch.agent_id = p_agent_id.strip_edges().is_empty() ? String("main") : p_agent_id.strip_edges();
	epoch.baseline = p_context.baseline;
	epoch.snapshot = p_context.snapshot.duplicate(true);
	epoch.baseline_seq = 0;
	epoch.replacement_seq = 0;
	epoch.revision = p_next_revision <= 0 ? 1 : p_next_revision;

	Dictionary data;
	data["session_id"] = p_session_id;
	data["message_id"] = AIId::make("system");
	data["text"] = p_context.baseline;
	data["baseline"] = p_context.baseline;
	data["snapshot"] = p_context.snapshot.duplicate(true);
	data["context"] = p_context.to_dictionary();
	data["agent_id"] = epoch.agent_id;
	data["baseline_seq"] = epoch.baseline_seq;
	data["replacement_seq"] = p_replacement_seq;
	data["revision"] = epoch.revision;
	data["epoch"] = epoch.to_dictionary();

	AIEventRow row;
	String event_error;
	if (!event_store->append(p_session_id, AIDomainEventTypes::context_updated(), data, false, row, event_error)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, event_error);
		return false;
	}

	epoch.baseline_seq = row.seq;
	if (!epoch_store->set_epoch_struct(epoch)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to store context epoch.");
		return false;
	}
	if (projector.is_valid()) {
		projector->project(row);
	}

	r_epoch = epoch;
	r_error = AIError::none();
	return true;
}

bool AIContextEpochService::_get_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const {
	if (epoch_store.is_valid() && epoch_store->get_epoch_struct(p_session_id, r_epoch)) {
		return true;
	}

	if (projector.is_valid()) {
		if (event_store.is_valid()) {
			const int64_t after_seq = projector->get_projected_seq(p_session_id);
			projector->project_from_store(event_store, p_session_id, after_seq);
		}

		if (projector->get_context_epoch_struct(p_session_id, r_epoch)) {
			if (epoch_store.is_valid()) {
				epoch_store->set_epoch_struct(r_epoch);
			}
			return true;
		}
	}

	return false;
}

int64_t AIContextEpochService::_latest_compaction_seq(const String &p_session_id) const {
	if (projector.is_null()) {
		return 0;
	}
	const Vector<AISessionMessage> messages = projector->get_messages_struct(p_session_id);
	int64_t seq = 0;
	for (int i = 0; i < messages.size(); i++) {
		if (messages[i].type == AI_SESSION_MESSAGE_COMPACTION && messages[i].seq > seq) {
			seq = messages[i].seq;
		}
	}
	return seq;
}

void AIContextEpochService::set_epoch_store(const Ref<AIContextEpochStore> &p_store) {
	epoch_store = p_store;
}

Ref<AIContextEpochStore> AIContextEpochService::get_epoch_store() const {
	return epoch_store;
}

void AIContextEpochService::set_event_store(const Ref<AIEventStore> &p_store) {
	event_store = p_store;
}

Ref<AIEventStore> AIContextEpochService::get_event_store() const {
	return event_store;
}

void AIContextEpochService::set_projector(const Ref<AISessionProjector> &p_projector) {
	projector = p_projector;
}

Ref<AISessionProjector> AIContextEpochService::get_projector() const {
	return projector;
}

bool AIContextEpochService::initialize_struct(const String &p_session_id, const AILocationRef &p_location, const String &p_agent_id, const AISystemContext &p_context, AIContextEpoch &r_epoch, bool &r_initialized, AIError &r_error) {
	(void)p_location;
	r_initialized = false;
	if (epoch_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ContextEpochService has no ContextEpochStore.");
		return false;
	}

	AIContextEpoch existing;
	if (_get_epoch_struct(p_session_id, existing)) {
		r_epoch = existing;
		r_error = AIError::none();
		return true;
	}

	AIContextEpoch epoch;
	if (!_append_context_updated(p_session_id, p_agent_id, p_context, 1, 0, epoch, r_error)) {
		return false;
	}

	r_epoch = epoch;
	r_initialized = true;
	return true;
}

bool AIContextEpochService::prepare_struct(const String &p_session_id, const AILocationRef &p_location, const String &p_agent_id, const AISystemContext &p_context, AIContextEpoch &r_epoch, AIError &r_error) {
	(void)p_location;
	if (epoch_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ContextEpochService has no ContextEpochStore.");
		return false;
	}

	AIContextEpoch existing;
	if (!_get_epoch_struct(p_session_id, existing)) {
		bool initialized = false;
		return initialize_struct(p_session_id, p_location, p_agent_id, p_context, r_epoch, initialized, r_error);
	}

	int64_t replacement_seq = existing.replacement_seq;
	const int64_t compaction_seq = _latest_compaction_seq(p_session_id);
	if (compaction_seq > existing.baseline_seq && compaction_seq > replacement_seq) {
		replacement_seq = compaction_seq;
	}

	const String agent_id = p_agent_id.strip_edges().is_empty() ? String("main") : p_agent_id.strip_edges();
	const bool replacement_requested = replacement_seq > 0 || existing.agent_id != agent_id;
	const bool changed = !_same_snapshot(existing.snapshot, p_context.snapshot) || existing.baseline != p_context.baseline;
	if (!replacement_requested && !changed) {
		r_epoch = existing;
		r_error = AIError::none();
		return true;
	}

	if (!p_context.is_available()) {
		const String message = replacement_requested ? String("Context replacement is blocked because system context is unavailable.") : String("System context is unavailable.");
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, p_context.blocked_reason.is_empty() ? message : p_context.blocked_reason);
		return false;
	}

	AIContextEpoch epoch;
	if (!_append_context_updated(p_session_id, agent_id, p_context, existing.revision + 1, replacement_seq, epoch, r_error)) {
		return false;
	}
	r_epoch = epoch;
	return true;
}

bool AIContextEpochService::current_struct(const String &p_session_id, const String &p_agent_id, int p_revision, AIContextEpoch &r_epoch, AIError &r_error) const {
	if (epoch_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ContextEpochService has no ContextEpochStore.");
		return false;
	}

	AIContextEpoch epoch;
	if (!_get_epoch_struct(p_session_id, epoch)) {
		r_error = AIError::make(AI_ERROR_CONFLICT, "Context epoch is missing.");
		return false;
	}

	const String agent_id = p_agent_id.strip_edges().is_empty() ? String("main") : p_agent_id.strip_edges();
	if (epoch.agent_id != agent_id || epoch.revision != p_revision) {
		Dictionary details;
		details["rebuild_request"] = true;
		details["expected_agent"] = agent_id;
		details["actual_agent"] = epoch.agent_id;
		details["expected_revision"] = p_revision;
		details["actual_revision"] = epoch.revision;
		r_error = AIError::make(AI_ERROR_CONFLICT, "Context epoch changed before provider turn.", details);
		return false;
	}

	r_epoch = epoch;
	r_error = AIError::none();
	return true;
}

bool AIContextEpochService::current_struct(const String &p_session_id, const String &p_agent_id, int p_revision, const AISystemContext &p_context, AIContextEpoch &r_epoch, AIError &r_error) const {
	if (!current_struct(p_session_id, p_agent_id, p_revision, r_epoch, r_error)) {
		return false;
	}

	if (!_same_snapshot(r_epoch.snapshot, p_context.snapshot) || r_epoch.baseline != p_context.baseline) {
		Dictionary details;
		details["rebuild_request"] = true;
		details["expected_snapshot"] = r_epoch.snapshot.duplicate(true);
		details["actual_snapshot"] = p_context.snapshot.duplicate(true);
		details["expected_baseline_hash"] = r_epoch.baseline.md5_text();
		details["actual_baseline_hash"] = p_context.baseline.md5_text();
		r_error = AIError::make(AI_ERROR_CONFLICT, "Context sources changed before provider turn.", details);
		return false;
	}

	r_error = AIError::none();
	return true;
}

bool AIContextEpochService::request_replacement_struct(const String &p_session_id, int64_t p_seq, AIError &r_error) {
	if (epoch_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ContextEpochService has no ContextEpochStore.");
		return false;
	}

	AIContextEpoch epoch;
	if (!_get_epoch_struct(p_session_id, epoch)) {
		r_error = AIError::none();
		return true;
	}

	epoch.replacement_seq = p_seq > epoch.replacement_seq ? p_seq : epoch.replacement_seq;
	epoch.revision += 1;
	if (!epoch_store->set_epoch_struct(epoch)) {
		r_error = AIError::make(AI_ERROR_INTERNAL, "Failed to request context epoch replacement.");
		return false;
	}
	r_error = AIError::none();
	return true;
}

bool AIContextEpochService::reset_struct(const String &p_session_id, AIError &r_error) {
	if (epoch_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "ContextEpochService has no ContextEpochStore.");
		return false;
	}
	epoch_store->clear_epoch_struct(p_session_id);
	r_error = AIError::none();
	return true;
}

Dictionary AIContextEpochService::initialize(const String &p_session_id, const Dictionary &p_location, const String &p_agent_id, const Dictionary &p_context) {
	AIContextEpoch epoch;
	bool initialized = false;
	AIError error;
	if (!initialize_struct(p_session_id, AILocationRef::from_dictionary(p_location), p_agent_id, AISystemContext::from_dictionary(p_context), epoch, initialized, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	return _make_result(epoch, initialized, initialized);
}

Dictionary AIContextEpochService::prepare(const String &p_session_id, const Dictionary &p_location, const String &p_agent_id, const Dictionary &p_context) {
	AIContextEpoch before;
	const bool had_epoch = _get_epoch_struct(p_session_id, before);

	AIContextEpoch epoch;
	AIError error;
	if (!prepare_struct(p_session_id, AILocationRef::from_dictionary(p_location), p_agent_id, AISystemContext::from_dictionary(p_context), epoch, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	const bool changed = !had_epoch || before.revision != epoch.revision || before.baseline_seq != epoch.baseline_seq || before.baseline != epoch.baseline;
	return _make_result(epoch, changed, !had_epoch);
}

Dictionary AIContextEpochService::current(const String &p_session_id, const String &p_agent_id, int p_revision) const {
	AIContextEpoch epoch;
	AIError error;
	if (!current_struct(p_session_id, p_agent_id, p_revision, epoch, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	Dictionary result;
	result["success"] = true;
	result["epoch"] = epoch.to_dictionary();
	return result;
}

Dictionary AIContextEpochService::request_replacement(const String &p_session_id, int64_t p_seq) {
	AIError error;
	Dictionary result;
	result["success"] = request_replacement_struct(p_session_id, p_seq, error);
	if (error.is_error()) {
		result["error"] = error.to_dictionary();
	}
	return result;
}

Dictionary AIContextEpochService::reset(const String &p_session_id) {
	AIError error;
	Dictionary result;
	result["success"] = reset_struct(p_session_id, error);
	if (error.is_error()) {
		result["error"] = error.to_dictionary();
	}
	return result;
}
