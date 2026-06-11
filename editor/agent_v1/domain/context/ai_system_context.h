/**************************************************************************/
/*  ai_system_context.h                                                   */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

struct AISystemContextSource {
	String domain;
	String text;
	String content_hash;
	bool required = true;
	bool available = true;
	int priority = 0;
	Dictionary metadata;

	bool is_blocking() const;
	Dictionary to_dictionary() const;
	static AISystemContextSource from_dictionary(const Dictionary &p_dict);
};

struct AISystemContext {
	String baseline;
	Vector<AISystemContextSource> sources;
	Dictionary snapshot;
	bool available = true;
	String blocked_reason;

	bool is_available() const;
	Dictionary to_dictionary() const;
	static AISystemContext from_dictionary(const Dictionary &p_dict);
	static AISystemContext combine(const Vector<AISystemContextSource> &p_sources);
};
