/**************************************************************************/
/*  next_engine_update_checker.cpp                                        */
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

#include "next_engine_update_checker.h"

#include "core/error/error_macros.h"
#include "core/io/json.h"
#include "core/next_version.h"
#include "core/version.h"
#include "scene/main/http_request.h"

namespace {

constexpr char NEXT_ENGINE_LATEST_RELEASE_URL[] = "https://api.github.com/repos/Fantety/next-engine/releases/latest";
constexpr char NEXT_ENGINE_DOWNLOAD_URL[] = "https://nextengine.net";

NextEngineUpdateCheckResult _make_error_result(const String &p_message, int p_request_result = OK, int p_response_code = 0) {
	NextEngineUpdateCheckResult result;
	result.status = NextEngineUpdateCheckStatus::ERROR;
	result.message = p_message;
	result.request_result = p_request_result;
	result.response_code = p_response_code;
	return result;
}

String _current_version_or_default(const String &p_current_version) {
	return p_current_version.is_empty() ? String(NEXT_VERSION_FULL_CONFIG) : p_current_version;
}

} // namespace

String get_next_engine_latest_release_url() {
	return NEXT_ENGINE_LATEST_RELEASE_URL;
}

String get_next_engine_download_url() {
	return NEXT_ENGINE_DOWNLOAD_URL;
}

Vector<String> get_next_engine_update_request_headers() {
	Vector<String> headers;
	headers.push_back("Accept: application/vnd.github+json");
	headers.push_back("User-Agent: NEXT-Engine");
	headers.push_back("X-GitHub-Api-Version: 2022-11-28");
	return headers;
}

Error request_next_engine_latest_release(HTTPRequest *p_http) {
	ERR_FAIL_NULL_V(p_http, ERR_INVALID_PARAMETER);
	return p_http->request(get_next_engine_latest_release_url(), get_next_engine_update_request_headers());
}

NextEngineUpdateCheckResult parse_next_engine_latest_release(const Dictionary &p_release_info, const String &p_current_version) {
	String latest_version = p_release_info.get("tag_name", String());
	if (latest_version.is_empty()) {
		latest_version = p_release_info.get("name", String());
	}
	if (latest_version.is_empty()) {
		return _make_error_result("GitHub release does not include a version tag.");
	}

	NextEngineVersion parsed_latest_version;
	if (!parse_next_engine_version(latest_version, parsed_latest_version)) {
		return _make_error_result(vformat("GitHub release version is not valid: %s.", latest_version));
	}

	const String current_version = _current_version_or_default(p_current_version);
	NextEngineVersion parsed_current_version;
	if (!parse_next_engine_version(current_version, parsed_current_version)) {
		return _make_error_result(vformat("Current NEXT Engine version is not valid: %s.", current_version));
	}

	NextEngineUpdateCheckResult result;
	result.latest_version = latest_version;
	if (compare_next_engine_versions(parsed_latest_version, parsed_current_version) > 0) {
		result.status = NextEngineUpdateCheckStatus::UPDATE_AVAILABLE;
		result.message = vformat("Update available: %s.", latest_version);
	} else {
		result.status = NextEngineUpdateCheckStatus::UP_TO_DATE;
		result.message = "You are using the latest version.";
	}
	return result;
}

NextEngineUpdateCheckResult parse_next_engine_update_response(int p_result, int p_response_code, const PackedByteArray &p_body, const String &p_current_version) {
	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		return _make_error_result(vformat("Failed to check for updates. Error: %d.", p_result), p_result, p_response_code);
	}

	if (p_response_code != 200) {
		return _make_error_result(vformat("Failed to check for updates. Response code: %d.", p_response_code), p_result, p_response_code);
	}

	const uint8_t *body_ptr = p_body.ptr();
	const String body_text = String::utf8(reinterpret_cast<const char *>(body_ptr), p_body.size());
	const Variant parsed = JSON::parse_string(body_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		return _make_error_result("Received JSON data is not a valid GitHub release.", p_result, p_response_code);
	}

	NextEngineUpdateCheckResult result = parse_next_engine_latest_release(parsed, p_current_version);
	result.request_result = p_result;
	result.response_code = p_response_code;
	return result;
}
