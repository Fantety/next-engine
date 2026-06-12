/**************************************************************************/
/*  ai_reference_text_edit.cpp                                             */
/**************************************************************************/

#include "ai_reference_text_edit.h"

#include "editor/agent_ui/component/ai_reference_resolver.h"
#include "editor/editor_string_names.h"

Dictionary AIReferenceSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	Dictionary color_map;
	TextEdit *edit = get_text_edit();
	if (!edit || p_line < 0 || p_line >= edit->get_line_count()) {
		return color_map;
	}

	const Color reference_color = edit->get_theme_color(SNAME("accent_color"), EditorStringName(Editor)).lerp(edit->get_font_color(), 0.2);
	const Color default_color = edit->get_font_color();
	Dictionary reference_info;
	reference_info["color"] = reference_color;
	Dictionary default_info;
	default_info["color"] = default_color;

	const Vector<AIReferenceResolver::ReferenceToken> tokens = AIReferenceResolver::find_reference_tokens_in_line(edit->get_line(p_line));
	for (const AIReferenceResolver::ReferenceToken &token : tokens) {
		color_map[token.start_column] = reference_info;
		color_map[token.end_column] = default_info;
	}

	return color_map;
}

void AIReferenceTextEdit::_notification(int p_what) {
	TextEdit::_notification(p_what);

	if (p_what == NOTIFICATION_THEME_CHANGED) {
		Ref<SyntaxHighlighter> highlighter = get_syntax_highlighter();
		if (highlighter.is_valid()) {
			highlighter->clear_highlighting_cache();
		}
	}
}

AIReferenceTextEdit::AIReferenceTextEdit() {
	Ref<AIReferenceSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	set_syntax_highlighter(highlighter);
}
