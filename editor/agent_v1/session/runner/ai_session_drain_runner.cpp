/**************************************************************************/
/*  ai_session_drain_runner.cpp                                           */
/**************************************************************************/

#include "ai_session_drain_runner.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AISessionDrainRunner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("drain", "session_id", "wake_seq"), &AISessionDrainRunner::drain, DEFVAL(0));
}

bool AISessionDrainRunner::drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) {
	(void)p_session_id;
	(void)p_cancel_token;
	(void)p_wake_seq;
	(void)r_promoted;
	r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Session drain runner is not implemented.");
	return false;
}

Dictionary AISessionDrainRunner::drain(const String &p_session_id, int64_t p_wake_seq) {
	Ref<AICancelToken> cancel_token;
	cancel_token.instantiate();

	Vector<AISessionInputRecord> promoted;
	AIError error;
	if (!drain_struct(p_session_id, cancel_token, p_wake_seq, promoted, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Array promoted_items;
	for (int i = 0; i < promoted.size(); i++) {
		promoted_items.push_back(promoted[i].to_dictionary());
	}

	Dictionary result;
	result["success"] = true;
	result["promoted"] = promoted_items;
	return result;
}
