/**************************************************************************/
/*  ai_llm_runtime_registry.cpp                                           */
/**************************************************************************/

#include "ai_llm_runtime_registry.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AILLMRuntimeRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_runtime", "provider", "runtime"), &AILLMRuntimeRegistry::register_runtime);
	ClassDB::bind_method(D_METHOD("has_runtime", "provider"), &AILLMRuntimeRegistry::has_runtime);
	ClassDB::bind_method(D_METHOD("get_runtime", "provider"), &AILLMRuntimeRegistry::get_runtime);
	ClassDB::bind_method(D_METHOD("configure_from_config", "config"), &AILLMRuntimeRegistry::configure_from_config);
	ClassDB::bind_method(D_METHOD("stream", "request", "event_callback"), &AILLMRuntimeRegistry::stream, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("list_runtimes"), &AILLMRuntimeRegistry::list_runtimes);
	ClassDB::bind_method(D_METHOD("clear"), &AILLMRuntimeRegistry::clear);
}

Dictionary AILLMRuntimeRegistry::_dictionary_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		return Dictionary(p_value).duplicate(true);
	}
	return Dictionary();
}

Ref<AILLMRuntime> AILLMRuntimeRegistry::_make_runtime_for_type(const String &p_type) {
	const String type = p_type.strip_edges().to_lower();
	if (type == "openai" || type == "openai-compatible") {
		Ref<AIOpenAICompatibleRuntime> runtime;
		runtime.instantiate();
		return runtime;
	}

	Ref<AIFakeLLMRuntime> runtime;
	runtime.instantiate();
	return runtime;
}

AILLMRuntimeRegistry::AILLMRuntimeRegistry() {
	Ref<AIFakeLLMRuntime> fake;
	fake.instantiate();
	register_runtime_struct("fake", fake);
}

void AILLMRuntimeRegistry::register_runtime_struct(const String &p_provider, const Ref<AILLMRuntime> &p_runtime) {
	const String provider = p_provider.strip_edges();
	if (provider.is_empty() || p_runtime.is_null()) {
		return;
	}
	MutexLock lock(mutex);
	runtimes[provider] = p_runtime;
}

bool AILLMRuntimeRegistry::get_runtime_struct(const String &p_provider, Ref<AILLMRuntime> &r_runtime) const {
	const String provider = p_provider.strip_edges();
	MutexLock lock(mutex);
	HashMap<String, Ref<AILLMRuntime>>::ConstIterator runtime = runtimes.find(provider);
	if (!runtime) {
		return false;
	}
	r_runtime = runtime->value;
	return r_runtime.is_valid();
}

bool AILLMRuntimeRegistry::configure_from_config_struct(const Dictionary &p_config, AIError &r_error) {
	const Dictionary providers = _dictionary_from_variant(p_config.get("providers", Dictionary()));
	for (const KeyValue<Variant, Variant> &kv : providers) {
		if (kv.value.get_type() != Variant::DICTIONARY) {
			continue;
		}
		const String provider = String(kv.key).strip_edges();
		if (provider.is_empty()) {
			continue;
		}
		const Dictionary provider_config = kv.value;
		const String type = String(provider_config.get("type", provider));
		Ref<AILLMRuntime> runtime;
		if (!get_runtime_struct(provider, runtime)) {
			runtime = _make_runtime_for_type(type);
			register_runtime_struct(provider, runtime);
		}
		if (!runtime->configure(provider_config, r_error)) {
			return false;
		}
	}
	r_error = AIError::none();
	return true;
}

void AILLMRuntimeRegistry::register_runtime(const String &p_provider, const Ref<AILLMRuntime> &p_runtime) {
	register_runtime_struct(p_provider, p_runtime);
}

bool AILLMRuntimeRegistry::has_runtime(const String &p_provider) const {
	Ref<AILLMRuntime> runtime;
	return get_runtime_struct(p_provider, runtime);
}

Ref<AILLMRuntime> AILLMRuntimeRegistry::get_runtime(const String &p_provider) const {
	Ref<AILLMRuntime> runtime;
	get_runtime_struct(p_provider, runtime);
	return runtime;
}

Dictionary AILLMRuntimeRegistry::configure_from_config(const Dictionary &p_config) {
	AIError error;
	if (!configure_from_config_struct(p_config, error)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = error.to_dictionary();
		return result;
	}
	Dictionary result;
	result["success"] = true;
	return result;
}

Dictionary AILLMRuntimeRegistry::stream(const Dictionary &p_request, const Callable &p_event_callback) {
	const AIModelRequest request = AILLMRuntime::request_from_dictionary(p_request);
	Ref<AILLMRuntime> runtime;
	if (!get_runtime_struct(request.provider, runtime)) {
		Dictionary result;
		result["success"] = false;
		result["error"] = AIError::make(AI_ERROR_UNAVAILABLE, "No LLM runtime registered for provider: " + request.provider).to_dictionary();
		return result;
	}
	return runtime->stream(p_request, p_event_callback);
}

Array AILLMRuntimeRegistry::list_runtimes() const {
	Array result;
	MutexLock lock(mutex);
	for (const KeyValue<String, Ref<AILLMRuntime>> &kv : runtimes) {
		Dictionary item;
		item["provider"] = kv.key;
		item["type"] = kv.value.is_valid() ? kv.value->get_runtime_type() : String();
		result.push_back(item);
	}
	return result;
}

void AILLMRuntimeRegistry::clear() {
	MutexLock lock(mutex);
	runtimes.clear();
}
