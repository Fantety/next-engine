/**************************************************************************/
/*  ai_text_diff_viewer.cpp                                                */
/**************************************************************************/

#include "ai_text_diff_viewer.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/code_edit.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/split_container.h"
#include "scene/resources/syntax_highlighter.h"
#include "servers/text/text_server.h"

namespace {

const Color DIFF_ADDED_COLOR = Color(0.18, 0.55, 0.28, 0.28);
const Color DIFF_REMOVED_COLOR = Color(0.72, 0.18, 0.18, 0.28);
const Color DIFF_CLEAR_COLOR = Color(0, 0, 0, 0);

struct DiffAlignedLines {
	Vector<String> old_lines;
	Vector<String> new_lines;
	Vector<int> old_states;
	Vector<int> new_states;
};

enum DiffLineState {
	DIFF_LINE_EQUAL,
	DIFF_LINE_ADDED,
	DIFF_LINE_REMOVED,
	DIFF_LINE_MODIFIED,
};

void _append_pair(DiffAlignedLines &r_aligned, const String &p_old_line, const String &p_new_line, int p_old_state, int p_new_state) {
	r_aligned.old_lines.push_back(p_old_line);
	r_aligned.new_lines.push_back(p_new_line);
	r_aligned.old_states.push_back(p_old_state);
	r_aligned.new_states.push_back(p_new_state);
}

} // namespace

void AITextDiffViewer::_bind_methods() {
}

String AITextDiffViewer::_normalize_text(const String &p_text) {
	String text = p_text.replace("\r\n", "\n").replace("\r", "\n");
	if (text.ends_with("\n")) {
		text = text.substr(0, text.length() - 1);
	}
	return text;
}

Vector<String> AITextDiffViewer::_split_lines(const String &p_text) {
	Vector<String> lines = _normalize_text(p_text).split("\n", true);
	if (lines.size() == 1 && lines[0].is_empty()) {
		lines.clear();
	}
	return lines;
}

String AITextDiffViewer::_join_lines(const Vector<String> &p_lines) {
	return String("\n").join(p_lines);
}

bool AITextDiffViewer::_is_gdscript_language(const String &p_language, const String &p_path) {
	const String language = p_language.to_lower();
	return language == "gdscript" || language == "gd" || p_path.get_extension().to_lower() == "gd";
}

bool AITextDiffViewer::_is_gdshader_language(const String &p_language, const String &p_path) {
	const String language = p_language.to_lower();
	return language == "gdshader" || language == "shader" || p_path.get_extension().to_lower() == "gdshader";
}

Ref<SyntaxHighlighter> AITextDiffViewer::_make_syntax_highlighter(const String &p_language, const String &p_path) {
	StringName class_name;
	if (_is_gdscript_language(p_language, p_path)) {
		class_name = SNAME("GDScriptSyntaxHighlighter");
	} else if (_is_gdshader_language(p_language, p_path) && ClassDB::class_exists(SNAME("GDShaderSyntaxHighlighter"))) {
		class_name = SNAME("GDShaderSyntaxHighlighter");
	}

	if (class_name == StringName() || !ClassDB::class_exists(class_name)) {
		return Ref<SyntaxHighlighter>();
	}

	Object *object = ClassDB::instantiate(class_name);
	SyntaxHighlighter *syntax_highlighter = Object::cast_to<SyntaxHighlighter>(object);
	Ref<SyntaxHighlighter> highlighter(syntax_highlighter);
	if (highlighter.is_null() && object) {
		memdelete(object);
	}
	return highlighter;
}

CodeEdit *AITextDiffViewer::_make_code_editor(const String &p_language, const String &p_path) {
	CodeEdit *editor = memnew(CodeEdit);
	editor->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	editor->set_editable(false);
	editor->set_context_menu_enabled(true);
	editor->set_selecting_enabled(true);
	editor->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_NONE);
	editor->set_autowrap_mode(TextServer::AUTOWRAP_OFF);
	editor->set_draw_line_numbers(true);
	editor->set_draw_fold_gutter(false);
	editor->set_line_folding_enabled(false);
	editor->set_symbol_lookup_on_click_enabled(false);
	editor->set_symbol_tooltip_on_hover_enabled(false);
	editor->set_code_completion_enabled(false);

	Ref<SyntaxHighlighter> syntax_highlighter = _make_syntax_highlighter(p_language, p_path);
	if (syntax_highlighter.is_valid()) {
		editor->set_syntax_highlighter(syntax_highlighter);
	}

	return editor;
}

