/**************************************************************************/
/*  ai_editor_context_snapshot.h                                           */
/**************************************************************************/

#pragma once

#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "editor/ai_component/tools/editor/ai_editor_tool_service.h"

struct AIEditorContextSnapshotResult {
	bool success = false;
	String error;
	String content;
	Dictionary metadata;
};

class AIEditorContextSnapshotService : public AIEditorToolService {
	GDCLASS(AIEditorContextSnapshotService, AIEditorToolService);

	struct MainThreadRequest {
		AIEditorContextSnapshotResult result;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	static AIEditorContextSnapshotService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AIEditorContextSnapshotResult _dispatch_to_main_thread(MainThreadRequest &r_request);
	AIEditorContextSnapshotResult _collect_main_thread() const;

protected:
	static void _bind_methods();

public:
	AIEditorContextSnapshotResult collect();
};
