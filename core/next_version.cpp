/**************************************************************************/
/*  next_version.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "next_version.h"

namespace {

struct StatusRule {
	const char *name;
	int rank;
};

bool _parse_int_component(const String &p_component, int &r_value) {
	if (p_component.is_empty()) {
		return false;
	}

	for (int i = 0; i < p_component.length(); i++) {
		if (!is_digit(p_component[i])) {
			return false;
		}
	}

	r_value = p_component.to_int();
	return true;
}

bool _status_starts_with_family(const String &p_status, const String &p_family) {
	if (!p_status.begins_with(p_family)) {
		return false;
	}

	if (p_status.length() == p_family.length()) {
		return true;
	}

	const char32_t next = p_status[p_family.length()];
	return is_digit(next) || next == '.' || next == '-';
}

int _get_status_rank(const String &p_status, int *r_index) {
	if (r_index) {
		*r_index = 0;
	}

	const String lower_status = p_status.to_lower();
	if (lower_status.is_empty() || lower_status == "stable") {
		return 100;
	}

	static const StatusRule status_rules[] = {
		{ "dev", 0 },
		{ "alpha", 10 },
		{ "beta", 20 },
		{ "preview", 30 },
		{ "rc", 40 },
	};

	for (const StatusRule &rule : status_rules) {
		const String family = rule.name;
		if (!_status_starts_with_family(lower_status, family)) {
			continue;
		}

		String index_text = lower_status.substr(family.length()).trim_prefix(".").trim_prefix("-");
		const int dot_pos = index_text.find_char('.');
		if (dot_pos >= 0) {
			index_text = index_text.substr(0, dot_pos);
		}

		int index = 0;
		if (!index_text.is_empty() && _parse_int_component(index_text, index) && r_index) {
			*r_index = index;
		}
		return rule.rank;
	}

	return 1;
}

int _compare_int(int p_a, int p_b) {
	if (p_a == p_b) {
		return 0;
	}
	return p_a > p_b ? 1 : -1;
}

} // namespace

bool parse_next_engine_version(const String &p_text, NextEngineVersion &r_version) {
	String text = p_text.strip_edges();
	if (text.begins_with("refs/tags/")) {
		text = text.trim_prefix("refs/tags/");
	}
	if (text.begins_with("v") || text.begins_with("V")) {
		text = text.substr(1);
	}

	const int build_metadata_pos = text.find_char('+');
	if (build_metadata_pos >= 0) {
		text = text.substr(0, build_metadata_pos);
	}

	String version_text = text;
	String status;
	const int status_pos = text.find_char('-');
	if (status_pos >= 0) {
		version_text = text.substr(0, status_pos);
		status = text.substr(status_pos + 1).strip_edges();
	}
	if (status.to_lower() == "stable") {
		status = String();
	}

	const Vector<String> components = version_text.split(".", false);
	if (components.size() != 5) {
		return false;
	}

	int parsed_components[5] = {};
	for (int i = 0; i < 5; i++) {
		if (!_parse_int_component(components[i], parsed_components[i])) {
			return false;
		}
	}

	r_version.next_major = parsed_components[0];
	r_version.next_minor = parsed_components[1];
	r_version.godot_major = parsed_components[2];
	r_version.godot_minor = parsed_components[3];
	r_version.godot_patch = parsed_components[4];
	r_version.status = status;
	return true;
}

int compare_next_engine_versions(const NextEngineVersion &p_a, const NextEngineVersion &p_b) {
	int result = _compare_int(p_a.next_major, p_b.next_major);
	if (result != 0) {
		return result;
	}
	result = _compare_int(p_a.next_minor, p_b.next_minor);
	if (result != 0) {
		return result;
	}
	result = _compare_int(p_a.godot_major, p_b.godot_major);
	if (result != 0) {
		return result;
	}
	result = _compare_int(p_a.godot_minor, p_b.godot_minor);
	if (result != 0) {
		return result;
	}
	result = _compare_int(p_a.godot_patch, p_b.godot_patch);
	if (result != 0) {
		return result;
	}

	int a_status_index = 0;
	int b_status_index = 0;
	const int a_status_rank = _get_status_rank(p_a.status, &a_status_index);
	const int b_status_rank = _get_status_rank(p_b.status, &b_status_index);
	result = _compare_int(a_status_rank, b_status_rank);
	if (result != 0) {
		return result;
	}
	result = _compare_int(a_status_index, b_status_index);
	if (result != 0) {
		return result;
	}

	if (p_a.status == p_b.status) {
		return 0;
	}
	return p_a.status > p_b.status ? 1 : -1;
}

bool is_next_engine_version_newer(const String &p_latest, const String &p_current) {
	NextEngineVersion latest_version;
	NextEngineVersion current_version;
	if (!parse_next_engine_version(p_latest, latest_version) || !parse_next_engine_version(p_current, current_version)) {
		return false;
	}
	return compare_next_engine_versions(latest_version, current_version) > 0;
}
