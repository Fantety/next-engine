/**************************************************************************/
/*  ai_tool.h                                                             */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

struct AIToolResult {
	String content;
	String error;
	Dictionary metadata;
	bool truncated = false;

	bool is_error() const;
	Dictionary to_dict() const;
	static AIToolResult from_dict(const Dictionary &p_dict);
};

class AITool : public RefCounted {
	GDCLASS(AITool, RefCounted);

protected:
	static void _bind_methods();

public:
	virtual String get_name() const = 0;
	virtual String get_description() const = 0;
	virtual Dictionary get_parameters_schema() const = 0;
	virtual AIToolResult execute(const Dictionary &p_arguments) = 0;

	Dictionary get_openai_schema() const;
};
