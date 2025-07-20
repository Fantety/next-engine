#ifndef MARKDOWN_RENDERER_H
#define MARKDOWN_RENDERER_H

#include "scene/gui/box_container.h"
#include "core/markdown/markdown_parser.h"

class MarkdownViewer : public VBoxContainer {
    GDCLASS(MarkdownViewer, VBoxContainer);

protected:
    static void _bind_methods();
private:
    String markdown_text;
public:
    MarkdownViewer();
    ~MarkdownViewer();
};

#endif // MARKDOWN_RENDERER_H
