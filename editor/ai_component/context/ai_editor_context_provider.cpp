/**************************************************************************/
/*  ai_editor_context_provider.cpp                                         */
/**************************************************************************/

#include "ai_editor_context_provider.h"

#include "editor/ai_component/context/ai_context_document.h"
#include "editor/ai_component/context/ai_editor_context_snapshot.h"

void AIEditorContextProvider::_bind_methods() {
}

AIEditorContextProvider::AIEditorContextProvider() {
	agent_profile = AIAgentProfile::get_ask_profile();
}

void AIEditorContextProvider::set_agent_profile(const AIAgentProfile &p_profile) {
	agent_profile = p_profile;
}

AIAgentProfile AIEditorContextProvider::get_agent_profile() const {
	return agent_profile;
}

Array AIEditorContextProvider::collect_context() {
	Ref<AIEditorContextSnapshotService> snapshot_service;
	snapshot_service.instantiate();
	AIEditorContextSnapshotResult snapshot = snapshot_service->collect(agent_profile.get_capabilities_id(), agent_profile.get_capabilities_summary());

	AIContextDocument doc;
	doc.title = "Editor Context";
	doc.source = "editor";
	if (snapshot.success) {
		doc.content = snapshot.content;
	} else {
		doc.content = "Godot Editor AI context capabilities: " + agent_profile.get_capabilities_summary() + "\nEditor context details are unavailable: " + snapshot.error;
	}

	Array result;
	result.push_back(doc.to_dict());
	return result;
}
