/**************************************************************************/
/*  ai_requirement_form_tool.cpp                                          */
/**************************************************************************/

#include "ai_requirement_form_tool.h"

#include "core/io/json.h"
#include "core/variant/variant.h"

const char *AIRequirementFormTool::TOOL_NAME = "agent.collect_requirements";

namespace {

Dictionary _make_string_property(const String &p_description) {
	Dictionary property;
	property["type"] = "string";
	property["description"] = p_description;
	return property;
}

Dictionary _normalize_form(const Dictionary &p_arguments) {
	Dictionary form;
	form["title"] = String(p_arguments.get("title", "Confirm requirements")).strip_edges();
	form["purpose"] = String(p_arguments.get("purpose", "Clarify the user's requirements before continuing.")).strip_edges();
	if (p_arguments.has("questions") && Variant(p_arguments["questions"]).get_type() == Variant::ARRAY) {
		form["questions"] = Array(p_arguments["questions"]).duplicate(true);
	} else {
		form["questions"] = Array();
	}
	if (p_arguments.has("sections") && Variant(p_arguments["sections"]).get_type() == Variant::ARRAY) {
		form["sections"] = Array(p_arguments["sections"]).duplicate(true);
	}
	return form;
}

String _format_answers_markdown(const Dictionary &p_answers) {
	String text;
	Array keys = p_answers.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = String(keys[i]);
		text += "- " + key + ": " + String(p_answers.get(key, Variant())) + "\n";
	}
	return text;
}

bool _question_has_required_fields(const Dictionary &p_question) {
	const String id = String(p_question.get("id", "")).strip_edges();
	const String label = String(p_question.get("label", "")).strip_edges();
	const String type = String(p_question.get("type", "")).strip_edges();
	return !id.is_empty() && !label.is_empty() && !type.is_empty();
}

} // namespace

String AIRequirementFormTool::get_name() const {
	return TOOL_NAME;
}

String AIRequirementFormTool::get_description() const {
	return "Creates a user-facing requirement confirmation form and pauses the agent until the user submits structured answers. Use this before planning or implementing when the brief is underspecified.";
}

Dictionary AIRequirementFormTool::get_parameters_schema() const {
	Dictionary schema;
	schema["type"] = "object";

	Dictionary properties;
	properties["title"] = _make_string_property("Short form title shown to the user, for example `Confirm game requirements`.");
	properties["purpose"] = _make_string_property("One sentence explaining what this form clarifies before the agent continues.");

	Dictionary option_item;
	option_item["type"] = "string";
	Dictionary options_property;
	options_property["type"] = "array";
	options_property["description"] = "Selectable labels for single_choice or multi_choice questions.";
	options_property["items"] = option_item;

	Dictionary question_properties;
	question_properties["id"] = _make_string_property("Stable snake_case key used in the submitted answers.");
	question_properties["label"] = _make_string_property("User-facing question label.");
	question_properties["type"] = _make_string_property("Question type: text, multiline, single_choice, multi_choice, boolean, or number.");
	question_properties["help"] = _make_string_property("Optional short helper text shown under the question.");
	question_properties["default"] = _make_string_property("Optional default value.");
	question_properties["options"] = options_property;

	Array question_required;
	question_required.push_back("id");
	question_required.push_back("label");
	question_required.push_back("type");

	Dictionary question_item;
	question_item["type"] = "object";
	question_item["properties"] = question_properties;
	question_item["required"] = question_required;

	Dictionary questions_property;
	questions_property["type"] = "array";
	questions_property["description"] = "Ordered list of requirement questions. Cover relevant style, UI, menu/home screen, operation logic, rules, scoring, content scope, target platform, and acceptance expectations.";
	questions_property["items"] = question_item;
	properties["questions"] = questions_property;

	Dictionary sections_item;
	sections_item["type"] = "string";
	Dictionary sections_property;
	sections_property["type"] = "array";
	sections_property["description"] = "Optional section headings for grouping questions.";
	sections_property["items"] = sections_item;
	properties["sections"] = sections_property;

	Array required;
	required.push_back("title");
	required.push_back("questions");

	schema["properties"] = properties;
	schema["required"] = required;
	return schema;
}

AIToolResult AIRequirementFormTool::execute(const Dictionary &p_arguments) {
	AIToolResult result;
	Dictionary form = _normalize_form(p_arguments);
	const Array questions = form["questions"];
	if (String(form.get("title", "")).strip_edges().is_empty()) {
		result.error = "Missing required title.";
		return result;
	}
	if (questions.is_empty()) {
		result.error = "Missing required questions.";
		return result;
	}
	for (int i = 0; i < questions.size(); i++) {
		if (Variant(questions[i]).get_type() != Variant::DICTIONARY || !_question_has_required_fields(Dictionary(questions[i]))) {
			result.error = vformat("Question %d must include id, label, and type.", i);
			return result;
		}
	}

	result.content = "Requirement confirmation form is ready. Wait for the user to submit answers before planning or implementing.";
	result.metadata["type"] = "requirement_form";
	result.metadata["form"] = form;
	return result;
}

bool AIRequirementFormTool::is_requirement_form_tool(const String &p_tool_name) {
	return p_tool_name == TOOL_NAME;
}

AIToolResult AIRequirementFormTool::make_submission_result(const Dictionary &p_form_arguments, const Dictionary &p_answers) {
	AIToolResult result;
	Dictionary form = _normalize_form(p_form_arguments);
	Dictionary answers = p_answers.duplicate(true);
	result.content = "Requirement confirmation submitted.\n\nConfirmed answers:\n" + _format_answers_markdown(answers);
	result.metadata["type"] = "requirement_form_result";
	result.metadata["form"] = form;
	result.metadata["answers"] = answers;
	result.metadata["answers_json"] = JSON::stringify(answers, "\t", false);
	return result;
}
