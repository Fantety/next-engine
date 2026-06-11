/**************************************************************************/
/*  ai_context_epoch_store.cpp                                            */
/**************************************************************************/

#include "ai_context_epoch_store.h"

#include "core/object/class_db.h"

void AIContextEpochStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_epoch", "epoch"), &AIContextEpochStore::set_epoch);
	ClassDB::bind_method(D_METHOD("get_epoch", "session_id"), &AIContextEpochStore::get_epoch);
	ClassDB::bind_method(D_METHOD("reset_epoch", "session_id", "baseline", "snapshot", "agent_id", "baseline_seq", "replacement_seq"), &AIContextEpochStore::reset_epoch, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("clear_epoch", "session_id"), &AIContextEpochStore::clear_epoch);
	ClassDB::bind_method(D_METHOD("has_epoch", "session_id"), &AIContextEpochStore::has_epoch);
	ClassDB::bind_method(D_METHOD("clear"), &AIContextEpochStore::clear);
}

bool AIContextEpochStore::set_epoch_struct(const AIContextEpoch &p_epoch) {
	if (p_epoch.session_id.strip_edges().is_empty()) {
		return false;
	}

	MutexLock lock(mutex);
	epochs_by_session[p_epoch.session_id] = p_epoch;
	return true;
}

bool AIContextEpochStore::get_epoch_struct(const String &p_session_id, AIContextEpoch &r_epoch) const {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	MutexLock lock(mutex);
	HashMap<String, AIContextEpoch>::ConstIterator epoch = epochs_by_session.find(session_id);
	if (!epoch) {
		return false;
	}

	r_epoch = epoch->value;
	return true;
}

AIContextEpoch AIContextEpochStore::reset_epoch_struct(const String &p_session_id, const String &p_baseline, const Dictionary &p_snapshot, const String &p_agent_id, int64_t p_baseline_seq, int64_t p_replacement_seq) {
	AIContextEpoch epoch;
	epoch.session_id = p_session_id.strip_edges();
	if (epoch.session_id.is_empty()) {
		return epoch;
	}

	MutexLock lock(mutex);
	const int previous_revision = epochs_by_session.has(epoch.session_id) ? epochs_by_session[epoch.session_id].revision : 0;
	epoch.baseline = p_baseline;
	epoch.snapshot = p_snapshot.duplicate(true);
	epoch.agent_id = p_agent_id;
	epoch.baseline_seq = p_baseline_seq;
	epoch.replacement_seq = p_replacement_seq;
	epoch.revision = previous_revision + 1;
	epochs_by_session[epoch.session_id] = epoch;
	return epoch;
}

bool AIContextEpochStore::clear_epoch_struct(const String &p_session_id) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		return false;
	}

	MutexLock lock(mutex);
	return epochs_by_session.erase(session_id);
}

bool AIContextEpochStore::set_epoch(const Dictionary &p_epoch) {
	return set_epoch_struct(AIContextEpoch::from_dictionary(p_epoch));
}

Dictionary AIContextEpochStore::get_epoch(const String &p_session_id) const {
	AIContextEpoch epoch;
	if (!get_epoch_struct(p_session_id, epoch)) {
		return Dictionary();
	}
	return epoch.to_dictionary();
}

Dictionary AIContextEpochStore::reset_epoch(const String &p_session_id, const String &p_baseline, const Dictionary &p_snapshot, const String &p_agent_id, int64_t p_baseline_seq, int64_t p_replacement_seq) {
	return reset_epoch_struct(p_session_id, p_baseline, p_snapshot, p_agent_id, p_baseline_seq, p_replacement_seq).to_dictionary();
}

bool AIContextEpochStore::clear_epoch(const String &p_session_id) {
	return clear_epoch_struct(p_session_id);
}

bool AIContextEpochStore::has_epoch(const String &p_session_id) const {
	MutexLock lock(mutex);
	return epochs_by_session.has(p_session_id.strip_edges());
}

void AIContextEpochStore::clear() {
	MutexLock lock(mutex);
	epochs_by_session.clear();
}
