/**************************************************************************/
/*  ai_local_settings_store.h                                             */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/core/base/ai_error.h"

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"

class AILocalSettingsStore : public RefCounted {
	GDCLASS(AILocalSettingsStore, RefCounted);

	String settings_path;
	mutable Mutex mutex;

	static bool _read_settings_file(const String &p_path, Dictionary &r_settings, AIError &r_error);
	static bool _write_settings_file(const String &p_path, const Dictionary &p_settings, AIError &r_error);
	static Dictionary _merge_dicts(const Dictionary &p_base, const Dictionary &p_patch);

protected:
	static void _bind_methods();

public:
	AILocalSettingsStore();

	void set_settings_path(const String &p_path);
	String get_settings_path() const;

	Dictionary get_settings();
	Dictionary update_settings(const Dictionary &p_patch);
	Dictionary clear_settings();
};