void AITextDiffViewer::_select_file_change(int p_index) {
	if (!summary_label || !file_selector || !old_header || !new_header || !old_editor || !new_editor) {
		return;
	}
	if (p_index < 0 || p_index >= current_changes.size() || Variant(current_changes[p_index]).get_type() != Variant::DICTIONARY) {
		summary_label->set_text(TTR("No file changes to display."));
		_set_editor_text(old_editor, Vector<String>());
		_set_editor_text(new_editor, Vector<String>());
		return;
	}

	Dictionary change = current_changes[p_index];
	const String path = change.get("path", TTR("Unknown file"));
	const String language = change.get("language", String());
	const String old_text = change.get("old_text", String());
	const String new_text = change.get("new_text", String());
	const int added_lines = change.get("added_lines", 0);
	const int removed_lines = change.get("removed_lines", 0);

	summary_label->set_text(vformat("%s    +%d -%d", path, added_lines, removed_lines));
	summary_label->set_tooltip_text(path);
	old_header->set_text(TTR("Before"));
	new_header->set_text(TTR("After"));

	old_editor->set_syntax_highlighter(_make_syntax_highlighter(language, path));
	new_editor->set_syntax_highlighter(_make_syntax_highlighter(language, path));

	_highlight_differences(_split_lines(old_text), _split_lines(new_text));
}

void AITextDiffViewer::_set_editor_text(CodeEdit *p_editor, const Vector<String> &p_lines) {
	if (!p_editor) {
		return;
	}
	p_editor->set_text(_join_lines(p_lines));
	p_editor->set_caret_line(0, false);
	p_editor->set_caret_column(0, false);
	p_editor->set_v_scroll(0);
	p_editor->set_h_scroll(0);
}

void AITextDiffViewer::_sync_old_scroll(double p_value) {
	if (syncing_scroll || !new_editor) {
		return;
	}
	syncing_scroll = true;
	new_editor->set_v_scroll(p_value);
	syncing_scroll = false;
}

void AITextDiffViewer::_sync_new_scroll(double p_value) {
	if (syncing_scroll || !old_editor) {
		return;
	}
	syncing_scroll = true;
	old_editor->set_v_scroll(p_value);
	syncing_scroll = false;
}

void AITextDiffViewer::_highlight_differences(const Vector<String> &p_old_lines, const Vector<String> &p_new_lines) {
	if (!old_editor || !new_editor) {
		return;
	}

	const int old_count = p_old_lines.size();
	const int new_count = p_new_lines.size();
	Vector<int> table;
	table.resize((old_count + 1) * (new_count + 1));
	for (int i = old_count - 1; i >= 0; i--) {
		for (int j = new_count - 1; j >= 0; j--) {
			const int index = i * (new_count + 1) + j;
			if (p_old_lines[i] == p_new_lines[j]) {
				table.write[index] = table[(i + 1) * (new_count + 1) + (j + 1)] + 1;
			} else {
				table.write[index] = MAX(table[(i + 1) * (new_count + 1) + j], table[i * (new_count + 1) + (j + 1)]);
			}
		}
	}

	DiffAlignedLines aligned;
	int old_index = 0;
	int new_index = 0;
	while (old_index < old_count || new_index < new_count) {
		if (old_index < old_count && new_index < new_count && p_old_lines[old_index] == p_new_lines[new_index]) {
			_append_pair(aligned, p_old_lines[old_index], p_new_lines[new_index], DIFF_LINE_EQUAL, DIFF_LINE_EQUAL);
			old_index++;
			new_index++;
			continue;
		}

		if (old_index < old_count && new_index < new_count) {
			const int delete_score = table[(old_index + 1) * (new_count + 1) + new_index];
			const int add_score = table[old_index * (new_count + 1) + (new_index + 1)];
			if (delete_score == add_score) {
				_append_pair(aligned, p_old_lines[old_index], p_new_lines[new_index], DIFF_LINE_MODIFIED, DIFF_LINE_MODIFIED);
				old_index++;
				new_index++;
				continue;
			}
		}

		if (new_index < new_count && (old_index == old_count || table[old_index * (new_count + 1) + (new_index + 1)] >= table[(old_index + 1) * (new_count + 1) + new_index])) {
			_append_pair(aligned, String(), p_new_lines[new_index], DIFF_LINE_EQUAL, DIFF_LINE_ADDED);
			new_index++;
		} else if (old_index < old_count) {
			_append_pair(aligned, p_old_lines[old_index], String(), DIFF_LINE_REMOVED, DIFF_LINE_EQUAL);
			old_index++;
		}
	}

	_set_editor_text(old_editor, aligned.old_lines);
	_set_editor_text(new_editor, aligned.new_lines);

	for (int i = 0; i < old_editor->get_line_count(); i++) {
		old_editor->set_line_background_color(i, DIFF_CLEAR_COLOR);
	}
	for (int i = 0; i < new_editor->get_line_count(); i++) {
		new_editor->set_line_background_color(i, DIFF_CLEAR_COLOR);
	}

	for (int i = 0; i < aligned.old_states.size(); i++) {
		if (aligned.old_states[i] == DIFF_LINE_REMOVED) {
			old_editor->set_line_background_color(i, DIFF_REMOVED_COLOR);
		} else if (aligned.old_states[i] == DIFF_LINE_MODIFIED) {
			old_editor->set_line_background_color(i, DIFF_REMOVED_COLOR);
		}

		if (aligned.new_states[i] == DIFF_LINE_ADDED) {
			new_editor->set_line_background_color(i, DIFF_ADDED_COLOR);
		} else if (aligned.new_states[i] == DIFF_LINE_MODIFIED) {
			new_editor->set_line_background_color(i, DIFF_ADDED_COLOR);
		}
	}
}

