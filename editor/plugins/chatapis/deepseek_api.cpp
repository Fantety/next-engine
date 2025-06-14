// deepseek_api.cpp
#include "deepseek_api.h"
#include "core/config/engine.h"
#include "core/io/json.h"

void DeepSeekAPI::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_api_key", "key"), &DeepSeekAPI::set_api_key);
    ClassDB::bind_method(D_METHOD("get_api_key"), &DeepSeekAPI::get_api_key);
    ClassDB::bind_method(D_METHOD("send_message", "message", "callback"), &DeepSeekAPI::send_message);

    BIND_ENUM_CONSTANT(OK);
    BIND_ENUM_CONSTANT(INVALID_REQUEST);
    BIND_ENUM_CONSTANT(NETWORK_ERROR);
    BIND_ENUM_CONSTANT(INVALID_RESPONSE);
    BIND_ENUM_CONSTANT(API_ERROR);
}

DeepSeekAPI::DeepSeekAPI() {
    http_client.instantiate();
}

DeepSeekAPI::~DeepSeekAPI() {
    if (http_client.is_valid()) {
        http_client.unref();
    }
}

void DeepSeekAPI::set_api_key(const String &p_key) {
    api_key = p_key;
}

String DeepSeekAPI::get_api_key() const {
    return api_key;
}

DeepSeekAPI::Error DeepSeekAPI::send_message(const String &p_message, const Callable &p_callback) {
    if (api_key.is_empty()) {
        return INVALID_REQUEST;
    }

    if (!http_client.is_valid()) {
        return NETWORK_ERROR;
    }

    String url = base_url + "/chat/completions";
    Error err = http_client->connect_to_host(url);
    if (err != OK) {
        return NETWORK_ERROR;
    }

    Dictionary request_data;
    request_data["model"] = "deepseek-chat";
    request_data["messages"] = Array::make(Dictionary::make("role", "user", "content", p_message));
    request_data["temperature"] = 0.7;

    String request_body = JSON::stringify(request_data);
    PackedStringArray headers;
    headers.push_back("Content-Type: application/json");
    headers.push_back("Authorization: Bearer " + api_key);

    err = http_client->request(HTTPClient::METHOD_POST, "/v1/chat/completions", headers, request_body);
    if (err != OK) {
        return NETWORK_ERROR;
    }

    // 设置回调处理响应
    http_client->set_blocking_mode(false);
    http_client->set_read_chunk_size(4096);
    http_client->set_response_body_max_size(1024 * 1024); // 1MB

    // 使用信号连接回调
    http_client->connect("request_completed", callable_mp(this, &DeepSeekAPI::_handle_request_completed).bind(p_callback));

    return OK;
}

void DeepSeekAPI::_handle_request_completed(int p_status, PackedByteArray p_body, const Callable &p_callback) {
    if (p_status != HTTPClient::RESPONSE_OK) {
        p_callback.call(Error::NETWORK_ERROR, Variant());
        return;
    }

    _parse_response(p_status, p_body, p_callback);
}

void DeepSeekAPI::_parse_response(int p_status, PackedByteArray p_body, const Callable &p_callback) {
    String response_text;
    response_text.parse_utf8((const char *)p_body.ptr(), p_body.size());

    Ref<JSON> json;
    json.instantiate();
    Error err = json->parse(response_text);
    if (err != OK) {
        p_callback.call(Error::INVALID_RESPONSE, Variant());
        return;
    }

    Variant response_data = json->get_data();
    if (response_data.get_type() != Variant::DICTIONARY) {
        p_callback.call(Error::INVALID_RESPONSE, Variant());
        return;
    }

    Dictionary response_dict = response_data;
    if (response_dict.has("error")) {
        p_callback.call(Error::API_ERROR, response_dict["error"]);
        return;
    }

    if (response_dict.has("choices") && response_dict["choices"].get_type() == Variant::ARRAY) {
        Array choices = response_dict["choices"];
        if (choices.size() > 0) {
            Dictionary choice = choices[0];
            if (choice.has("message") && choice["message"].get_type() == Variant::DICTIONARY) {
                Dictionary message = choice["message"];
                p_callback.call(Error::OK, message["content"]);
                return;
            }
        }
    }

    p_callback.call(Error::INVALID_RESPONSE, Variant());
}