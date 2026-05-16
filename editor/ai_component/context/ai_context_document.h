/**************************************************************************/
/*  ai_context_document.h                                                  */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

struct AIContextDocument {
	String title;
	String source;
	String content;
	bool truncated = false;

	Dictionary to_dict() const {
		Dictionary dict;
		dict["title"] = title;
		dict["source"] = source;
		dict["content"] = content;
		dict["truncated"] = truncated;
		return dict;
	}
};
