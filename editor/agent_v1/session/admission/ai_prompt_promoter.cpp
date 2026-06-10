/**************************************************************************/
/*  ai_prompt_promoter.cpp                                                */
/**************************************************************************/

#include "ai_prompt_promoter.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AIPromptPromoter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_input_store", "input_store"), &AIPromptPromoter::set_input_store);
	ClassDB::bind_method(D_METHOD("get_input_store"), &AIPromptPromoter::get_input_store);
	ClassDB::bind_method(D_METHOD("promote_eligible", "session_id", "mode"), &AIPromptPromoter::promote_eligible, DEFVAL("new-activity"));
}

void AIPromptPromoter::set_input_store(const Ref<AISessionInputStore> &p_input_store) {
	input_store = p_input_store;
}

Ref<AISessionInputStore> AIPromptPromoter::get_input_store() const {
	return input_store;
}

bool AIPromptPromoter::promote_eligible_struct(const String &p_session_id, const String &p_mode, Vector<AISessionInputRecord> &r_promoted, AIError &r_error) {
	if (input_store.is_null()) {
		r_error = AIError::make(AI_ERROR_UNAVAILABLE, "Prompt promoter has no SessionInputStore.");
		return false;
	}

	const Vector<AISessionInputRecord> admitted = input_store->list_admitted_struct(p_session_id);
	if (admitted.is_empty()) {
		return true;
	}

	Vector<String> prompt_ids;
	const String mode = p_mode.strip_edges().to_lower();
	for (int i = 0; i < admitted.size(); i++) {
		const AISessionInputRecord &input = admitted[i];
		if (mode == "active-boundary" && input.delivery != AI_SESSION_INPUT_DELIVERY_STEER) {
			break;
		}

		prompt_ids.push_back(input.id);
		if (input.delivery == AI_SESSION_INPUT_DELIVERY_QUEUE) {
			break;
		}
	}

	if (prompt_ids.is_empty()) {
		return true;
	}
	return input_store->mark_promoted(p_session_id, prompt_ids, r_promoted, r_error);
}

Dictionary AIPromptPromoter::promote_eligible(const String &p_session_id, const String &p_mode) {
	Vector<AISessionInputRecord> promoted;
	AIError error;
	if (!promote_eligible_struct(p_session_id, p_mode, promoted, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}

	Array items;
	for (int i = 0; i < promoted.size(); i++) {
		items.push_back(promoted[i].to_dictionary());
	}

	Dictionary result;
	result["success"] = true;
	result["promoted"] = items;
	return result;
}
