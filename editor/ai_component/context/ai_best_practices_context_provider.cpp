/**************************************************************************/
/*  ai_best_practices_context_provider.cpp                                */
/**************************************************************************/

#include "ai_best_practices_context_provider.h"

#include "core/object/class_db.h"

#include "editor/ai_component/agent/best_practices.gen.h"
#include "editor/ai_component/context/ai_context_document.h"

void AIBestPracticesContextProvider::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_chars", "max_chars"), &AIBestPracticesContextProvider::set_max_chars);
	ClassDB::bind_method(D_METHOD("get_max_chars"), &AIBestPracticesContextProvider::get_max_chars);
}

void AIBestPracticesContextProvider::set_max_chars(int p_max_chars) {
	max_chars = MAX(1024, p_max_chars);
}

int AIBestPracticesContextProvider::get_max_chars() const {
	return max_chars;
}

Array AIBestPracticesContextProvider::collect_context() {
	Array result;

	String content = String::utf8(AI_AGENT_BEST_PRACTICES_MARKDOWN);
	bool truncated = false;
	if (content.length() > max_chars) {
		content = content.substr(0, max_chars) + "\n[truncated]";
		truncated = true;
	}

	AIContextDocument doc;
	doc.title = "Agent Best Practices";
	doc.source = "editor/ai_component/agent/best_practices.md";
	doc.content = content;
	doc.truncated = truncated;
	result.push_back(doc.to_dict());
	return result;
}
