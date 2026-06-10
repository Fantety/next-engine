/**************************************************************************/
/*  ai_session_history.cpp                                                */
/**************************************************************************/

#include "ai_session_history.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AISessionHistory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("entries_for_runner_from_messages", "messages", "baseline_seq"), &AISessionHistory::entries_for_runner_from_messages);
	ClassDB::bind_method(D_METHOD("entries_for_runner_from_projector", "projector", "session_id", "baseline_seq"), &AISessionHistory::entries_for_runner_from_projector);
}

Vector<AISessionMessage> AISessionHistory::entries_for_runner(const Vector<AISessionMessage> &p_messages, int64_t p_baseline_seq) {
	int64_t compaction_boundary_seq = 0;
	for (int i = 0; i < p_messages.size(); i++) {
		if (p_messages[i].type == AI_SESSION_MESSAGE_COMPACTION && p_messages[i].seq > compaction_boundary_seq) {
			compaction_boundary_seq = p_messages[i].seq;
		}
	}

	Vector<AISessionMessage> result;
	for (int i = 0; i < p_messages.size(); i++) {
		const AISessionMessage &message = p_messages[i];
		if (compaction_boundary_seq > 0 && message.seq < compaction_boundary_seq) {
			continue;
		}
		if (!message.is_runner_visible_system_message(p_baseline_seq)) {
			continue;
		}
		result.push_back(message);
	}
	return result;
}

Array AISessionHistory::entries_for_runner_from_messages(const Array &p_messages, int64_t p_baseline_seq) const {
	Vector<AISessionMessage> messages;
	for (int i = 0; i < p_messages.size(); i++) {
		if (p_messages[i].get_type() == Variant::DICTIONARY) {
			messages.push_back(AISessionMessage::from_dictionary(p_messages[i]));
		}
	}

	Array result;
	const Vector<AISessionMessage> entries = entries_for_runner(messages, p_baseline_seq);
	for (int i = 0; i < entries.size(); i++) {
		result.push_back(entries[i].to_dictionary());
	}
	return result;
}

Array AISessionHistory::entries_for_runner_from_projector(const Ref<AISessionProjector> &p_projector, const String &p_session_id, int64_t p_baseline_seq) const {
	if (p_projector.is_null()) {
		return Array();
	}

	Array result;
	const Vector<AISessionMessage> entries = entries_for_runner(p_projector->get_messages_struct(p_session_id), p_baseline_seq);
	for (int i = 0; i < entries.size(); i++) {
		result.push_back(entries[i].to_dictionary());
	}
	return result;
}
