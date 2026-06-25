/**************************************************************************/
/*  test_next_file_logger.cpp                                             */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_next_file_logger);

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "editor/next_file_logger.h"

namespace TestNextFileLogger {

static const String test_log_path = "user://logs/next-editor-debug.log";

void initialize_logs() {
	ProjectSettings::get_singleton()->set_setting("application/config/name", "godot_tests");
	DirAccess::make_dir_recursive_absolute(OS::get_singleton()->get_user_data_dir().path_join("logs"));
	if (FileAccess::exists(test_log_path)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(test_log_path));
	}
	NextFileLogger::set_log_path(test_log_path);
}

void cleanup_logs() {
	NextFileLogger::reset_log_path();
	ProjectSettings::get_singleton()->set_setting("application/config/name", "godot_tests");
	if (FileAccess::exists(test_log_path)) {
		DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(test_log_path));
	}
	DirAccess::remove_absolute(OS::get_singleton()->get_user_data_dir().path_join("logs"));
	DirAccess::remove_absolute(OS::get_singleton()->get_user_data_dir());
}

TEST_CASE("[Editor][NextFileLogger] Writes detailed diagnostic lines to file") {
	initialize_logs();

	NextFileLogger::log(NextFileLogger::LEVEL_DEBUG, "AI Agent", "tool started\nwith detail", "editor/test_next_file_logger.cpp", "test_function", 74);

	Error err = OK;
	Ref<FileAccess> log = FileAccess::open(test_log_path, FileAccess::READ, &err);
	REQUIRE_EQ(err, OK);
	const String text = log->get_as_text();

	CHECK(text.contains("timestamp=\""));
	CHECK(text.contains("level=DEBUG"));
	CHECK(text.contains("category=\"AI Agent\""));
	CHECK(text.contains("thread_id="));
	CHECK(text.contains("source=\"editor/test_next_file_logger.cpp:74\""));
	CHECK(text.contains("function=\"test_function\""));
	CHECK(text.contains("message=\"tool started\\nwith detail\""));

	cleanup_logs();
}

} // namespace TestNextFileLogger
