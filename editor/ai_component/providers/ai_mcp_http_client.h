/**************************************************************************/
/*  ai_mcp_http_client.h                                                  */
/**************************************************************************/

#pragma once

#include "editor/ai_component/providers/ai_mcp_client.h"

#include "core/io/http_client.h"
#include "core/templates/list.h"

class AIMCPHTTPClient : public AIMCPClient {
	GDCLASS(AIMCPHTTPClient, AIMCPClient);

	String session_id;

	struct Endpoint {
		String scheme;
		String host;
		String path;
		int port = 0;
		bool use_tls = false;
	};

	struct ResponseMetadata {
		bool headers_received = false;
		int response_code = 0;
		String session_id;
	};

	static String _normalize_connection_host(const String &p_host);
	static bool _parse_endpoint(const String &p_url, Endpoint &r_endpoint, String &r_error);
	static Vector<String> _build_headers(const String &p_extra_headers, const String &p_session_id, bool p_accept_sse);
	static String _join_paths(const String &p_base_path, const String &p_endpoint_path);
	static bool _split_sse_data_events(const String &p_text, bool p_require_complete_events, Vector<String> &r_events);
	static int _find_sse_event_separator(const PackedByteArray &p_buffer, int &r_separator_size);
	static bool _consume_sse_data_events(PackedByteArray &r_buffer, Vector<String> &r_events);
	static bool _extract_sse_data_events(const String &p_text, Vector<String> &r_events);
	static bool _parse_json_or_sse_response(const String &p_response_text, int p_expected_id, Dictionary &r_result, String &r_error);
	static bool _capture_response_metadata(const Ref<HTTPClient> &p_client, HTTPClient::Status p_status, ResponseMetadata &r_metadata);
	bool _connect(const Endpoint &p_endpoint, Ref<HTTPClient> &r_client, String &r_error) const;
	bool _wait_for_response_headers(const Ref<HTTPClient> &p_client, ResponseMetadata &r_metadata, String &r_error) const;
	bool _read_response_body(const Ref<HTTPClient> &p_client, PackedByteArray &r_body, String &r_error, bool p_allow_open_stream = false, ResponseMetadata *r_metadata = nullptr) const;
	bool _read_streamable_http_response(const Ref<HTTPClient> &p_client, int p_expected_id, ResponseMetadata &r_metadata, Dictionary &r_result, String &r_error) const;
	bool _send_streamable_http_message(const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error);
	bool _extract_sse_endpoint_path(const String &p_event_data, String &r_endpoint_path) const;
	bool _read_sse_endpoint_event(const Ref<HTTPClient> &p_stream_client, ResponseMetadata &r_metadata, String &r_endpoint_path, String &r_error) const;
	bool _open_sse_channel(Ref<HTTPClient> &r_stream_client, Endpoint &r_post_endpoint, String &r_error) const;
	bool _send_legacy_sse_post(const Endpoint &p_post_endpoint, const String &p_request_json, String &r_error) const;
	bool _read_legacy_sse_response(const Ref<HTTPClient> &p_stream_client, int p_expected_id, Dictionary &r_result, String &r_error) const;
	bool _initialize_legacy_sse_channel(const Ref<HTTPClient> &p_stream_client, const Endpoint &p_post_endpoint, String &r_error);
	bool _send_legacy_sse_request(const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error);
	bool _send_legacy_sse_message(const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error);
	bool _send_message(const String &p_request_json, int p_request_id, Dictionary &r_result, String &r_error);
	bool _initialize_session(String &r_error);
	bool _list_tools_legacy_sse(Vector<AIMCPToolDescriptor> &r_tools, String &r_error);
	AIMCPToolCallResult _call_tool_legacy_sse(const String &p_tool_name, const Dictionary &p_arguments);

protected:
	static void _bind_methods();

public:
	static String normalize_connection_host_for_test(const String &p_host);
	static Vector<String> extract_sse_data_events_for_test(const String &p_text);
	static Vector<String> consume_sse_data_events_for_test(PackedByteArray &r_buffer);

	virtual bool initialize(String &r_error) override;
	virtual bool list_tools(Vector<AIMCPToolDescriptor> &r_tools, String &r_error) override;
	virtual AIMCPToolCallResult call_tool(const String &p_tool_name, const Dictionary &p_arguments) override;
	virtual bool list_resources(Vector<AIMCPResourceDescriptor> &r_resources, String &r_error) override;
	virtual AIMCPResourceReadResult read_resource(const String &p_uri) override;
	virtual bool list_prompts(Vector<AIMCPPromptDescriptor> &r_prompts, String &r_error) override;
	virtual AIMCPPromptRenderResult render_prompt(const String &p_prompt_name, const Dictionary &p_arguments) override;
};
