/**************************************************************************/
/*  ai_reference_resolver.h                                                */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIReferenceResolver {
public:
	struct ReferenceToken {
		int start_column = 0;
		int end_column = 0;
		bool clipboard = false;
		bool canvas = false;
		String path;
	};

	static Vector<ReferenceToken> find_reference_tokens(const String &p_text);
	static Vector<ReferenceToken> find_reference_tokens_in_line(const String &p_line);
	static Array resolve_attachments(const String &p_text);
	static Dictionary make_reference_attachment(const String &p_path);
	static Dictionary make_clipboard_reference_attachment();
	static Dictionary make_canvas_reference_attachment();
	static String make_reference_token_for_path(const String &p_path);
	static String make_attachment_label(const Dictionary &p_attachment);
	static bool attachments_equivalent(const Dictionary &p_a, const Dictionary &p_b);
};
