/**************************************************************************/
/*  ai_system_context.cpp                                                 */
/**************************************************************************/

#include "ai_system_context.h"

#include "core/variant/variant.h"

static String _ai_system_context_source_hash(const AISystemContextSource &p_source) {
	String signature = p_source.domain + "\n" + p_source.text + "\n";
	signature += Variant(p_source.metadata).stringify();
	signature += p_source.required ? "\nrequired" : "\noptional";
	signature += p_source.available ? "\navailable" : "\nunavailable";
	return signature.md5_text();
}

static void _ai_system_context_sort_sources(Vector<AISystemContextSource> &r_sources) {
	for (int i = 0; i < r_sources.size(); i++) {
		for (int j = i + 1; j < r_sources.size(); j++) {
			if (r_sources[j].priority < r_sources[i].priority) {
				const AISystemContextSource tmp = r_sources[i];
				r_sources.write[i] = r_sources[j];
				r_sources.write[j] = tmp;
			}
		}
	}
}

bool AISystemContextSource::is_blocking() const {
	return required && !available;
}

Dictionary AISystemContextSource::to_dictionary() const {
	Dictionary result;
	result["domain"] = domain;
	result["text"] = text;
	result["content_hash"] = content_hash;
	result["required"] = required;
	result["available"] = available;
	result["priority"] = priority;
	result["metadata"] = metadata.duplicate(true);
	return result;
}

AISystemContextSource AISystemContextSource::from_dictionary(const Dictionary &p_dict) {
	AISystemContextSource result;
	result.domain = String(p_dict.get("domain", String())).strip_edges();
	result.text = p_dict.get("text", String());
	result.content_hash = String(p_dict.get("content_hash", p_dict.get("contentHash", String()))).strip_edges();
	result.required = bool(p_dict.get("required", true));
	result.available = bool(p_dict.get("available", true));
	result.priority = int(p_dict.get("priority", 0));
	if (p_dict.get("metadata", Variant()).get_type() == Variant::DICTIONARY) {
		result.metadata = Dictionary(p_dict["metadata"]).duplicate(true);
	}
	if (result.content_hash.is_empty()) {
		result.content_hash = _ai_system_context_source_hash(result);
	}
	return result;
}

bool AISystemContext::is_available() const {
	return available;
}

Dictionary AISystemContext::to_dictionary() const {
	Dictionary result;
	result["baseline"] = baseline;
	result["snapshot"] = snapshot.duplicate(true);
	result["available"] = available;
	result["blocked_reason"] = blocked_reason;

	Array source_array;
	for (int i = 0; i < sources.size(); i++) {
		source_array.push_back(sources[i].to_dictionary());
	}
	result["sources"] = source_array;
	return result;
}

AISystemContext AISystemContext::from_dictionary(const Dictionary &p_dict) {
	AISystemContext result;
	result.baseline = p_dict.get("baseline", String());
	if (p_dict.get("snapshot", Variant()).get_type() == Variant::DICTIONARY) {
		result.snapshot = Dictionary(p_dict["snapshot"]).duplicate(true);
	}
	result.available = bool(p_dict.get("available", true));
	result.blocked_reason = p_dict.get("blocked_reason", p_dict.get("blockedReason", String()));
	if (p_dict.get("sources", Variant()).get_type() == Variant::ARRAY) {
		const Array source_array = p_dict["sources"];
		for (int i = 0; i < source_array.size(); i++) {
			if (source_array[i].get_type() == Variant::DICTIONARY) {
				result.sources.push_back(AISystemContextSource::from_dictionary(source_array[i]));
			}
		}
	}
	return result;
}

AISystemContext AISystemContext::combine(const Vector<AISystemContextSource> &p_sources) {
	AISystemContext result;
	result.sources = p_sources;
	_ai_system_context_sort_sources(result.sources);

	Array snapshot_sources;
	String baseline;
	String snapshot_signature;
	bool available = true;
	String blocked_reason;

	for (int i = 0; i < result.sources.size(); i++) {
		AISystemContextSource source = result.sources[i];
		if (source.content_hash.is_empty()) {
			source.content_hash = _ai_system_context_source_hash(source);
			result.sources.write[i] = source;
		}

		if (source.is_blocking()) {
			available = false;
			if (blocked_reason.is_empty()) {
				blocked_reason = "Required context source is unavailable: " + source.domain;
			}
		}

		const String text = source.text.strip_edges();
		if (source.available && !text.is_empty()) {
			baseline += baseline.is_empty() ? text : "\n\n" + text;
		}

		Dictionary source_snapshot;
		source_snapshot["domain"] = source.domain;
		source_snapshot["content_hash"] = source.content_hash;
		source_snapshot["required"] = source.required;
		source_snapshot["available"] = source.available;
		source_snapshot["priority"] = source.priority;
		source_snapshot["metadata"] = source.metadata.duplicate(true);
		snapshot_sources.push_back(source_snapshot);

		snapshot_signature += source.domain + ":" + source.content_hash + ";";
	}

	Dictionary snapshot;
	snapshot["sources"] = snapshot_sources;
	snapshot["source_count"] = result.sources.size();
	snapshot["baseline_hash"] = baseline.md5_text();
	snapshot["snapshot_hash"] = snapshot_signature.md5_text();
	snapshot["available"] = available;

	result.baseline = baseline;
	result.snapshot = snapshot;
	result.available = available;
	result.blocked_reason = blocked_reason;
	return result;
}
