/**************************************************************************/
/*  ai_documentation_service.cpp                                          */
/**************************************************************************/

#include "ai_documentation_service.h"

#include "core/doc_data.h"
#include "core/object/class_db.h"
#include "core/object/method_info.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"
#include "editor/doc/doc_tools.h"
#include "editor/doc/editor_help.h"

namespace {

enum class AIDocSearchSource {
	DOC_DATA,
	CLASS_DB,
};

enum class AIDocSearchKind {
	CLASS,
	PROPERTY,
	METHOD,
	SIGNAL,
	CONSTANT,
	THEME_ITEM,
	ENUM,
	CONSTRUCTOR,
	OPERATOR,
	ANNOTATION,
};

struct AIDocSearchCandidate {
	float score = 0.0f;
	String sort_key;
	AIDocSearchSource source = AIDocSearchSource::DOC_DATA;
	AIDocSearchKind kind = AIDocSearchKind::CLASS;
	String class_name;
	String name;
	const DocData::ClassDoc *class_doc = nullptr;
	const DocData::PropertyDoc *property_doc = nullptr;
	const DocData::MethodDoc *method_doc = nullptr;
	const DocData::ConstantDoc *constant_doc = nullptr;
	const DocData::ThemeItemDoc *theme_item_doc = nullptr;
	const DocData::EnumDoc *enum_doc = nullptr;
	PropertyInfo classdb_property;
	MethodInfo classdb_method;
};

struct AIDocSearchCandidateCompare {
	_FORCE_INLINE_ bool operator()(const AIDocSearchCandidate &p_l, const AIDocSearchCandidate &p_r) const {
		if (p_l.score == p_r.score) {
			return p_l.sort_key.naturalcasecmp_to(p_r.sort_key) < 0;
		}
		return p_l.score > p_r.score;
	}
};

struct AIDocSearchTopResults {
	Vector<AIDocSearchCandidate> candidates;
	int total_matches = 0;
	int max_results = 20;
};

struct AIDocPropertySuggestion {
	float score = 0.0f;
	String sort_key;
	Dictionary data;
};

struct AIDocPropertySuggestionCompare {
	_FORCE_INLINE_ bool operator()(const AIDocPropertySuggestion &p_l, const AIDocPropertySuggestion &p_r) const {
		if (p_l.score == p_r.score) {
			return p_l.sort_key.naturalcasecmp_to(p_r.sort_key) < 0;
		}
		return p_l.score > p_r.score;
	}
};

String _normalize_identifier(const String &p_text) {
	const String text = p_text.to_lower();
	String normalized;
	for (int i = 0; i < text.length(); i++) {
		const char32_t c = text[i];
		if (c == '_' || c == '-' || c == ':' || c == '/' || c == '.' || c == ' ' || c == '\t') {
			continue;
		}
		normalized += c;
	}
	return normalized;
}

String _strip_bbcode_tags(const String &p_text) {
	String stripped;
	bool in_tag = false;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if (c == '[') {
			in_tag = true;
			continue;
		}
		if (in_tag) {
			if (c == ']') {
				in_tag = false;
			}
			continue;
		}
		stripped += c;
	}
	return stripped;
}

String _clean_doc_text(const String &p_text, int p_max_chars = 420) {
	String text = _strip_bbcode_tags(p_text).replace("\r\n", "\n").replace("\r", "\n").replace("\n", " ").replace("\t", " ").strip_edges();
	while (text.contains("  ")) {
		text = text.replace("  ", " ");
	}
	if (p_max_chars > 0 && text.length() > p_max_chars) {
		text = text.substr(0, p_max_chars).strip_edges() + "...";
	}
	return text;
}

float _score_match(const String &p_query, const String &p_name, const String &p_qualified_name, const String &p_description = String()) {
	const String query = p_query.strip_edges().to_lower();
	if (query.is_empty()) {
		return 1.0f;
	}

	const String name = p_name.to_lower();
	const String qualified_name = p_qualified_name.to_lower();
	const String normalized_query = _normalize_identifier(query);
	const String normalized_name = _normalize_identifier(name);
	const String normalized_qualified_name = _normalize_identifier(qualified_name);

	float score = MAX(name.similarity(query), qualified_name.similarity(query));
	score = MAX(score, normalized_name.similarity(normalized_query));
	score = MAX(score, normalized_qualified_name.similarity(normalized_query));

	if (name == query || qualified_name == query || normalized_name == normalized_query || normalized_qualified_name == normalized_query) {
		score = MAX(score, 2.0f);
	}
	if (!query.is_empty() && (name.contains(query) || qualified_name.contains(query))) {
		score = MAX(score, 1.65f);
	}
	if (!normalized_query.is_empty() && (normalized_name.contains(normalized_query) || normalized_qualified_name.contains(normalized_query))) {
		score = MAX(score, 1.85f);
	}
	if (!normalized_query.is_empty() && (normalized_query.contains(normalized_name) || normalized_query.contains(normalized_qualified_name))) {
		score = MAX(score, 1.35f);
	}
	if (!p_description.is_empty() && p_description.to_lower().contains(query)) {
		score = MAX(score, 0.85f);
	}
	return score;
}

String _make_score_description(const String &p_query, const String &p_description, const String &p_keywords = String(), const String &p_extra_description = String()) {
	if (p_query.strip_edges().is_empty()) {
		return String();
	}

	String description;
	if (!p_description.is_empty()) {
		description = p_description;
	}
	if (!p_extra_description.is_empty()) {
		description = description.is_empty() ? p_extra_description : description + " " + p_extra_description;
	}
	if (!p_keywords.is_empty()) {
		description = description.is_empty() ? p_keywords : description + " " + p_keywords;
	}
	return description;
}

bool _kind_matches(const String &p_filter, const String &p_kind) {
	if (p_filter.is_empty() || p_filter == "all") {
		return true;
	}
	return p_filter == p_kind;
}

