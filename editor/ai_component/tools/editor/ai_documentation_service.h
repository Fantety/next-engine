/**************************************************************************/
/*  ai_documentation_service.h                                            */
/**************************************************************************/

#pragma once

#include "core/object/property_info.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "editor/ai_component/tools/editor/ai_editor_tool_service.h"

class Object;

struct AIDocumentationResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

class AIDocumentationService : public AIEditorToolService {
	GDCLASS(AIDocumentationService, AIEditorToolService);

	struct MainThreadRequest {
		String query;
		String class_name;
		String kind;
		int max_results = 20;
		bool include_descriptions = true;
		AIDocumentationResult result;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	static AIDocumentationService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AIDocumentationResult _dispatch_to_main_thread(MainThreadRequest &r_request);
	AIDocumentationResult _search_main_thread(const String &p_query, const String &p_class_name, const String &p_kind, int p_max_results, bool p_include_descriptions) const;

protected:
	static void _bind_methods();

public:
	AIDocumentationResult search(const String &p_query, const String &p_class_name, const String &p_kind, int p_max_results, bool p_include_descriptions);

	static Array get_writable_property_suggestions(Object *p_object, const String &p_property_path, int p_max_suggestions = 5);
	static String format_property_suggestions_for_error(Object *p_object, const String &p_object_label, const String &p_property_path, int p_max_suggestions = 5);
};
