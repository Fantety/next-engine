/**************************************************************************/
/*  ai_fake_mcp_server.h                                                  */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class AIFakeMCPServer : public RefCounted {
	GDCLASS(AIFakeMCPServer, RefCounted);

	struct ToolStub {
		Dictionary descriptor;
		Variant result;
		bool fail = false;
		String error;
	};

	bool running = false;
	HashMap<String, ToolStub> tools;
	Array calls;

protected:
	static void _bind_methods();

public:
	void start();
	void stop();
	bool is_running() const;
	void clear();

	void register_tool_struct(const String &p_name, const Dictionary &p_descriptor, const Variant &p_result = Variant(), bool p_fail = false, const String &p_error = String());
	Array list_tools() const;
	Dictionary call_tool(const String &p_name, const Variant &p_input = Variant());
	Array get_calls() const;
};
