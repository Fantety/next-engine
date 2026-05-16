/**************************************************************************/
/*  ai_openai_runtime_client.cpp                                          */
/**************************************************************************/

#include "ai_openai_runtime_client.h"

#include "core/io/http_client.h"
#include "core/os/os.h"

#include "editor/ai_component/providers/ai_openai_compatible_provider.h"

void AIOpenAIRuntimeTransport::_bind_methods() {
}

bool AIOpenAIRuntimeTransport::request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error) {
	(void)p_config;
	(void)p_messages;
	(void)p_tool_schemas;
	(void)r_response_text;
	r_error = "OpenAI runtime transport is not implemented.";
	return false;
}

void AIOpenAIHTTPRuntimeTransport::_bind_methods() {
}

bool AIOpenAIHTTPRuntimeTransport::request_chat_completion(const AIProviderConfig &p_config, const Array &p_messages, const Array &p_tool_schemas, String &r_response_text, String &r_error) {
	r_response_text = String();
	r_error = String();

	if (p_config.api_key.strip_edges().is_empty()) {
		r_error = "API key is not configured.";
		return false;
	}
	if (p_config.base_url.strip_edges().is_empty()) {
		r_error = "Provider base URL is not configured.";
		return false;
	}

	String scheme;
	String host;
	String base_path;
	String fragment;
	int port = 0;
	Error err = p_config.base_url.parse_url(scheme, host, port, base_path, fragment);
	if (err != OK || (scheme != "https://" && scheme != "http://") || host.is_empty()) {
		r_error = "Provider base URL is invalid.";
		return false;
	}

	const bool use_tls = scheme == "https://";
	if (port == 0) {
		port = use_tls ? 443 : 80;
	}

	Ref<HTTPClient> client = HTTPClient::create();
	client->set_blocking_mode(true);
	err = client->connect_to_host(host, port, use_tls ? Ref<TLSOptions>(TLSOptions::client()) : Ref<TLSOptions>());
	if (err != OK) {
		r_error = "Failed to connect to provider.";
		return false;
	}

	const uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	const uint64_t timeout_msec = MAX(1, p_config.timeout_seconds) * 1000;
	while (client->get_status() == HTTPClient::STATUS_CONNECTING || client->get_status() == HTTPClient::STATUS_RESOLVING) {
		client->poll();
		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			r_error = "Provider connection timed out.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	if (client->get_status() != HTTPClient::STATUS_CONNECTED) {
		r_error = "Provider connection failed.";
		return false;
	}

	Vector<String> headers;
	headers.push_back("Authorization: Bearer " + p_config.api_key);
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");

	PackedByteArray body = AIOpenAICompatibleProvider::build_body(p_messages, p_config.model, p_tool_schemas, false);
	String request_path = AIOpenAICompatibleProvider::build_request_path(base_path);
	err = client->request(HTTPClient::METHOD_POST, request_path, headers, body.ptr(), body.size());
	if (err != OK) {
		r_error = vformat("Failed to send provider request (%d): %s", err, request_path);
		return false;
	}

	PackedByteArray response_body;
	while (true) {
		client->poll();
		HTTPClient::Status status = client->get_status();
		if (status == HTTPClient::STATUS_BODY) {
			PackedByteArray chunk = client->read_response_body_chunk();
			if (!chunk.is_empty()) {
				response_body.append_array(chunk);
			}
		} else if (status == HTTPClient::STATUS_DISCONNECTED) {
			break;
		} else if (status >= HTTPClient::STATUS_CONNECTION_ERROR) {
			r_error = "Provider connection error.";
			return false;
		}

		if (OS::get_singleton()->get_ticks_msec() - start_time > timeout_msec) {
			r_error = "Provider request timed out.";
			return false;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	r_response_text = String::utf8(reinterpret_cast<const char *>(response_body.ptr()), response_body.size());
	return true;
}

void AIOpenAICompatibleRuntimeClient::_bind_methods() {
}

AIOpenAICompatibleRuntimeClient::AIOpenAICompatibleRuntimeClient() {
	Ref<AIOpenAIHTTPRuntimeTransport> http_transport;
	http_transport.instantiate();
	transport = http_transport;
}

void AIOpenAICompatibleRuntimeClient::set_config(const AIProviderConfig &p_config) {
	config = p_config;
}

AIProviderConfig AIOpenAICompatibleRuntimeClient::get_config() const {
	return config;
}

void AIOpenAICompatibleRuntimeClient::set_transport(const Ref<AIOpenAIRuntimeTransport> &p_transport) {
	transport = p_transport;
}

Ref<AIOpenAIRuntimeTransport> AIOpenAICompatibleRuntimeClient::get_transport() const {
	return transport;
}

AIAgentRuntimeResponse AIOpenAICompatibleRuntimeClient::complete(const Array &p_messages, const Array &p_tool_schemas) {
	AIAgentRuntimeResponse response;
	if (transport.is_null()) {
		response.error = "OpenAI runtime transport is not configured.";
		return response;
	}

	String response_text;
	String transport_error;
	if (!transport->request_chat_completion(config, p_messages, p_tool_schemas, response_text, transport_error)) {
		response.error = transport_error.is_empty() ? String("Provider request failed.") : transport_error;
		return response;
	}

	String parse_error;
	if (!AIOpenAICompatibleProvider::parse_chat_completion(response_text, response, parse_error)) {
		response.error = parse_error.is_empty() ? String("Failed to parse provider response.") : parse_error;
	}
	return response;
}