String _normalize_kind_filter(const String &p_kind, String &r_error) {
	String kind = p_kind.strip_edges().to_lower();
	if (kind.is_empty()) {
		return "all";
	}
	kind = kind.replace("-", "_");
	if (kind == "classes") {
		kind = "class";
	} else if (kind == "properties") {
		kind = "property";
	} else if (kind == "methods") {
		kind = "method";
	} else if (kind == "signals") {
		kind = "signal";
	} else if (kind == "constants") {
		kind = "constant";
	} else if (kind == "theme_property" || kind == "theme_properties" || kind == "theme_items") {
		kind = "theme_item";
	} else if (kind == "enums") {
		kind = "enum";
	} else if (kind == "constructors") {
		kind = "constructor";
	} else if (kind == "operators") {
		kind = "operator";
	} else if (kind == "annotations") {
		kind = "annotation";
	}

	if (kind == "all" || kind == "class" || kind == "property" || kind == "method" || kind == "signal" || kind == "constant" || kind == "theme_item" || kind == "enum" || kind == "constructor" || kind == "operator" || kind == "annotation") {
		return kind;
	}

	r_error = "kind must be one of: all, class, property, method, signal, constant, theme_item, enum, constructor, operator, annotation.";
	return String();
}

DocTools *_get_ready_doc_data_main_thread(String &r_error) {
	if (!Thread::is_main_thread()) {
		r_error = "Documentation data must be accessed on the main thread.";
		return nullptr;
	}

	return EditorHelp::get_doc_data();
}

DocTools *_get_doc_data_if_ready_main_thread() {
	if (!Thread::is_main_thread()) {
		return nullptr;
	}
	return EditorHelp::get_doc_data();
}

const DocData::ClassDoc *_find_class_doc(DocTools *p_doc, const String &p_class_name) {
	ERR_FAIL_NULL_V(p_doc, nullptr);

	const String class_name = p_class_name.strip_edges();
	if (class_name.is_empty()) {
		return nullptr;
	}

	const DocData::ClassDoc *class_doc = p_doc->class_list.getptr(class_name);
	if (class_doc) {
		return class_doc;
	}

	for (const KeyValue<String, DocData::ClassDoc> &E : p_doc->class_list) {
		if (E.key.to_lower() == class_name.to_lower()) {
			return &E.value;
		}
	}
	return nullptr;
}

const DocData::PropertyDoc *_find_property_doc(DocTools *p_doc, const String &p_class_name, const String &p_property_name) {
	String class_name = p_class_name;
	while (!class_name.is_empty()) {
		const DocData::ClassDoc *class_doc = _find_class_doc(p_doc, class_name);
		if (!class_doc) {
			break;
		}
		for (int i = 0; i < class_doc->properties.size(); i++) {
			if (class_doc->properties[i].name == p_property_name) {
				return &class_doc->properties[i];
			}
		}

		if (!class_doc->inherits.is_empty()) {
			class_name = class_doc->inherits;
		} else if (ClassDB::class_exists(class_name)) {
			class_name = ClassDB::get_parent_class(class_name);
		} else {
			break;
		}
	}
	return nullptr;
}

Array _make_argument_array(const Vector<DocData::ArgumentDoc> &p_arguments) {
	Array arguments;
	for (int i = 0; i < p_arguments.size(); i++) {
		arguments.push_back(DocData::ArgumentDoc::to_dict(p_arguments[i]));
	}
	return arguments;
}

Dictionary _make_method_dict(const DocData::MethodDoc &p_doc, const String &p_class_name, const String &p_kind, bool p_include_descriptions) {
	Dictionary dict;
	dict["kind"] = p_kind;
	dict["class_name"] = p_class_name;
	dict["name"] = p_doc.name;
	dict["qualified_name"] = p_class_name + "." + p_doc.name;
	if (!p_doc.return_type.is_empty()) {
		dict["return_type"] = p_doc.return_type;
	}
	if (!p_doc.return_enum.is_empty()) {
		dict["return_enum"] = p_doc.return_enum;
		dict["return_is_bitfield"] = p_doc.return_is_bitfield;
	}
	if (!p_doc.qualifiers.is_empty()) {
		dict["qualifiers"] = p_doc.qualifiers;
	}
	if (!p_doc.arguments.is_empty()) {
		dict["arguments"] = _make_argument_array(p_doc.arguments);
	}
	if (p_doc.rest_argument.name != String()) {
		dict["rest_argument"] = DocData::ArgumentDoc::to_dict(p_doc.rest_argument);
	}
	if (p_doc.is_deprecated) {
		dict["deprecated"] = p_doc.deprecated_message;
	}
	if (p_doc.is_experimental) {
		dict["experimental"] = p_doc.experimental_message;
	}
	if (p_include_descriptions && !p_doc.description.is_empty()) {
		dict["description"] = _clean_doc_text(p_doc.description);
	}
	return dict;
}

Dictionary _make_property_dict(const DocData::PropertyDoc &p_doc, const String &p_class_name, bool p_include_descriptions) {
	Dictionary dict;
	dict["kind"] = "property";
	dict["class_name"] = p_class_name;
	dict["name"] = p_doc.name;
	dict["qualified_name"] = p_class_name + "." + p_doc.name;
	if (!p_doc.type.is_empty()) {
		dict["type"] = p_doc.type;
	}
	if (!p_doc.enumeration.is_empty()) {
		dict["enumeration"] = p_doc.enumeration;
		dict["is_bitfield"] = p_doc.is_bitfield;
	}
	if (!p_doc.default_value.is_empty()) {
		dict["default_value"] = p_doc.default_value;
	}
	if (!p_doc.setter.is_empty()) {
		dict["setter"] = p_doc.setter;
	}
	if (!p_doc.getter.is_empty()) {
		dict["getter"] = p_doc.getter;
	}
	if (p_doc.overridden) {
		dict["overridden"] = true;
	}
	if (!p_doc.overrides.is_empty()) {
		dict["overrides"] = p_doc.overrides;
	}
	if (p_doc.is_deprecated) {
		dict["deprecated"] = p_doc.deprecated_message;
	}
	if (p_doc.is_experimental) {
		dict["experimental"] = p_doc.experimental_message;
	}
	if (p_include_descriptions && !p_doc.description.is_empty()) {
		dict["description"] = _clean_doc_text(p_doc.description);
	}
	return dict;
}

