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

	struct ResourceStub {
		Dictionary descriptor;
		Variant content;
		bool fail = false;
		String error;
	};

	struct PromptStub {
		Dictionary descriptor;
		Array messages;
		bool fail = false;
		String error;
	};

	bool running = false;
	HashMap<String, ToolStub> tools;
	HashMap<String, ResourceStub> resources;
	HashMap<String, PromptStub> prompts;
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
	void register_resource_struct(const String &p_uri, const Dictionary &p_descriptor, const Variant &p_content = Variant(), bool p_fail = false, const String &p_error = String());
	Array list_resources() const;
	Dictionary read_resource(const String &p_uri);
	void register_prompt_struct(const String &p_name, const Dictionary &p_descriptor, const Array &p_messages = Array(), bool p_fail = false, const String &p_error = String());
	Array list_prompts() const;
	Dictionary render_prompt(const String &p_name, const Dictionary &p_arguments = Dictionary());
	Array get_calls() const;
};
