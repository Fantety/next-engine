/**************************************************************************/
/*  editor_user_session.h                                                 */
/**************************************************************************/

#pragma once

#include "editor/user_system/auth_client.h"

#include "core/variant/dictionary.h"

class EditorUserSession {
	static Dictionary _session_to_dictionary(const AuthSessionData &p_session);
	static AuthSessionData _session_from_dictionary(const Dictionary &p_storage);
	static String _make_device_id();

public:
	static AuthSessionData load_session();
	static void save_session(const AuthSessionData &p_session);
	static void clear_session(bool p_keep_device_id = true);
	static String get_or_create_device_id();

	static Dictionary get_session_storage_for_test();
	static void set_session_storage_for_test(const Dictionary &p_storage);
};