Dictionary _make_constant_dict(const DocData::ConstantDoc &p_doc, const String &p_class_name, bool p_include_descriptions) {
	Dictionary dict;
	dict["kind"] = "constant";
	dict["class_name"] = p_class_name;
	dict["name"] = p_doc.name;
	dict["qualified_name"] = p_class_name + "." + p_doc.name;
	if (!p_doc.value.is_empty()) {
		dict["value"] = p_doc.value;
	}
	dict["is_value_valid"] = p_doc.is_value_valid;
	if (!p_doc.type.is_empty()) {
		dict["type"] = p_doc.type;
	}
	if (!p_doc.enumeration.is_empty()) {
		dict["enumeration"] = p_doc.enumeration;
		dict["is_bitfield"] = p_doc.is_bitfield;
	}
	if (p_doc.is_deprecated) {
		dict["deprecated"] = p_doc.deprecated_message;
	}
	if (p_doc.is_experimental) {
		dict["experimental"] = p_doc.experimental_message;
	}
	if (p_include_descriptions && !p_doc.description.is_empty()) {
		dict["description"] = _clean_doc_text(p_doc.description);
	}
	return dict;
}

Dictionary _make_theme_item_dict(const DocData::ThemeItemDoc &p_doc, const String &p_class_name, bool p_include_descriptions) {
	Dictionary dict;
	dict["kind"] = "theme_item";
	dict["class_name"] = p_class_name;
	dict["name"] = p_doc.name;
	dict["qualified_name"] = p_class_name + "." + p_doc.name;
	if (!p_doc.type.is_empty()) {
		dict["type"] = p_doc.type;
	}
	if (!p_doc.data_type.is_empty()) {
		dict["data_type"] = p_doc.data_type;
	}
	if (!p_doc.default_value.is_empty()) {
		dict["default_value"] = p_doc.default_value;
	}
	if (p_doc.is_deprecated) {
		dict["deprecated"] = p_doc.deprecated_message;
	}
	if (p_doc.is_experimental) {
		dict["experimental"] = p_doc.experimental_message;
	}
	if (p_include_descriptions && !p_doc.description.is_empty()) {
		dict["description"] = _clean_doc_text(p_doc.description);
	}
	return dict;
}

Dictionary _make_enum_dict(const String &p_enum_name, const DocData::EnumDoc &p_doc, const String &p_class_name, bool p_include_descriptions) {
	Dictionary dict;
	dict["kind"] = "enum";
	dict["class_name"] = p_class_name;
	dict["name"] = p_enum_name;
	dict["qualified_name"] = p_class_name + "." + p_enum_name;
	if (p_doc.is_deprecated) {
		dict["deprecated"] = p_doc.deprecated_message;
	}
	if (p_doc.is_experimental) {
		dict["experimental"] = p_doc.experimental_message;
	}
	if (p_include_descriptions && !p_doc.description.is_empty()) {
		dict["description"] = _clean_doc_text(p_doc.description);
	}
	return dict;
}

Dictionary _make_class_dict(const DocData::ClassDoc &p_doc, bool p_include_descriptions) {
	Dictionary dict;
	dict["kind"] = "class";
	dict["class_name"] = p_doc.name;
	dict["name"] = p_doc.name;
	dict["qualified_name"] = p_doc.name;
	if (!p_doc.inherits.is_empty()) {
		dict["inherits"] = p_doc.inherits;
	}
	if (!p_doc.api_type.is_empty()) {
		dict["api_type"] = p_doc.api_type;
	}
	dict["is_script_doc"] = p_doc.is_script_doc;
	if (!p_doc.script_path.is_empty()) {
		dict["script_path"] = p_doc.script_path;
	}
	dict["property_count"] = p_doc.properties.size();
	dict["method_count"] = p_doc.methods.size();
	dict["signal_count"] = p_doc.signals.size();
	dict["constant_count"] = p_doc.constants.size();
	if (p_doc.is_deprecated) {
		dict["deprecated"] = p_doc.deprecated_message;
	}
	if (p_doc.is_experimental) {
		dict["experimental"] = p_doc.experimental_message;
	}
	if (p_include_descriptions) {
		if (!p_doc.brief_description.is_empty()) {
			dict["brief_description"] = _clean_doc_text(p_doc.brief_description);
		}
		if (!p_doc.description.is_empty()) {
			dict["description"] = _clean_doc_text(p_doc.description);
		}
	}
	return dict;
}

String _search_kind_to_string(AIDocSearchKind p_kind) {
	switch (p_kind) {
		case AIDocSearchKind::CLASS:
			return "class";
		case AIDocSearchKind::PROPERTY:
			return "property";
		case AIDocSearchKind::METHOD:
			return "method";
		case AIDocSearchKind::SIGNAL:
			return "signal";
		case AIDocSearchKind::CONSTANT:
			return "constant";
		case AIDocSearchKind::THEME_ITEM:
			return "theme_item";
		case AIDocSearchKind::ENUM:
			return "enum";
		case AIDocSearchKind::CONSTRUCTOR:
			return "constructor";
		case AIDocSearchKind::OPERATOR:
			return "operator";
		case AIDocSearchKind::ANNOTATION:
			return "annotation";
	}

	return String();
}

String _make_candidate_sort_key(AIDocSearchKind p_kind, const String &p_class_name, const String &p_name) {
	if (p_kind == AIDocSearchKind::CLASS) {
		return p_class_name;
	}
	return p_class_name + "." + p_name;
}

bool _is_candidate_before(const AIDocSearchCandidate &p_l, const AIDocSearchCandidate &p_r) {
	return AIDocSearchCandidateCompare()(p_l, p_r);
}

int _find_worst_candidate_index(const Vector<AIDocSearchCandidate> &p_candidates) {
	ERR_FAIL_COND_V(p_candidates.is_empty(), -1);

	int worst_index = 0;
	for (int i = 1; i < p_candidates.size(); i++) {
		if (_is_candidate_before(p_candidates[worst_index], p_candidates[i])) {
			worst_index = i;
		}
	}
	return worst_index;
}

bool _try_accept_candidate(AIDocSearchTopResults &r_results, float p_score, const String &p_sort_key, int &r_candidate_index) {
	if (p_score <= 0.05f) {
		return false;
	}

	r_results.total_matches++;

	AIDocSearchCandidate probe;
	probe.score = p_score;
	probe.sort_key = p_sort_key;

	if (r_results.candidates.size() < r_results.max_results) {
		r_candidate_index = r_results.candidates.size();
		return true;
	}

	const int worst_index = _find_worst_candidate_index(r_results.candidates);
	if (worst_index < 0 || !_is_candidate_before(probe, r_results.candidates[worst_index])) {
		return false;
	}

	r_candidate_index = worst_index;
	return true;
}

