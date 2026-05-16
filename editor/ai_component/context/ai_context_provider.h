/**************************************************************************/
/*  ai_context_provider.h                                                  */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"

class AIContextProvider : public RefCounted {
	GDCLASS(AIContextProvider, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual Array collect_context();
};
