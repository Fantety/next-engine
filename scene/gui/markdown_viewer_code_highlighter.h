/**************************************************************************/
/*  markdown_viewer_code_highlighter.h                                    */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

struct MarkdownViewerCodeSpan {
	enum Type {
		TYPE_NORMAL,
		TYPE_KEYWORD,
		TYPE_STRING,
		TYPE_COMMENT,
		TYPE_NUMBER,
	};

	Type type = TYPE_NORMAL;
	String text;
};

class MarkdownViewerCodeHighlighter {
	bool _is_keyword(const String &p_language, const String &p_word) const;
	bool _is_identifier_char(char32_t p_char) const;
	void _push_span(Vector<MarkdownViewerCodeSpan> &r_spans, MarkdownViewerCodeSpan::Type p_type, const String &p_text) const;

public:
	Vector<MarkdownViewerCodeSpan> highlight_line(const String &p_language, const String &p_line) const;
};
