/**************************************************************************/
/*  ai_editor_context_provider.cpp                                         */
/**************************************************************************/

#include "ai_editor_context_provider.h"

#include "editor/ai_component/context/ai_context_document.h"
#include "editor/ai_component/context/ai_editor_context_snapshot.h"

void AIEditorContextProvider::_bind_methods() {
}

Array AIEditorContextProvider::collect_context() {
	Ref<AIEditorContextSnapshotService> snapshot_service;
	snapshot_service.instantiate();
	AIEditorContextSnapshotResult snapshot = snapshot_service->collect();

	AIContextDocument doc;
	doc.title = "Editor Context";
	doc.source = "editor";
	if (snapshot.success) {
		doc.content = snapshot.content;
	} else {
		doc.content = "Godot Editor AI context is read-only in this phase.\nEditor context details are unavailable: " + snapshot.error;
	}

	Array result;
	result.push_back(doc.to_dict());
	return result;
}
