/**************************************************************************/
/*  ai_diff_service.cpp                                                    */
/**************************************************************************/

#include "ai_diff_service.h"

#include "core/templates/vector.h"
#include "core/variant/variant.h"

namespace {

String _normalize_text(const String &p_text) {
	return p_text.replace("\r\n", "\n").replace("\r", "\n");
}

PackedStringArray _split_lines(const String &p_text) {
	return _normalize_text(p_text).split("\n");
}

String _prefix_lines(const PackedStringArray &p_lines, const String &p_prefix, int &r_count) {
	String output;
	for (int i = 0; i < p_lines.size(); i++) {
		output += p_prefix + p_lines[i] + "\n";
		r_count++;
	}
	return output;
}

} // namespace

String AIDiffService::build_unified_diff(const String &p_path, const String &p_old_text, const String &p_new_text, int &r_added_lines, int &r_removed_lines) {
	r_added_lines = 0;
	r_removed_lines = 0;

	PackedStringArray old_lines = _split_lines(p_old_text);
	PackedStringArray new_lines = _split_lines(p_new_text);
	String diff = "--- " + p_path + "\n+++ " + p_path + "\n";

	const int old_count = old_lines.size();
	const int new_count = new_lines.size();
	if (old_count > 1200 || new_count > 1200) {
		diff += "@@ large-file @@\n";
		diff += _prefix_lines(old_lines, "-", r_removed_lines);
		diff += _prefix_lines(new_lines, "+", r_added_lines);
		return diff;
	}

	Vector<int> table;
	table.resize((old_count + 1) * (new_count + 1));
	for (int i = old_count - 1; i >= 0; i--) {
		for (int j = new_count - 1; j >= 0; j--) {
			const int index = i * (new_count + 1) + j;
			if (old_lines[i] == new_lines[j]) {
				table.write[index] = table[(i + 1) * (new_count + 1) + (j + 1)] + 1;
			} else {
				table.write[index] = MAX(table[(i + 1) * (new_count + 1) + j], table[i * (new_count + 1) + (j + 1)]);
			}
		}
	}

	diff += "@@ change @@\n";
	int i = 0;
	int j = 0;
	while (i < old_count || j < new_count) {
		if (i < old_count && j < new_count && old_lines[i] == new_lines[j]) {
			diff += " " + old_lines[i] + "\n";
			i++;
			j++;
		} else if (j < new_count && (i == old_count || table[i * (new_count + 1) + (j + 1)] >= table[(i + 1) * (new_count + 1) + j])) {
			diff += "+" + new_lines[j] + "\n";
			r_added_lines++;
			j++;
		} else if (i < old_count) {
			diff += "-" + old_lines[i] + "\n";
			r_removed_lines++;
			i++;
		}
	}

	return diff;
}

Dictionary AIDiffService::build_text_change(const String &p_path, const String &p_change_type, const String &p_old_text, const String &p_new_text, const String &p_language, const Dictionary &p_metadata) {
	int added_lines = 0;
	int removed_lines = 0;
	const String diff = build_unified_diff(p_path, p_old_text, p_new_text, added_lines, removed_lines);

	Dictionary change;
	change["path"] = p_path;
	change["type"] = p_change_type;
	change["language"] = p_language;
	change["old_text"] = p_old_text;
	change["new_text"] = p_new_text;
	change["diff"] = diff;
	change["added_lines"] = added_lines;
	change["removed_lines"] = removed_lines;
	change["metadata"] = p_metadata.duplicate(true);
	return change;
}
