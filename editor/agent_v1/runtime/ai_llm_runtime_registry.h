/**************************************************************************/
/*  ai_llm_runtime_registry.h                                             */
/**************************************************************************/

#pragma once

#include "editor/agent_v1/runtime/ai_fake_llm_runtime.h"
#include "editor/agent_v1/runtime/ai_openai_compatible_runtime.h"

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"

class AILLMRuntimeRegistry : public RefCounted {
	GDCLASS(AILLMRuntimeRegistry, RefCounted);

	HashMap<String, Ref<AILLMRuntime>> runtimes;
	mutable Mutex mutex;

	static Dictionary _dictionary_from_variant(const Variant &p_value);
	static Ref<AILLMRuntime> _make_runtime_for_type(const String &p_type);

protected:
	static void _bind_methods();

public:
	AILLMRuntimeRegistry();

	void register_runtime_struct(const String &p_provider, const Ref<AILLMRuntime> &p_runtime);
	bool get_runtime_struct(const String &p_provider, Ref<AILLMRuntime> &r_runtime) const;
	bool configure_from_config_struct(const Dictionary &p_config, AIError &r_error);

	void register_runtime(const String &p_provider, const Ref<AILLMRuntime> &p_runtime);
	bool has_runtime(const String &p_provider) const;
	Ref<AILLMRuntime> get_runtime(const String &p_provider) const;
	Dictionary configure_from_config(const Dictionary &p_config);
	Dictionary stream(const Dictionary &p_request, const Callable &p_event_callback = Callable());
	Array list_runtimes() const;
	void clear();
};
