#include "string_tag_wrapper.h"

String StringTagWrapper::wrap_with_tag(const String& content, const String& tag) {
    return "<" + tag + ">" + content + "</" + tag + ">";
}

String StringTagWrapper::wrap_with_question_tag(const String& content) {
    return wrap_with_tag(content, "question");
}