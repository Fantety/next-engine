/**************************************************************************/
/*  next_engine_update_checker.h                                          */
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

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

class HTTPRequest;

enum class NextEngineUpdateCheckStatus {
	NOT_CHECKED,
	ERROR,
	UPDATE_AVAILABLE,
	UP_TO_DATE,
};

struct NextEngineUpdateCheckResult {
	NextEngineUpdateCheckStatus status = NextEngineUpdateCheckStatus::ERROR;
	String latest_version;
	String message;
	int request_result = OK;
	int response_code = 0;
};

String get_next_engine_latest_release_url();
String get_next_engine_download_url();
Vector<String> get_next_engine_update_request_headers();
Error request_next_engine_latest_release(HTTPRequest *p_http);
NextEngineUpdateCheckResult parse_next_engine_latest_release(const Dictionary &p_release_info, const String &p_current_version = String());
NextEngineUpdateCheckResult parse_next_engine_update_response(int p_result, int p_response_code, const PackedByteArray &p_body, const String &p_current_version = String());