void _store_candidate(AIDocSearchTopResults &r_results, int p_candidate_index, AIDocSearchCandidate p_candidate) {
	if (p_candidate_index == r_results.candidates.size()) {
		r_results.candidates.push_back(p_candidate);
		return;
	}

	ERR_FAIL_INDEX(p_candidate_index, r_results.candidates.size());
	r_results.candidates.write[p_candidate_index] = p_candidate;
}

void _push_doc_candidate(AIDocSearchTopResults &r_results, AIDocSearchKind p_kind, const String &p_class_name, const String &p_name, float p_score, const DocData::ClassDoc *p_class_doc = nullptr, const DocData::PropertyDoc *p_property_doc = nullptr, const DocData::MethodDoc *p_method_doc = nullptr, const DocData::ConstantDoc *p_constant_doc = nullptr, const DocData::ThemeItemDoc *p_theme_item_doc = nullptr, const DocData::EnumDoc *p_enum_doc = nullptr) {
	const String sort_key = _make_candidate_sort_key(p_kind, p_class_name, p_name);
	int candidate_index = -1;
	if (!_try_accept_candidate(r_results, p_score, sort_key, candidate_index)) {
		return;
	}

	AIDocSearchCandidate candidate;
	candidate.score = p_score;
	candidate.sort_key = sort_key;
	candidate.source = AIDocSearchSource::DOC_DATA;
	candidate.kind = p_kind;
	candidate.class_name = p_class_name;
	candidate.name = p_name;
	candidate.class_doc = p_class_doc;
	candidate.property_doc = p_property_doc;
	candidate.method_doc = p_method_doc;
	candidate.constant_doc = p_constant_doc;
	candidate.theme_item_doc = p_theme_item_doc;
	candidate.enum_doc = p_enum_doc;
	_store_candidate(r_results, candidate_index, candidate);
}

void _search_class_doc(const DocData::ClassDoc &p_class_doc, const String &p_query, const String &p_kind_filter, AIDocSearchTopResults &r_results) {
	if (_kind_matches(p_kind_filter, "class")) {
		const String class_text = _make_score_description(p_query, p_class_doc.brief_description, p_class_doc.keywords, p_class_doc.description);
		const float score = _score_match(p_query, p_class_doc.name, p_class_doc.name, class_text);
		_push_doc_candidate(r_results, AIDocSearchKind::CLASS, p_class_doc.name, p_class_doc.name, score, &p_class_doc);
	}

	if (_kind_matches(p_kind_filter, "property")) {
		for (int i = 0; i < p_class_doc.properties.size(); i++) {
			const DocData::PropertyDoc &property_doc = p_class_doc.properties[i];
			const String qualified_name = p_class_doc.name + "." + property_doc.name;
			const float score = _score_match(p_query, property_doc.name, qualified_name, _make_score_description(p_query, property_doc.description, property_doc.keywords));
			_push_doc_candidate(r_results, AIDocSearchKind::PROPERTY, p_class_doc.name, property_doc.name, score, &p_class_doc, &property_doc);
		}
	}

	if (_kind_matches(p_kind_filter, "method")) {
		for (int i = 0; i < p_class_doc.methods.size(); i++) {
			const DocData::MethodDoc &method_doc = p_class_doc.methods[i];
			const String qualified_name = p_class_doc.name + "." + method_doc.name;
			const float score = _score_match(p_query, method_doc.name, qualified_name, _make_score_description(p_query, method_doc.description, method_doc.keywords));
			_push_doc_candidate(r_results, AIDocSearchKind::METHOD, p_class_doc.name, method_doc.name, score, &p_class_doc, nullptr, &method_doc);
		}
	}

	if (_kind_matches(p_kind_filter, "signal")) {
		for (int i = 0; i < p_class_doc.signals.size(); i++) {
			const DocData::MethodDoc &signal_doc = p_class_doc.signals[i];
			const String qualified_name = p_class_doc.name + "." + signal_doc.name;
			const float score = _score_match(p_query, signal_doc.name, qualified_name, _make_score_description(p_query, signal_doc.description, signal_doc.keywords));
			_push_doc_candidate(r_results, AIDocSearchKind::SIGNAL, p_class_doc.name, signal_doc.name, score, &p_class_doc, nullptr, &signal_doc);
		}
	}

	if (_kind_matches(p_kind_filter, "constant")) {
		for (int i = 0; i < p_class_doc.constants.size(); i++) {
			const DocData::ConstantDoc &constant_doc = p_class_doc.constants[i];
			const String qualified_name = p_class_doc.name + "." + constant_doc.name;
			const float score = _score_match(p_query, constant_doc.name, qualified_name, _make_score_description(p_query, constant_doc.description, constant_doc.keywords));
			_push_doc_candidate(r_results, AIDocSearchKind::CONSTANT, p_class_doc.name, constant_doc.name, score, &p_class_doc, nullptr, nullptr, &constant_doc);
		}
	}

	if (_kind_matches(p_kind_filter, "theme_item")) {
		for (int i = 0; i < p_class_doc.theme_properties.size(); i++) {
			const DocData::ThemeItemDoc &theme_item_doc = p_class_doc.theme_properties[i];
			const String qualified_name = p_class_doc.name + "." + theme_item_doc.name;
			const float score = _score_match(p_query, theme_item_doc.name, qualified_name, _make_score_description(p_query, theme_item_doc.description, theme_item_doc.keywords));
			_push_doc_candidate(r_results, AIDocSearchKind::THEME_ITEM, p_class_doc.name, theme_item_doc.name, score, &p_class_doc, nullptr, nullptr, nullptr, &theme_item_doc);
		}
	}

	if (_kind_matches(p_kind_filter, "enum")) {
		for (const KeyValue<String, DocData::EnumDoc> &E : p_class_doc.enums) {
			const String qualified_name = p_class_doc.name + "." + E.key;
			const float score = _score_match(p_query, E.key, qualified_name, E.value.description);
			_push_doc_candidate(r_results, AIDocSearchKind::ENUM, p_class_doc.name, E.key, score, &p_class_doc, nullptr, nullptr, nullptr, nullptr, &E.value);
		}
	}

	if (_kind_matches(p_kind_filter, "constructor")) {
		for (int i = 0; i < p_class_doc.constructors.size(); i++) {
			const DocData::MethodDoc &constructor_doc = p_class_doc.constructors[i];
			const String qualified_name = p_class_doc.name + "." + constructor_doc.name;
			const float score = _score_match(p_query, constructor_doc.name, qualified_name, _make_score_description(p_query, constructor_doc.description, constructor_doc.keywords));
			_push_doc_candidate(r_results, AIDocSearchKind::CONSTRUCTOR, p_class_doc.name, constructor_doc.name, score, &p_class_doc, nullptr, &constructor_doc);
		}
	}

	if (_kind_matches(p_kind_filter, "operator")) {
		for (int i = 0; i < p_class_doc.operators.size(); i++) {
			const DocData::MethodDoc &operator_doc = p_class_doc.operators[i];
			const String qualified_name = p_class_doc.name + "." + operator_doc.name;
			const float score = _score_match(p_query, operator_doc.name, qualified_name, _make_score_description(p_query, operator_doc.description, operator_doc.keywords));
			_push_doc_candidate(r_results, AIDocSearchKind::OPERATOR, p_class_doc.name, operator_doc.name, score, &p_class_doc, nullptr, &operator_doc);
		}
	}

	if (_kind_matches(p_kind_filter, "annotation")) {
		for (int i = 0; i < p_class_doc.annotations.size(); i++) {
			const DocData::MethodDoc &annotation_doc = p_class_doc.annotations[i];
			const String qualified_name = p_class_doc.name + "." + annotation_doc.name;
			const float score = _score_match(p_query, annotation_doc.name, qualified_name, _make_score_description(p_query, annotation_doc.description, annotation_doc.keywords));
			_push_doc_candidate(r_results, AIDocSearchKind::ANNOTATION, p_class_doc.name, annotation_doc.name, score, &p_class_doc, nullptr, &annotation_doc);
		}
	}
}

