/**************************************************************************/
/*  ai_text_diff_viewer.h                                                  */
/**************************************************************************/

#pragma once

#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "scene/gui/box_container.h"
#include "scene/resources/syntax_highlighter.h"

class CodeEdit;
class HSplitContainer;
class Label;
class OptionButton;

class AITextDiffViewer : public VBoxContainer {
	GDCLASS(AITextDiffViewer, VBoxContainer);

	Label *summary_label = nullptr;
	OptionButton *file_selector = nullptr;
	Label *old_header = nullptr;
	Label *new_header = nullptr;
	CodeEdit *old_editor = nullptr;
	CodeEdit *new_editor = nullptr;
	Array current_changes;

	bool syncing_scroll = false;

	static String _normalize_text(const String &p_text);
	static Vector<String> _split_lines(const String &p_text);
	static String _join_lines(const Vector<String> &p_lines);
	static bool _is_gdscript_language(const String &p_language, const String &p_path);
	static bool _is_gdshader_language(const String &p_language, const String &p_path);
	static Ref<SyntaxHighlighter> _make_syntax_highlighter(const String &p_language, const String &p_path);
	static CodeEdit *_make_code_editor(const String &p_language, const String &p_path);

	void _select_file_change(int p_index);
	void _set_editor_text(CodeEdit *p_editor, const Vector<String> &p_lines);
	void _sync_old_scroll(double p_value);
	void _sync_new_scroll(double p_value);
	void _highlight_differences(const Vector<String> &p_old_lines, const Vector<String> &p_new_lines);

protected:
	static void _bind_methods();

public:
	void set_change_set(const Dictionary &p_change_set);

	AITextDiffViewer();
};