void AITextDiffViewer::set_change_set(const Dictionary &p_change_set) {
	current_changes.clear();
	Array changes = p_change_set.get("changes", Array());
	if (file_selector) {
		file_selector->clear();
	}

	for (int i = 0; i < changes.size(); i++) {
		if (Variant(changes[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		current_changes.push_back(changes[i]);
		Dictionary change = changes[i];
		String path = change.get("path", TTR("Unknown file"));
		if (file_selector) {
			file_selector->add_item(path, current_changes.size() - 1);
			file_selector->set_item_tooltip(file_selector->get_item_count() - 1, path);
		}
	}

	if (current_changes.is_empty() || Variant(current_changes[0]).get_type() != Variant::DICTIONARY) {
		summary_label->set_text(TTR("No file changes to display."));
		_set_editor_text(old_editor, Vector<String>());
		_set_editor_text(new_editor, Vector<String>());
		return;
	}

	if (file_selector) {
		file_selector->select(0);
	}
	_select_file_change(0);
}

AITextDiffViewer::AITextDiffViewer() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_theme_constant_override("separation", 6 * EDSCALE);

	HBoxContainer *summary_bar = memnew(HBoxContainer);
	summary_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	summary_bar->add_theme_constant_override("separation", 6 * EDSCALE);
	add_child(summary_bar);

	file_selector = memnew(OptionButton);
	file_selector->set_custom_minimum_size(Size2(220, 0) * EDSCALE);
	file_selector->set_fit_to_longest_item(false);
	file_selector->connect(SceneStringName(item_selected), callable_mp(this, &AITextDiffViewer::_select_file_change));
	summary_bar->add_child(file_selector);

	summary_label = memnew(Label);
	summary_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	summary_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	summary_bar->add_child(summary_label);

	HSplitContainer *split = memnew(HSplitContainer);
	split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(split);

	VBoxContainer *old_side = memnew(VBoxContainer);
	old_side->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	old_side->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	old_side->add_theme_constant_override("separation", 4 * EDSCALE);
	split->add_child(old_side);

	old_header = memnew(Label);
	old_header->set_text(TTR("Before"));
	old_header->add_theme_font_size_override(SceneStringName(font_size), int(12 * EDSCALE));
	old_side->add_child(old_header);

	old_editor = _make_code_editor(String(), String());
	old_side->add_child(old_editor);

	VBoxContainer *new_side = memnew(VBoxContainer);
	new_side->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	new_side->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	new_side->add_theme_constant_override("separation", 4 * EDSCALE);
	split->add_child(new_side);

	new_header = memnew(Label);
	new_header->set_text(TTR("After"));
	new_header->add_theme_font_size_override(SceneStringName(font_size), int(12 * EDSCALE));
	new_side->add_child(new_header);

	new_editor = _make_code_editor(String(), String());
	new_side->add_child(new_editor);

	if (old_editor->get_v_scroll_bar()) {
		old_editor->get_v_scroll_bar()->connect(SceneStringName(value_changed), callable_mp(this, &AITextDiffViewer::_sync_old_scroll));
	}
	if (new_editor->get_v_scroll_bar()) {
		new_editor->get_v_scroll_bar()->connect(SceneStringName(value_changed), callable_mp(this, &AITextDiffViewer::_sync_new_scroll));
	}
}
