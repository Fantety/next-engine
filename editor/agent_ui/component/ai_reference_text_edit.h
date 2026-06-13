/**************************************************************************/
/*  ai_reference_text_edit.h                                               */
/**************************************************************************/

#pragma once

#include "scene/gui/text_edit.h"
#include "scene/resources/syntax_highlighter.h"

class AIReferenceSyntaxHighlighter : public SyntaxHighlighter {
	GDCLASS(AIReferenceSyntaxHighlighter, SyntaxHighlighter);

protected:
	static void _bind_methods() {}

public:
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;
};

class AIReferenceTextEdit : public TextEdit {
	GDCLASS(AIReferenceTextEdit, TextEdit);

	void _ensure_syntax_highlighter();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	AIReferenceTextEdit();
};
