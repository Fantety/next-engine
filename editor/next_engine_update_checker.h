/**************************************************************************/
/*  next_engine_update_checker.h                                           */
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
