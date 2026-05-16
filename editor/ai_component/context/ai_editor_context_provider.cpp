/**************************************************************************/
/*  ai_editor_context_provider.cpp                                         */
/**************************************************************************/

#include "ai_editor_context_provider.h"

#include "core/object/class_db.h"
#include "editor/editor_node.h"

#include "editor/ai_component/context/ai_context_document.h"

void AIEditorContextProvider::_bind_methods() {
}

Array AIEditorContextProvider::collect_context() {
	AIContextDocument doc;
	doc.title = "Editor Context";
	doc.source = "editor";
	doc.content = "Godot Editor AI context is read-only in this phase.";

	Array result;
	result.push_back(doc.to_dict());
	return result;
}