Dictionary _make_classdb_property_dict(const PropertyInfo &p_info, const String &p_class_name) {
	Dictionary dict;
	dict["kind"] = "property";
	dict["class_name"] = p_class_name;
	dict["name"] = String(p_info.name);
	dict["qualified_name"] = p_class_name + "." + String(p_info.name);
	dict["type"] = Variant::get_type_name(p_info.type);
	dict["hint"] = int(p_info.hint);
	dict["hint_text"] = p_info.hint_string;
	dict["declared_class_name"] = String(p_info.class_name);
	dict["read_only"] = bool(p_info.usage & PROPERTY_USAGE_READ_ONLY);
	return dict;
}

Dictionary _make_classdb_method_dict(const MethodInfo &p_info, const String &p_class_name, const String &p_kind) {
	Dictionary dict;
	dict["kind"] = p_kind;
	dict["class_name"] = p_class_name;
	dict["name"] = p_info.name;
	dict["qualified_name"] = p_class_name + "." + p_info.name;
	if (p_info.return_val.type != Variant::NIL) {
		dict["return_type"] = Variant::get_type_name(p_info.return_val.type);
		if (!String(p_info.return_val.class_name).is_empty()) {
			dict["return_class_name"] = String(p_info.return_val.class_name);
		}
	}
	dict["flags"] = int(p_info.flags);

	Array arguments;
	for (int i = 0; i < p_info.arguments.size(); i++) {
		const PropertyInfo &argument_info = p_info.arguments[i];
		Dictionary argument;
		argument["name"] = String(argument_info.name);
		argument["type"] = Variant::get_type_name(argument_info.type);
		if (!String(argument_info.class_name).is_empty()) {
			argument["class_name"] = String(argument_info.class_name);
		}
		if (p_info.default_arguments.size() > 0 && i >= p_info.arguments.size() - p_info.default_arguments.size()) {
			const int default_index = i - (p_info.arguments.size() - p_info.default_arguments.size());
			argument["default_value"] = String(p_info.default_arguments[default_index]);
		}
		arguments.push_back(argument);
	}
	if (!arguments.is_empty()) {
		dict["arguments"] = arguments;
	}
	return dict;
}

Dictionary _make_classdb_constant_dict(const String &p_name, const String &p_class_name) {
	Dictionary dict;
	dict["kind"] = "constant";
	dict["class_name"] = p_class_name;
	dict["name"] = p_name;
	dict["qualified_name"] = p_class_name + "." + p_name;
	bool valid = false;
	const int64_t value = ClassDB::get_integer_constant(p_class_name, p_name, &valid);
	if (valid) {
		dict["value"] = String::num_int64(value);
		dict["is_value_valid"] = true;
	}
	const StringName enum_name = ClassDB::get_integer_constant_enum(p_class_name, p_name);
	if (enum_name != StringName()) {
		dict["enumeration"] = String(enum_name);
	}
	return dict;
}

Dictionary _make_classdb_enum_dict(const StringName &p_enum_name, const String &p_class_name) {
	Dictionary dict;
	dict["kind"] = "enum";
	dict["class_name"] = p_class_name;
	dict["name"] = String(p_enum_name);
	dict["qualified_name"] = p_class_name + "." + String(p_enum_name);
	return dict;
}

Dictionary _make_classdb_class_dict(const String &p_class_name) {
	Dictionary dict;
	dict["kind"] = "class";
	dict["class_name"] = p_class_name;
	dict["name"] = p_class_name;
	dict["qualified_name"] = p_class_name;
	if (ClassDB::class_exists(p_class_name)) {
		const String parent = ClassDB::get_parent_class(p_class_name);
		if (!parent.is_empty()) {
			dict["inherits"] = parent;
		}
		dict["can_instantiate"] = ClassDB::can_instantiate(p_class_name);

		List<PropertyInfo> properties;
		ClassDB::get_property_list(p_class_name, &properties);
		dict["property_count"] = properties.size();

		List<MethodInfo> methods;
		ClassDB::get_method_list(p_class_name, &methods);
		dict["method_count"] = methods.size();

		List<MethodInfo> signals;
		ClassDB::get_signal_list(p_class_name, &signals);
		dict["signal_count"] = signals.size();
	}
	return dict;
}

bool _is_classdb_visible_property(const PropertyInfo &p_info) {
	if (p_info.name.is_empty()) {
		return false;
	}
	if (p_info.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL)) {
		return false;
	}
	if (!(p_info.usage & (PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE))) {
		return false;
	}
	return true;
}

