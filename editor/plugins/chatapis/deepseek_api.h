// deepseek_api.h
#ifndef DEEPSEEK_API_H
#define DEEPSEEK_API_H

#include "core/object/ref_counted.h"
#include "core/io/http_client.h"
#include "core/io/json.h"

class DeepSeekAPI : public RefCounted {
    GDCLASS(DeepSeekAPI, RefCounted);

private:
    Ref<HTTPClient> http_client;
    String api_key;
    String base_url = "https://api.deepseek.com/v1";
    
    void _parse_response(int p_status, PackedByteArray p_body);
    void _handle_request_completed(int p_status, PackedByteArray p_body);

protected:
    static void _bind_methods();

public:
    enum Error {
        OK = 0,
        INVALID_REQUEST,
        NETWORK_ERROR,
        INVALID_RESPONSE,
        API_ERROR
    };

    void set_api_key(const String &p_key);
    String get_api_key() const;
    
    Error send_message(const String &p_message, const Callable &p_callback);
    
    DeepSeekAPI();
    ~DeepSeekAPI();
};

VARIANT_ENUM_CAST(DeepSeekAPI::Error);

#endif // DEEPSEEK_API_H