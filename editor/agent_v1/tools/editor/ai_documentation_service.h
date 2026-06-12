/**************************************************************************/
/*  ai_documentation_service.h                                            */
/**************************************************************************/

#pragma once

#include "core/object/property_info.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "editor/agent_v1/tools/editor/ai_editor_tool_service.h"

class Object;

struct AIV1DocumentationResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

class AIV1DocumentationService : public AIV1EditorToolService {
	GDCLASS(AIV1DocumentationService, AIV1EditorToolService);

	struct MainThreadRequest {
		String query;
		String class_name;
		String kind;
		int max_results = 20;
		bool include_descriptions = true;
		AIV1DocumentationResult result;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	static AIV1DocumentationService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AIV1DocumentationResult _dispatch_to_main_thread(MainThreadRequest &r_request);
	AIV1DocumentationResult _search_main_thread(const String &p_query, const String &p_class_name, const String &p_kind, int p_max_results, bool p_include_descriptions) const;

protected:
	static void _bind_methods();

public:
	AIV1DocumentationResult search(const String &p_query, const String &p_class_name, const String &p_kind, int p_max_results, bool p_include_descriptions);

	static Array get_writable_property_suggestions(Object *p_object, const String &p_property_path, int p_max_suggestions = 5);
	static String format_property_suggestions_for_error(Object *p_object, const String &p_object_label, const String &p_property_path, int p_max_suggestions = 5);
};
