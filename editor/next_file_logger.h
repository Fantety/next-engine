/**************************************************************************/
/*  next_file_logger.h                                                    */
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

#pragma once

#include "core/string/ustring.h"

class NextFileLogger {
public:
	enum Level {
		LEVEL_DEBUG,
		LEVEL_INFO,
		LEVEL_WARNING,
		LEVEL_ERROR,
	};

	static constexpr const char *DEFAULT_LOG_PATH = "user://net.nextengine/logs/editor-debug.log";

	static void log(Level p_level, const String &p_category, const String &p_message, const char *p_file, const char *p_function, int p_line);
	static void set_log_path(const String &p_path);
	static void reset_log_path();
	static String get_log_path();
};

#define NEXT_FILE_LOG_DEBUG(m_category, m_message) NextFileLogger::log(NextFileLogger::LEVEL_DEBUG, m_category, m_message, __FILE__, __FUNCTION__, __LINE__)
#define NEXT_FILE_LOG_INFO(m_category, m_message) NextFileLogger::log(NextFileLogger::LEVEL_INFO, m_category, m_message, __FILE__, __FUNCTION__, __LINE__)
#define NEXT_FILE_LOG_WARNING(m_category, m_message) NextFileLogger::log(NextFileLogger::LEVEL_WARNING, m_category, m_message, __FILE__, __FUNCTION__, __LINE__)
#define NEXT_FILE_LOG_ERROR(m_category, m_message) NextFileLogger::log(NextFileLogger::LEVEL_ERROR, m_category, m_message, __FILE__, __FUNCTION__, __LINE__)
