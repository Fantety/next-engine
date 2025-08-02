#ifndef STRING_TAG_WRAPPER_H
#define STRING_TAG_WRAPPER_H

#include "core/string/ustring.h"

class StringTagWrapper {
public:
    static String wrap_with_tag(const String& content, const String& tag);
    static String wrap_with_question_tag(const String& content);
};

#endif // STRING_TAG_WRAPPER_H