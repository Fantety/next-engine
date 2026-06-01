/**************************************************************************/
/*  ai_session_base.cpp                                                   */
/**************************************************************************/

#include "ai_session_base.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/os/time.h"

void AISessionBase::_bind_methods() {
}

String AISessionBase::_get_project_scope_key() const {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (!project_settings) {
		return "global";
	}

	const String resource_path = project_settings->get_resource_path();
	if (resource_path.is_empty()) {
		return "global";
	}
	return resource_path.md5_text();
}

String AISessionBase::_make_unique_id(const String &p_prefix) const {
	const String unique_id = OS::get_singleton()->get_unique_id() + "_" + itos(Time::get_singleton()->get_unix_time_from_system()) + "_" + itos(Math::rand());
	if (p_prefix.is_empty()) {
		return unique_id;
	}
	return p_prefix + "_" + unique_id;
}

void AISessionBase::_connect_runtime_signals(const Ref<AIAgentRuntimeRunner> &p_runner, const Callable &p_finished_callable, const Callable &p_message_added_callable, const Callable &p_message_updated_callable) const {
	ERR_FAIL_COND(p_runner.is_null());

	p_runner->connect("runtime_finished", p_finished_callable, CONNECT_DEFERRED);
	p_runner->connect("runtime_message_added", p_message_added_callable, CONNECT_DEFERRED);
	p_runner->connect("runtime_message_updated", p_message_updated_callable, CONNECT_DEFERRED);
}