void _push_classdb_candidate(AIDocSearchTopResults &r_results, AIDocSearchKind p_kind, const String &p_class_name, const String &p_name, float p_score) {
	const String sort_key = _make_candidate_sort_key(p_kind, p_class_name, p_name);
	int candidate_index = -1;
	if (!_try_accept_candidate(r_results, p_score, sort_key, candidate_index)) {
		return;
	}

	AIDocSearchCandidate candidate;
	candidate.score = p_score;
	candidate.sort_key = sort_key;
	candidate.source = AIDocSearchSource::CLASS_DB;
	candidate.kind = p_kind;
	candidate.class_name = p_class_name;
	candidate.name = p_name;
	_store_candidate(r_results, candidate_index, candidate);
}

void _push_classdb_property_candidate(AIDocSearchTopResults &r_results, const String &p_class_name, const PropertyInfo &p_info, float p_score) {
	const String property_name = String(p_info.name);
	const String sort_key = _make_candidate_sort_key(AIDocSearchKind::PROPERTY, p_class_name, property_name);
	int candidate_index = -1;
	if (!_try_accept_candidate(r_results, p_score, sort_key, candidate_index)) {
		return;
	}

	AIDocSearchCandidate candidate;
	candidate.score = p_score;
	candidate.sort_key = sort_key;
	candidate.source = AIDocSearchSource::CLASS_DB;
	candidate.kind = AIDocSearchKind::PROPERTY;
	candidate.class_name = p_class_name;
	candidate.name = property_name;
	candidate.classdb_property = p_info;
	_store_candidate(r_results, candidate_index, candidate);
}

void _push_classdb_method_candidate(AIDocSearchTopResults &r_results, AIDocSearchKind p_kind, const String &p_class_name, const MethodInfo &p_info, float p_score) {
	const String method_name = String(p_info.name);
	const String sort_key = _make_candidate_sort_key(p_kind, p_class_name, method_name);
	int candidate_index = -1;
	if (!_try_accept_candidate(r_results, p_score, sort_key, candidate_index)) {
		return;
	}

	AIDocSearchCandidate candidate;
	candidate.score = p_score;
	candidate.sort_key = sort_key;
	candidate.source = AIDocSearchSource::CLASS_DB;
	candidate.kind = p_kind;
	candidate.class_name = p_class_name;
	candidate.name = method_name;
	candidate.classdb_method = p_info;
	_store_candidate(r_results, candidate_index, candidate);
}

Dictionary _make_search_candidate_dict(const AIDocSearchCandidate &p_candidate, bool p_include_descriptions) {
	Dictionary dict;

	if (p_candidate.source == AIDocSearchSource::DOC_DATA) {
		switch (p_candidate.kind) {
			case AIDocSearchKind::CLASS:
				if (p_candidate.class_doc) {
					dict = _make_class_dict(*p_candidate.class_doc, p_include_descriptions);
				}
				break;
			case AIDocSearchKind::PROPERTY:
				if (p_candidate.property_doc) {
					dict = _make_property_dict(*p_candidate.property_doc, p_candidate.class_name, p_include_descriptions);
				}
				break;
			case AIDocSearchKind::METHOD:
			case AIDocSearchKind::SIGNAL:
			case AIDocSearchKind::CONSTRUCTOR:
			case AIDocSearchKind::OPERATOR:
			case AIDocSearchKind::ANNOTATION:
				if (p_candidate.method_doc) {
					dict = _make_method_dict(*p_candidate.method_doc, p_candidate.class_name, _search_kind_to_string(p_candidate.kind), p_include_descriptions);
				}
				break;
			case AIDocSearchKind::CONSTANT:
				if (p_candidate.constant_doc) {
					dict = _make_constant_dict(*p_candidate.constant_doc, p_candidate.class_name, p_include_descriptions);
				}
				break;
			case AIDocSearchKind::THEME_ITEM:
				if (p_candidate.theme_item_doc) {
					dict = _make_theme_item_dict(*p_candidate.theme_item_doc, p_candidate.class_name, p_include_descriptions);
				}
				break;
			case AIDocSearchKind::ENUM:
				if (p_candidate.enum_doc) {
					dict = _make_enum_dict(p_candidate.name, *p_candidate.enum_doc, p_candidate.class_name, p_include_descriptions);
				}
				break;
		}
	} else {
		switch (p_candidate.kind) {
			case AIDocSearchKind::CLASS:
				dict = _make_classdb_class_dict(p_candidate.class_name);
				break;
			case AIDocSearchKind::PROPERTY:
				dict = _make_classdb_property_dict(p_candidate.classdb_property, p_candidate.class_name);
				break;
			case AIDocSearchKind::METHOD:
			case AIDocSearchKind::SIGNAL:
				dict = _make_classdb_method_dict(p_candidate.classdb_method, p_candidate.class_name, _search_kind_to_string(p_candidate.kind));
				break;
			case AIDocSearchKind::CONSTANT:
				dict = _make_classdb_constant_dict(p_candidate.name, p_candidate.class_name);
				break;
			case AIDocSearchKind::ENUM:
				dict = _make_classdb_enum_dict(StringName(p_candidate.name), p_candidate.class_name);
				break;
			case AIDocSearchKind::THEME_ITEM:
			case AIDocSearchKind::CONSTRUCTOR:
			case AIDocSearchKind::OPERATOR:
			case AIDocSearchKind::ANNOTATION:
				break;
		}
	}

	if (!dict.is_empty()) {
		dict["score"] = p_candidate.score;
	}
	return dict;
}

