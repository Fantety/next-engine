/**************************************************************************/
/*  ai_editor_context_snapshot.h                                           */
/**************************************************************************/

#pragma once

#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "editor/agent_v1/tools/editor/ai_editor_tool_service.h"

struct AIEditorContextSnapshotResult {
	bool success = false;
	String error;
	String content;
	Dictionary metadata;
};

class AIV1EditorContextSnapshotService : public AIV1EditorToolService {
	GDCLASS(AIV1EditorContextSnapshotService, AIV1EditorToolService);

	struct MainThreadRequest {
		AIEditorContextSnapshotResult result;
		String capabilities_id;
		String capabilities_summary;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AIEditorContextSnapshotResult _dispatch_to_main_thread(MainThreadRequest &r_request);
	AIEditorContextSnapshotResult _collect_main_thread(const String &p_capabilities_id, const String &p_capabilities_summary) const;

protected:
	static void _bind_methods();

public:
	AIEditorContextSnapshotResult collect(const String &p_capabilities_id = String(), const String &p_capabilities_summary = String());
};
