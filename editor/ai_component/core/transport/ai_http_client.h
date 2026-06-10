/**************************************************************************/
/*  ai_http_client.h                                                      */
/**************************************************************************/

#pragma once

#include "editor/ai_component/core/base/ai_cancel_token.h"
#include "editor/ai_component/core/transport/ai_sse_parser.h"

#include "core/io/http_client.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"
#include "core/variant/callable.h"

struct AIHTTPEndpoint {
	String scheme;
	String host;
	String path;
	int port = 0;
	bool use_tls = false;
};

struct AIHTTPRequest {
	HTTPClient::Method method = HTTPClient::METHOD_GET;
	String url;
	Vector<String> headers;
	PackedByteArray body;
	int timeout_msec = 30000;
	int poll_interval_usec = 1000;
	int max_response_body_bytes = 64 * 1024 * 1024;
	bool fail_on_http_error = false;
	bool store_stream_body = false;
	String label;
	Ref<AICancelToken> cancel_token;
};

struct AIHTTPResponse {
	bool headers_received = false;
	int response_code = 0;
	Vector<String> headers;
	PackedByteArray body;

	bool is_success_status_code() const;
	String get_body_as_text() const;
	String get_header_value(const String &p_name) const;
};

class AIHTTPClient : public RefCounted {
	GDCLASS(AIHTTPClient, RefCounted);

	static constexpr int STREAM_PREVIEW_BYTES = 8192;

	static bool _is_cancel_requested(const Ref<AICancelToken> &p_cancel_token);
	static String _get_cancel_message(const Ref<AICancelToken> &p_cancel_token);
	static bool _capture_response_metadata(const Ref<HTTPClient> &p_client, HTTPClient::Status p_status, AIHTTPResponse &r_response);
	static bool _append_body_limited(PackedByteArray &r_body, const PackedByteArray &p_chunk, int p_limit, String &r_error);
	static bool _call_event_callback(const Callable &p_callback, const AISSEEvent &p_event, bool &r_stop_requested, String &r_error);
	static String _make_http_error_preview(const AIHTTPResponse &p_response);

	bool _connect(const AIHTTPEndpoint &p_endpoint, const AIHTTPRequest &p_request, Ref<HTTPClient> &r_client, String &r_error) const;
	bool _send_request(const Ref<HTTPClient> &p_client, const AIHTTPEndpoint &p_endpoint, const AIHTTPRequest &p_request, String &r_error) const;
	bool _read_response(const Ref<HTTPClient> &p_client, const AIHTTPRequest &p_request, AIHTTPResponse &r_response, String &r_error) const;
	bool _read_sse_response(const Ref<HTTPClient> &p_client, const AIHTTPRequest &p_request, const Callable &p_event_callback, AIHTTPResponse &r_response, String &r_error) const;

protected:
	static void _bind_methods();

public:
	static String normalize_connection_host(const String &p_host);
	static bool parse_endpoint(const String &p_url, AIHTTPEndpoint &r_endpoint, String &r_error);
	static String join_paths(const String &p_base_path, const String &p_endpoint_path);

	bool request(const AIHTTPRequest &p_request, AIHTTPResponse &r_response, String &r_error) const;
	bool request_sse(const AIHTTPRequest &p_request, const Callable &p_event_callback, AIHTTPResponse &r_response, String &r_error) const;
};
