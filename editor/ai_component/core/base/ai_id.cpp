/**************************************************************************/
/*  ai_id.cpp                                                             */
/**************************************************************************/

#include "ai_id.h"

#include "core/crypto/crypto_core.h"
#include "core/os/os.h"

String AIId::make(const String &p_prefix) {
	uint8_t random_bytes[16];
	CryptoCore::RandomGenerator rng;
	String body;
	if (rng.init() == OK && rng.get_random_bytes(random_bytes, sizeof(random_bytes)) == OK) {
		body = String::hex_encode_buffer(random_bytes, sizeof(random_bytes));
	} else {
		const uint64_t ticks = OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : 0;
		body = String::num_uint64(ticks, 16) + String::num_uint64((p_prefix + itos(ticks)).hash64(), 16);
	}

	const String prefix = p_prefix.strip_edges();
	return prefix.is_empty() ? body : prefix + "_" + body;
}

bool AIId::is_valid_name(const String &p_name, int p_max_length) {
	if (p_name.is_empty() || p_name.length() > p_max_length) {
		return false;
	}

	const char32_t first = p_name[0];
	if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z'))) {
		return false;
	}

	for (int i = 1; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
			return false;
		}
	}
	return true;
}
