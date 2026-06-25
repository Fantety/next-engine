/**************************************************************************/
/*  next_file_logger.cpp                                                  */
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

#include "next_file_logger.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/os/time.h"

namespace {

Mutex log_mutex;
String log_path_override;

const char *level_to_string(NextFileLogger::Level p_level) {
	switch (p_level) {
		case NextFileLogger::LEVEL_DEBUG:
			return "DEBUG";
		case NextFileLogger::LEVEL_INFO:
			return "INFO";
		case NextFileLogger::LEVEL_WARNING:
			return "WARNING";
		case NextFileLogger::LEVEL_ERROR:
			return "ERROR";
	}
	return "UNKNOWN";
}

String get_log_path_unlocked() {
	if (!log_path_override.is_empty()) {
		return log_path_override;
	}
	return NextFileLogger::DEFAULT_LOG_PATH;
}

String escape_log_value(const String &p_value) {
	String escaped;
	for (int i = 0; i < p_value.length(); i++) {
		const char32_t ch = p_value[i];
		switch (ch) {
			case '\\':
				escaped += "\\\\";
				break;
			case '"':
				escaped += "\\\"";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\r':
				escaped += "\\r";
				break;
			case '\t':
				escaped += "\\t";
				break;
			default:
				escaped += String::chr(ch);
				break;
		}
	}
	return escaped;
}

String quote_log_value(const String &p_value) {
	return "\"" + escape_log_value(p_value) + "\"";
}

} // namespace

void NextFileLogger::log(Level p_level, const String &p_category, const String &p_message, const char *p_file, const char *p_function, int p_line) {
	MutexLock lock(log_mutex);

	const String path = get_log_path_unlocked();
	if (path.is_empty()) {
		return;
	}

	const String base_dir = path.get_base_dir();
	if (!base_dir.is_empty()) {
		const Error dir_err = DirAccess::make_dir_recursive_absolute(base_dir);
		if (dir_err != OK) {
			return;
		}
	}

	Error err = OK;
	const bool exists = FileAccess::exists(path);
	Ref<FileAccess> file = FileAccess::open(path, exists ? FileAccess::READ_WRITE : FileAccess::WRITE_READ, &err);
	if (err != OK || file.is_null()) {
		return;
	}

	file->seek_end();

	const String timestamp = Time::get_singleton()->get_datetime_string_from_system(false, true);
	const String source = vformat("%s:%d", String::utf8(p_file ? p_file : ""), p_line);
	const String function = String::utf8(p_function ? p_function : "");
	const String line = vformat(
			"timestamp=%s level=%s category=%s thread_id=%d source=%s function=%s message=%s",
			quote_log_value(timestamp),
			level_to_string(p_level),
			quote_log_value(p_category),
			Thread::get_caller_id(),
			quote_log_value(source),
			quote_log_value(function),
			quote_log_value(p_message));

	file->store_line(line);
	file->flush();
}

void NextFileLogger::set_log_path(const String &p_path) {
	MutexLock lock(log_mutex);
	log_path_override = p_path.simplify_path();
}

void NextFileLogger::reset_log_path() {
	MutexLock lock(log_mutex);
	log_path_override = String();
}

String NextFileLogger::get_log_path() {
	MutexLock lock(log_mutex);
	return get_log_path_unlocked();
}
