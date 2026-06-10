/**************************************************************************/
/*  ai_scoped_registration.cpp                                            */
/**************************************************************************/

#include "ai_scoped_registration.h"

#include "core/variant/variant.h"

bool AIRegistrationIdentity::is_valid() const {
	return !id.is_empty();
}

bool AIRegistrationIdentity::matches(const AIRegistrationIdentity &p_other) const {
	return id == p_other.id && generation == p_other.generation;
}

Dictionary AIRegistrationIdentity::to_dictionary() const {
	Dictionary result;
	result["id"] = id;
	result["name"] = name;
	result["owner"] = owner;
	result["generation"] = static_cast<int64_t>(generation);
	result["metadata"] = metadata;
	return result;
}

void AIScopedRegistration::_bind_methods() {
}

AIScopedRegistration::~AIScopedRegistration() {
	close();
}

void AIScopedRegistration::setup(const AIRegistrationIdentity &p_identity, const Callable &p_close_callback) {
	close();
	identity = p_identity;
	close_callback = p_close_callback;
	closed = false;
}

void AIScopedRegistration::close() {
	if (closed) {
		return;
	}
	closed = true;

	if (!close_callback.is_valid()) {
		return;
	}

	Dictionary identity_dict = identity.to_dictionary();
	Variant identity_variant = identity_dict;
	const Variant *argptrs[1] = { &identity_variant };
	Variant ret;
	Callable::CallError ce;
	close_callback.callp(argptrs, 1, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_PRINT("Failed to close AI scoped registration: " + Variant::get_callable_error_text(close_callback, argptrs, 1, ce) + ".");
	}
}

bool AIScopedRegistration::is_closed() const {
	return closed;
}

AIRegistrationIdentity AIScopedRegistration::get_identity() const {
	return identity;
}

Dictionary AIScopedRegistration::get_identity_dict() const {
	return identity.to_dictionary();
}