void _search_classdb_class(const String &p_class_name, const String &p_query, const String &p_kind_filter, AIDocSearchTopResults &r_results) {
	if (_kind_matches(p_kind_filter, "class")) {
		_push_classdb_candidate(r_results, AIDocSearchKind::CLASS, p_class_name, p_class_name, _score_match(p_query, p_class_name, p_class_name));
	}

	if (_kind_matches(p_kind_filter, "property")) {
		List<PropertyInfo> property_list;
		ClassDB::get_property_list(p_class_name, &property_list);
		for (const PropertyInfo &E : property_list) {
			if (!_is_classdb_visible_property(E)) {
				continue;
			}
			const String property_name = String(E.name);
			const String qualified_name = p_class_name + "." + property_name;
			_push_classdb_property_candidate(r_results, p_class_name, E, _score_match(p_query, property_name, qualified_name, E.hint_string));
		}
	}

	if (_kind_matches(p_kind_filter, "method")) {
		List<MethodInfo> method_list;
		ClassDB::get_method_list(p_class_name, &method_list, false, true);
		for (const MethodInfo &E : method_list) {
			const String qualified_name = p_class_name + "." + E.name;
			_push_classdb_method_candidate(r_results, AIDocSearchKind::METHOD, p_class_name, E, _score_match(p_query, E.name, qualified_name));
		}
	}

	if (_kind_matches(p_kind_filter, "signal")) {
		List<MethodInfo> signal_list;
		ClassDB::get_signal_list(p_class_name, &signal_list);
		for (const MethodInfo &E : signal_list) {
			const String qualified_name = p_class_name + "." + E.name;
			_push_classdb_method_candidate(r_results, AIDocSearchKind::SIGNAL, p_class_name, E, _score_match(p_query, E.name, qualified_name));
		}
	}

	if (_kind_matches(p_kind_filter, "constant")) {
		List<String> constants;
		ClassDB::get_integer_constant_list(p_class_name, &constants);
		for (const String &E : constants) {
			const String qualified_name = p_class_name + "." + E;
			_push_classdb_candidate(r_results, AIDocSearchKind::CONSTANT, p_class_name, E, _score_match(p_query, E, qualified_name));
		}
	}

	if (_kind_matches(p_kind_filter, "enum")) {
		List<StringName> enums;
		ClassDB::get_enum_list(p_class_name, &enums);
		for (const StringName &E : enums) {
			const String enum_name = String(E);
			const String qualified_name = p_class_name + "." + enum_name;
			_push_classdb_candidate(r_results, AIDocSearchKind::ENUM, p_class_name, enum_name, _score_match(p_query, enum_name, qualified_name));
		}
	}
}

bool _is_writable_tool_property(const PropertyInfo &p_info) {
	if (p_info.name.is_empty()) {
		return false;
	}
	if (p_info.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_READ_ONLY)) {
		return false;
	}
	if (!(p_info.usage & (PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE))) {
		return false;
	}
	return true;
}

} // namespace

void AIV1DocumentationService::_bind_methods() {
}

AIV1DocumentationService *AIV1DocumentationService::get_dispatcher_singleton() {
	static Ref<AIV1DocumentationService> dispatcher;
	if (dispatcher.is_null()) {
		dispatcher.instantiate();
	}
	return dispatcher.ptr();
}

AIV1DocumentationResult AIV1DocumentationService::_dispatch_to_main_thread(MainThreadRequest &r_request) {
	return _dispatch_main_thread_request<AIV1DocumentationResult>(r_request, get_dispatcher_singleton(), &AIV1DocumentationService::_execute_request, request_mutex, "Failed to schedule documentation search on the main thread.");
}

void AIV1DocumentationService::_execute_request(uint64_t p_request_ptr) {
	_execute_request_ptr(reinterpret_cast<MainThreadRequest *>(p_request_ptr));
}

void AIV1DocumentationService::_execute_request_ptr(MainThreadRequest *p_request) {
	ERR_FAIL_NULL(p_request);
	p_request->result = _search_main_thread(p_request->query, p_request->class_name, p_request->kind, p_request->max_results, p_request->include_descriptions);
	p_request->done.post();
}

AIV1DocumentationResult AIV1DocumentationService::_search_main_thread(const String &p_query, const String &p_class_name, const String &p_kind, int p_max_results, bool p_include_descriptions) const {
	AIV1DocumentationResult result;
	const String query = p_query.strip_edges();
	const String requested_class_name = p_class_name.strip_edges();
	String kind_error;
	const String kind_filter = _normalize_kind_filter(p_kind, kind_error);
	if (!kind_error.is_empty()) {
		result.error = kind_error;
		return result;
	}
	if (query.is_empty() && requested_class_name.is_empty()) {
		result.error = "Provide query, class_name, or both.";
		return result;
	}

	String error;
	DocTools *doc = _get_ready_doc_data_main_thread(error);
	if (!error.is_empty()) {
		result.error = error;
		return result;
	}
	const bool has_doc_data = doc && !doc->class_list.is_empty();

	const int max_results = CLAMP(p_max_results, 1, 80);
	AIDocSearchTopResults search_results;
	search_results.max_results = max_results;

	if (has_doc_data && !requested_class_name.is_empty()) {
		const DocData::ClassDoc *class_doc = _find_class_doc(doc, requested_class_name);
		if (class_doc) {
			_search_class_doc(*class_doc, query, kind_filter, search_results);
		} else if (ClassDB::class_exists(requested_class_name)) {
			_search_classdb_class(requested_class_name, query, kind_filter, search_results);
		} else {
			Array suggestions;
			for (const KeyValue<String, DocData::ClassDoc> &E : doc->class_list) {
				const float score = _score_match(requested_class_name, E.key, E.key);
				if (score < 0.35f) {
					continue;
				}
				Dictionary suggestion;
				suggestion["class_name"] = E.key;
				suggestion["score"] = score;
				suggestions.push_back(suggestion);
			}
			result.error = vformat("Class documentation `%s` was not found.", requested_class_name);
			result.metadata["suggestions"] = suggestions;
			return result;
		}
	} else if (has_doc_data) {
		for (const KeyValue<String, DocData::ClassDoc> &E : doc->class_list) {
			_search_class_doc(E.value, query, kind_filter, search_results);
		}
	} else if (!requested_class_name.is_empty()) {
		if (!ClassDB::class_exists(requested_class_name)) {
			Array suggestions;
			LocalVector<StringName> classes;
			ClassDB::get_class_list(classes);
			for (uint32_t i = 0; i < classes.size(); i++) {
				const String class_name = String(classes[i]);
				const float score = _score_match(requested_class_name, class_name, class_name);
				if (score < 0.35f) {
					continue;
				}
				Dictionary suggestion;
				suggestion["class_name"] = class_name;
				suggestion["score"] = score;
				suggestions.push_back(suggestion);
			}
			result.error = vformat("Class `%s` was not found in Godot documentation or ClassDB.", requested_class_name);
			result.metadata["suggestions"] = suggestions;
			return result;
		}
		_search_classdb_class(requested_class_name, query, kind_filter, search_results);
	} else {
		LocalVector<StringName> classes;
		ClassDB::get_class_list(classes);
		for (uint32_t i = 0; i < classes.size(); i++) {
			_search_classdb_class(String(classes[i]), query, kind_filter, search_results);
		}
	}

	search_results.candidates.sort_custom<AIDocSearchCandidateCompare>();

	Array results;
	String content;
	content += "Godot documentation search results";
	if (!query.is_empty()) {
		content += vformat(" for `%s`", query);
	}
	if (!requested_class_name.is_empty()) {
		content += vformat(" in `%s`", requested_class_name);
	}
	content += "\n";

	for (int i = 0; i < search_results.candidates.size() && results.size() < max_results; i++) {
		Dictionary item = _make_search_candidate_dict(search_results.candidates[i], p_include_descriptions);
		if (item.is_empty()) {
			continue;
		}
		results.push_back(item);

		content += vformat("- [%s] %s", String(item["kind"]), String(item["qualified_name"]));
		if (item.has("type")) {
			content += ": " + String(item["type"]);
		} else if (item.has("return_type")) {
			content += " -> " + String(item["return_type"]);
		}
		if (item.has("brief_description")) {
			content += " - " + String(item["brief_description"]);
		} else if (item.has("description")) {
			content += " - " + String(item["description"]);
		}
		content += "\n";
	}

	if (results.is_empty()) {
		content += "No matching documentation entries found.\n";
	}
	if (search_results.total_matches > results.size()) {
		content += vformat("... %d more results omitted. Narrow query, class_name, or kind.\n", search_results.total_matches - results.size());
	}

	result.success = true;
	result.message = content.strip_edges();
	result.metadata["query"] = query;
	result.metadata["class_name"] = requested_class_name;
	result.metadata["kind"] = kind_filter;
	result.metadata["results"] = results;
	result.metadata["result_count"] = results.size();
	result.metadata["total_matches"] = search_results.total_matches;
	result.metadata["max_results"] = max_results;
	result.metadata["source"] = has_doc_data ? String("DocData") : String("ClassDB");
	return result;
}

