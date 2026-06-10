/**************************************************************************/
/*  ai_scoped_registration.h                                              */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"

struct AIRegistrationIdentity {
	String id;
	String name;
	String owner;
	uint64_t generation = 0;
	Dictionary metadata;

	bool is_valid() const;
	bool matches(const AIRegistrationIdentity &p_other) const;
	Dictionary to_dictionary() const;
};

class AIScopedRegistration : public RefCounted {
	GDCLASS(AIScopedRegistration, RefCounted);

	AIRegistrationIdentity identity;
	Callable close_callback;
	bool closed = false;

protected:
	static void _bind_methods();

public:
	~AIScopedRegistration();

	void setup(const AIRegistrationIdentity &p_identity, const Callable &p_close_callback);
	void close();
	bool is_closed() const;
	AIRegistrationIdentity get_identity() const;
	Dictionary get_identity_dict() const;
};
