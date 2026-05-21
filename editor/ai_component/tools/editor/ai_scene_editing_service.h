/**************************************************************************/
/*  ai_scene_editing_service.h                                            */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

class Node;

struct AISceneEditingResult {
	bool success = false;
	String error;
	String message;
	Dictionary metadata;
};

class AISceneEditingService : public RefCounted {
	GDCLASS(AISceneEditingService, RefCounted);

	struct MainThreadRequest {
		enum Operation {
			OP_CREATE_SCENE,
			OP_ADD_NODE,
			OP_DELETE_NODE,
		};

		Operation operation = OP_CREATE_SCENE;
		String root_type;
		String root_name;
		String parent_path;
		String node_type;
		String node_name;
		String node_path;
		AISceneEditingResult result;
		Semaphore done;
	};

	mutable Mutex request_mutex;

	static AISceneEditingService *get_dispatcher_singleton();
	void _execute_request(uint64_t p_request_ptr);
	void _execute_request_ptr(MainThreadRequest *p_request);
	AISceneEditingResult _dispatch_to_main_thread(MainThreadRequest &r_request);

	Node *_get_edited_scene(String &r_error) const;
	Node *_resolve_node_path(Node *p_scene_root, const String &p_path, bool p_allow_root, String &r_error) const;
	Node *_instantiate_node(const String &p_type, String &r_error) const;
	String _normalize_node_name(Node *p_parent, Node *p_node, const String &p_requested_name) const;
	AISceneEditingResult _create_scene_main_thread(const String &p_root_type, const String &p_root_name);
	AISceneEditingResult _add_node_main_thread(const String &p_parent_path, const String &p_type, const String &p_name);
	AISceneEditingResult _delete_node_main_thread(const String &p_node_path);
	void _select_node(Node *p_node) const;
	void _update_scene_tree() const;

protected:
	static void _bind_methods();

public:
	AISceneEditingResult create_scene(const String &p_root_type, const String &p_root_name);
	AISceneEditingResult add_node(const String &p_parent_path, const String &p_type, const String &p_name);
	AISceneEditingResult delete_node(const String &p_node_path);
};
