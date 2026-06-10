/**************************************************************************/
/*  ai_empty_session_runner.cpp                                           */
/**************************************************************************/

#include "ai_empty_session_runner.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AIEmptySessionRunner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_prompt_promoter", "prompt_promoter"), &AIEmptySessionRunner::set_prompt_promoter);
	ClassDB::bind_method(D_METHOD("get_prompt_promoter"), &AIEmptySessionRunner::get_prompt_promoter);
	ClassDB::bind_method(D_METHOD("drain", "session_id", "wake_seq"), &AIEmptySessionRunner::drain, DEFVAL(0));

	ADD_SIGNAL(MethodInfo("drain_started", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::INT, "wake_seq")));
	ADD_SIGNAL(MethodInfo("prompts_promoted", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::INT, "promoted_count")));
	ADD_SIGNAL(MethodInfo("drain_completed", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::INT, "promoted_count")));
	ADD_SIGNAL(MethodInfo("drain_failed", PropertyInfo(Variant::STRING, "session_id"), PropertyInfo(Variant::DICTIONARY, "error")));
}

Array AIEmptySessionRunner::_records_to_array(const Vector<AISessionInputRecord> &p_records) {
	Array result;
	for (int i = 0; i < p_records.size(); i++) {
		result.push_back(p_records[i].to_dictionary());
	}
	return result;
}

void AIEmptySessionRunner::set_prompt_promoter(const Ref<AIPromptPromoter> &p_prompt_promoter) {
	prompt_promoter = p_prompt_promoter;
}

Ref<AIPromptPromoter> AIEmptySessionRunner::get_prompt_promoter() const {
	return prompt_promoter;
}

bool AIEmptySessionRunner::drain_struct(const String &p_session_id, const Ref<AICancelToken> &p_cancel_token, int64_t p_wake_seq, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) {
	const String session_id = p_session_id.strip_edges();
	if (session_id.is_empty()) {
		r_error = AIError::make(AI_ERROR_VALIDATION, "Session id is required to drain.");
		return false;
	}
	if (prompt_promoter.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Empty session runner has no PromptPromoter.");
		return false;
	}
	if (p_cancel_token.is_valid() && p_cancel_token->is_cancel_requested()) {
		r_error = AIError::make(AI_ERROR_INTERRUPTED, p_cancel_token->get_cancel_message("Session drain interrupted."));
		return false;
	}

	call_deferred("emit_signal", SNAME("drain_started"), session_id, p_wake_seq);

	Vector<AISessionInputRecord> promoted;
	if (!prompt_promoter->promote_eligible_struct(session_id, "new-activity", promoted, r_error)) {
		call_deferred("emit_signal", SNAME("drain_failed"), session_id, r_error.to_dictionary());
		return false;
	}

	r_promoted = promoted;
	if (!promoted.is_empty()) {
		call_deferred("emit_signal", SNAME("prompts_promoted"), session_id, promoted.size());
	}
	call_deferred("emit_signal", SNAME("drain_completed"), session_id, promoted.size());
	return true;
}

Dictionary AIEmptySessionRunner::drain(const String &p_session_id, int64_t p_wake_seq) {
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

	Dictionary result;
	result["success"] = true;
	result["promoted"] = _records_to_array(promoted);
	return result;
}
