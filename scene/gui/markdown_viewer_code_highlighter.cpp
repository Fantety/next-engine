/**************************************************************************/
/*  markdown_viewer_code_highlighter.cpp                                  */
/**************************************************************************/

#include "markdown_viewer_code_highlighter.h"

bool MarkdownViewerCodeHighlighter::_is_identifier_char(char32_t p_char) const {
	return (p_char >= 'a' && p_char <= 'z') || (p_char >= 'A' && p_char <= 'Z') || (p_char >= '0' && p_char <= '9') || p_char == '_';
}

bool MarkdownViewerCodeHighlighter::_is_keyword(const String &p_language, const String &p_word) const {
	const String language = p_language.to_lower();

	static const char *gdscript_keywords[] = {
		"extends", "class_name", "func", "var", "const", "if", "elif", "else", "for", "while", "return", "match", "await", "signal", "true", "false", "null", "self", "pass", "break", "continue"
	};
	static const char *json_keywords[] = { "true", "false", "null" };
	static const char *cpp_keywords[] = {
		"class", "struct", "enum", "public", "private", "protected", "virtual", "override", "const", "static", "if", "else", "for", "while", "return", "switch", "case", "break", "continue", "true", "false", "nullptr", "auto", "void", "int", "float", "double", "bool"
	};

	const char **keywords = nullptr;
	int keyword_count = 0;
	if (language == "gdscript" || language == "gd") {
		keywords = gdscript_keywords;
		keyword_count = sizeof(gdscript_keywords) / sizeof(gdscript_keywords[0]);
	} else if (language == "json") {
		keywords = json_keywords;
		keyword_count = sizeof(json_keywords) / sizeof(json_keywords[0]);
	} else if (language == "cpp" || language == "c++" || language == "c" || language == "h" || language == "hpp") {
		keywords = cpp_keywords;
		keyword_count = sizeof(cpp_keywords) / sizeof(cpp_keywords[0]);
	}

	for (int i = 0; i < keyword_count; i++) {
		if (p_word == keywords[i]) {
			return true;
		}
	}
	return false;
}

void MarkdownViewerCodeHighlighter::_push_span(Vector<MarkdownViewerCodeSpan> &r_spans, MarkdownViewerCodeSpan::Type p_type, const String &p_text) const {
	if (p_text.is_empty()) {
		return;
	}
	if (!r_spans.is_empty() && r_spans.write[r_spans.size() - 1].type == p_type) {
		r_spans.write[r_spans.size() - 1].text += p_text;
		return;
	}

	MarkdownViewerCodeSpan span;
	span.type = p_type;
	span.text = p_text;
	r_spans.push_back(span);
}

Vector<MarkdownViewerCodeSpan> MarkdownViewerCodeHighlighter::highlight_line(const String &p_language, const String &p_line) const {
	Vector<MarkdownViewerCodeSpan> spans;
	const String language = p_language.to_lower();
	int index = 0;

	while (index < p_line.length()) {
		const char32_t c = p_line[index];

		if ((language == "gdscript" || language == "gd") && c == '#') {
			_push_span(spans, MarkdownViewerCodeSpan::TYPE_COMMENT, p_line.substr(index));
			break;
		}
		if ((language == "cpp" || language == "c++" || language == "c" || language == "h" || language == "hpp" || language.is_empty()) && c == '/' && index + 1 < p_line.length() && p_line[index + 1] == '/') {
			_push_span(spans, MarkdownViewerCodeSpan::TYPE_COMMENT, p_line.substr(index));
			break;
		}

		if (c == '"' || c == '\'') {
			const char32_t quote = c;
			int end = index + 1;
			bool escaped = false;
			while (end < p_line.length()) {
				const char32_t ec = p_line[end];
				if (ec == quote && !escaped) {
					end++;
					break;
				}
				escaped = ec == '\\' && !escaped;
				if (ec != '\\') {
					escaped = false;
				}
				end++;
			}
			_push_span(spans, MarkdownViewerCodeSpan::TYPE_STRING, p_line.substr(index, end - index));
			index = end;
			continue;
		}

		if (c >= '0' && c <= '9') {
			int end = index + 1;
			while (end < p_line.length() && ((p_line[end] >= '0' && p_line[end] <= '9') || p_line[end] == '.')) {
				end++;
			}
			_push_span(spans, MarkdownViewerCodeSpan::TYPE_NUMBER, p_line.substr(index, end - index));
			index = end;
			continue;
		}

		if (_is_identifier_char(c) && !(c >= '0' && c <= '9')) {
			int end = index + 1;
			while (end < p_line.length() && _is_identifier_char(p_line[end])) {
				end++;
			}
			const String word = p_line.substr(index, end - index);
			_push_span(spans, _is_keyword(language, word) ? MarkdownViewerCodeSpan::TYPE_KEYWORD : MarkdownViewerCodeSpan::TYPE_NORMAL, word);
			index = end;
			continue;
		}

		_push_span(spans, MarkdownViewerCodeSpan::TYPE_NORMAL, String::chr(c));
		index++;
	}

	if (spans.is_empty()) {
		MarkdownViewerCodeSpan span;
		span.text = p_line;
		spans.push_back(span);
	}
	return spans;
}
