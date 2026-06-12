/**************************************************************************/
/*  ai_reference_text_edit.cpp                                             */
/**************************************************************************/

#include "ai_reference_text_edit.h"

#include "editor/agent_ui/component/ai_reference_resolver.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"

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

	if (p_what == NOTIFICATION_DRAW) {
		_draw_reference_blocks();
	} else if (p_what == NOTIFICATION_THEME_CHANGED) {
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

void AIReferenceTextEdit::_draw_reference_blocks() {
	const Color accent_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	Color fill_color = accent_color;
	fill_color.a = 0.14;
	Color edge_color = accent_color;
	edge_color.a = 0.34;

	for (int line = 0; line < get_line_count(); line++) {
		const Vector<AIReferenceResolver::ReferenceToken> tokens = AIReferenceResolver::find_reference_tokens_in_line(get_line(line));
		for (const AIReferenceResolver::ReferenceToken &token : tokens) {
			for (int column = token.start_column; column < token.end_column; column++) {
				Rect2 rect = get_rect_at_line_column(line, column);
				if (rect.position.x < 0 || rect.position.y < 0 || rect.size.x <= 0 || rect.size.y <= 0) {
					continue;
				}

				rect = rect.grow_individual(1 * EDSCALE, 1 * EDSCALE, 1 * EDSCALE, 1 * EDSCALE);
				draw_rect(rect, fill_color);
				draw_rect(Rect2(rect.position.x, rect.position.y, rect.size.x, 1 * EDSCALE), edge_color);
				draw_rect(Rect2(rect.position.x, rect.position.y + rect.size.y - 1 * EDSCALE, rect.size.x, 1 * EDSCALE), edge_color);
			}
		}
	}
}
