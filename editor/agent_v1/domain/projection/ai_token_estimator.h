/**************************************************************************/
/*  ai_token_estimator.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/domain/model/ai_domain_types.h"

#include "core/object/ref_counted.h"

class AITokenEstimator : public RefCounted {
	GDCLASS(AITokenEstimator, RefCounted);

protected:
	static void _bind_methods();

public:
	static int64_t estimate_text_tokens(const String &p_text);
	static int64_t estimate_variant_tokens(const Variant &p_value);
	static int64_t estimate_message_struct(const AISessionMessage &p_message);

	int64_t estimate_text(const String &p_text) const;
	int64_t estimate_variant(const Variant &p_value) const;
	int64_t estimate_message(const Dictionary &p_message) const;
};
