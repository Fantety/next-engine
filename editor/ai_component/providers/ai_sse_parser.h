/**************************************************************************/
/*  ai_sse_parser.h                                                        */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"

class AISSEParser : public RefCounted {
	GDCLASS(AISSEParser, RefCounted);

	String pending_data;

protected:
	static void _bind_methods();

public:
	Array push_chunk(const String &p_chunk);
	void reset();
};
