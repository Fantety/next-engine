/**************************************************************************/
/*  ai_token_estimator.cpp                                                */
/**************************************************************************/

#include "ai_token_estimator.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AITokenEstimator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("estimate_text", "text"), &AITokenEstimator::estimate_text);
	ClassDB::bind_method(D_METHOD("estimate_variant", "value"), &AITokenEstimator::estimate_variant);
	ClassDB::bind_method(D_METHOD("estimate_message", "message"), &AITokenEstimator::estimate_message);
}

int64_t AITokenEstimator::estimate_text_tokens(const String &p_text) {
	const int64_t length = p_text.length();
	if (length <= 0) {
		return 0;
	}
	const int64_t estimated = (length + 3) / 4;
	return estimated < 1 ? 1 : estimated;
}

int64_t AITokenEstimator::estimate_variant_tokens(const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) {
		return 0;
	}
	if (p_value.get_type() == Variant::STRING) {
		return estimate_text_tokens(p_value);
	}
	return estimate_text_tokens(Variant(p_value).stringify());
}

int64_t AITokenEstimator::estimate_message_struct(const AISessionMessage &p_message) {
	int64_t total = estimate_text_tokens(p_message.text);
	for (int i = 0; i < p_message.content.size(); i++) {
		const AIAssistantContent &content = p_message.content[i];
		total += estimate_text_tokens(content.text);
		if (content.type == "tool") {
			total += estimate_text_tokens(content.name);
			total += estimate_variant_tokens(content.tool_state.input);
			total += estimate_variant_tokens(content.tool_state.output);
			total += estimate_variant_tokens(content.tool_state.result);
			if (content.tool_state.error.is_error()) {
				total += estimate_text_tokens(content.tool_state.error.message);
			}
		}
	}
	for (int i = 0; i < p_message.files.size(); i++) {
		total += estimate_text_tokens(p_message.files[i].path + " " + p_message.files[i].name + " " + p_message.files[i].mime);
	}
	return total < 1 ? 1 : total;
}

int64_t AITokenEstimator::estimate_text(const String &p_text) const {
	return estimate_text_tokens(p_text);
}

int64_t AITokenEstimator::estimate_variant(const Variant &p_value) const {
	return estimate_variant_tokens(p_value);
}

int64_t AITokenEstimator::estimate_message(const Dictionary &p_message) const {
	return estimate_message_struct(AISessionMessage::from_dictionary(p_message));
}
