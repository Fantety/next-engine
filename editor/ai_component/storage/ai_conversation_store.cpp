/**************************************************************************/
/*  ai_conversation_store.cpp                                              */
/**************************************************************************/

#include "ai_conversation_store.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/time.h"

#include "editor/ai_component/storage/ai_conversation_serializer.h"

static bool _ai_conversation_updated_desc(const Variant &p_a, const Variant &p_b) {
	Dictionary a = p_a;
	Dictionary b = p_b;
	return uint64_t(a.get("updated_at", 0)) > uint64_t(b.get("updated_at", 0));
}

void AIConversationStore::_bind_methods() {
	ClassDB::bind_method(D_METHOD("list_conversations"), &AIConversationStore::list_conversations);
	ClassDB::bind_method(D_METHOD("delete_conversation", "session_id"), &AIConversationStore::delete_conversation);
}

Error AIConversationStore::_ensure_base_dir() const {
	return DirAccess::make_dir_recursive_absolute(base_dir);
}

String AIConversationStore::_get_session_path(const String &p_session_id) const {
	return base_dir.path_join(p_session_id + ".json");
}

String AIConversationStore::_sanitize_scope_key(const String &p_scope_key) {
	String scope_key = p_scope_key.strip_edges();
	if (scope_key.is_empty()) {
		scope_key = "global";
	}
	return scope_key.validate_filename();
}

void AIConversationStore::set_project_scope(const String &p_project_scope_key) {
	base_dir = String("user://ai_agent/projects").path_join(_sanitize_scope_key(p_project_scope_key)).path_join("conversations");
}

String AIConversationStore::get_base_dir_for_test() const {
	return base_dir;
}

Error AIConversationStore::save_conversation(const String &p_session_id, const String &p_title, const Vector<AIAgentMessage> &p_messages) {
	ERR_FAIL_COND_V(p_session_id.is_empty(), ERR_INVALID_PARAMETER);
	Error err = _ensure_base_dir();
	ERR_FAIL_COND_V(err != OK, err);

	Dictionary root;
	root["id"] = p_session_id;
	root["title"] = p_title;
	root["updated_at"] = Time::get_singleton()->get_unix_time_from_system();
	root["messages"] = AIConversationSerializer::messages_to_array(p_messages);

	Ref<FileAccess> file = FileAccess::open(_get_session_path(p_session_id), FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(file.is_null() || err != OK, err);
	file->store_string(JSON::stringify(root, "\t"));
	return OK;
}

bool AIConversationStore::load_conversation(const String &p_session_id, String &r_title, Vector<AIAgentMessage> &r_messages) const {
	if (p_session_id.is_empty() || !FileAccess::exists(_get_session_path(p_session_id))) {
		return false;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(_get_session_path(p_session_id), FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		return false;
	}

	Ref<JSON> json;
	json.instantiate();
	err = json->parse(file->get_as_text());
	if (err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary root = json->get_data();
	if (root.has("title")) {
		r_title = root["title"];
	}
	if (root.has("messages") && Variant(root["messages"]).get_type() == Variant::ARRAY) {
		r_messages = AIConversationSerializer::messages_from_array(root["messages"]);
	}
	return true;
}

bool AIConversationStore::load_conversation_metadata(const String &p_session_id, Dictionary &r_metadata) const {
	if (p_session_id.is_empty() || !FileAccess::exists(_get_session_path(p_session_id))) {
		return false;
	}

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(_get_session_path(p_session_id), FileAccess::READ, &err);
	if (file.is_null() || err != OK) {
		return false;
	}

	Ref<JSON> json;
	json.instantiate();
	err = json->parse(file->get_as_text());
	if (err != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary root = json->get_data();
	r_metadata["id"] = root.get("id", p_session_id);
	r_metadata["title"] = root.get("title", TTR("New Chat"));
	r_metadata["updated_at"] = root.get("updated_at", 0);
	Array messages = root.get("messages", Array());
	r_metadata["message_count"] = messages.size();
	return true;
}

bool AIConversationStore::delete_conversation(const String &p_session_id) const {
	if (p_session_id.is_empty()) {
		return false;
	}

	const String session_path = _get_session_path(p_session_id);
	if (!FileAccess::exists(session_path)) {
		return false;
	}

	return DirAccess::remove_absolute(session_path) == OK;
}

bool AIConversationStore::get_most_recent_conversation_id(String &r_session_id) const {
	Array conversations = list_conversations();
	if (conversations.is_empty() || Variant(conversations[0]).get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary item = conversations[0];
	r_session_id = item.get("id", String());
	return !r_session_id.is_empty();
}

Array AIConversationStore::list_conversations() const {
	Array conversations;
	Ref<DirAccess> dir = DirAccess::open(base_dir);
	if (dir.is_null()) {
		return conversations;
	}

	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (!dir->current_is_dir() && entry.get_extension().to_lower() == "json") {
			Dictionary item;
			String session_id = entry.get_basename();
			if (load_conversation_metadata(session_id, item)) {
				item["path"] = base_dir.path_join(entry);
				conversations.push_back(item);
			}
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();
	conversations.sort_custom(callable_mp_static(_ai_conversation_updated_desc));
	return conversations;
}