AIV1DocumentationResult AIV1DocumentationService::search(const String &p_query, const String &p_class_name, const String &p_kind, int p_max_results, bool p_include_descriptions) {
	MainThreadRequest request;
	request.query = p_query;
	request.class_name = p_class_name;
	request.kind = p_kind;
	request.max_results = p_max_results;
	request.include_descriptions = p_include_descriptions;
	return _dispatch_to_main_thread(request);
}

Array AIV1DocumentationService::get_writable_property_suggestions(Object *p_object, const String &p_property_path, int p_max_suggestions) {
	Array suggestions;
	ERR_FAIL_NULL_V(p_object, suggestions);

	Object *suggestion_object = p_object;
	String requested_property = p_property_path.strip_edges();
	String prefix;
	const int colon_index = requested_property.find(":");
	if (colon_index > 0) {
		const String base_property = requested_property.substr(0, colon_index).strip_edges();
		const String nested_property = requested_property.substr(colon_index + 1).strip_edges();
		bool valid = false;
		Variant value = p_object->get(StringName(base_property), &valid);
		Object *nested_object = value;
		if (valid && nested_object) {
			suggestion_object = nested_object;
			requested_property = nested_property;
			prefix = base_property + ":";
		}
	}

	const int max_suggestions = CLAMP(p_max_suggestions, 1, 12);
	DocTools *doc = _get_doc_data_if_ready_main_thread();
	const String object_class = suggestion_object->get_class();

	List<PropertyInfo> property_list;
	suggestion_object->get_property_list(&property_list);

	Vector<AIDocPropertySuggestion> scored_suggestions;
	for (const PropertyInfo &E : property_list) {
		if (!_is_writable_tool_property(E)) {
			continue;
		}

		const String property_name = String(E.name);
		const String property_path = prefix + property_name;
		float score = _score_match(requested_property, property_name, object_class + "." + property_name, E.hint_string);
		if (score < 0.22f) {
			continue;
		}

		Dictionary suggestion;
		suggestion["property_path"] = property_path;
		suggestion["type"] = Variant::get_type_name(E.type);
		suggestion["hint"] = int(E.hint);
		suggestion["hint_text"] = E.hint_string;
		suggestion["class_name"] = object_class;
		suggestion["score"] = score;

		if (doc) {
			const DocData::PropertyDoc *property_doc = _find_property_doc(doc, object_class, property_name);
			if (property_doc && !property_doc->description.is_empty()) {
				suggestion["description"] = _clean_doc_text(property_doc->description, 180);
			}
			if (property_doc && !property_doc->default_value.is_empty()) {
				suggestion["default_value"] = property_doc->default_value;
			}
		}

		AIDocPropertySuggestion scored;
		scored.score = score;
		scored.sort_key = property_path;
		scored.data = suggestion;
		scored_suggestions.push_back(scored);
	}

	scored_suggestions.sort_custom<AIDocPropertySuggestionCompare>();
	for (int i = 0; i < scored_suggestions.size() && suggestions.size() < max_suggestions; i++) {
		suggestions.push_back(scored_suggestions[i].data);
	}
	return suggestions;
}

String AIV1DocumentationService::format_property_suggestions_for_error(Object *p_object, const String &p_object_label, const String &p_property_path, int p_max_suggestions) {
	ERR_FAIL_NULL_V(p_object, String());

	Array suggestions = get_writable_property_suggestions(p_object, p_property_path, p_max_suggestions);
	if (suggestions.is_empty()) {
		return vformat(" No close writable properties were found on type `%s`. Call scene.list_properties for `%s` with a narrow filter before retrying.", p_object->get_class(), p_object_label);
	}

	Vector<String> suggestion_texts;
	for (int i = 0; i < suggestions.size(); i++) {
		Dictionary suggestion = suggestions[i];
		String text = vformat("`%s` (%s)", String(suggestion["property_path"]), String(suggestion["type"]));
		if (suggestion.has("default_value")) {
			text += " default=" + String(suggestion["default_value"]);
		}
		suggestion_texts.push_back(text);
	}

	return vformat(" Closest writable properties on type `%s`: %s. Call scene.list_properties for `%s` to confirm exact property paths and value types before retrying.", p_object->get_class(), String(", ").join(suggestion_texts), p_object_label);
}
