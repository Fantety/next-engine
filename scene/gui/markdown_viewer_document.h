/**************************************************************************/
/*  markdown_viewer_document.h                                            */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

struct MarkdownViewerInline {
	enum Type {
		TYPE_TEXT,
		TYPE_STRONG,
		TYPE_EMPHASIS,
		TYPE_CODE,
		TYPE_LINK,
		TYPE_IMAGE,
		TYPE_STRIKETHROUGH,
	};

	Type type = TYPE_TEXT;
	String text;
	String source;
	String title;
	Vector<MarkdownViewerInline> children;
};

struct MarkdownViewerTableCell {
	Vector<MarkdownViewerInline> inlines;
	String plain_text;
};

struct MarkdownViewerTableRow {
	Vector<MarkdownViewerTableCell> cells;
	bool header = false;
};

struct MarkdownViewerBlock {
	enum Type {
		TYPE_PARAGRAPH,
		TYPE_HEADING,
		TYPE_LIST,
		TYPE_LIST_ITEM,
		TYPE_BLOCK_QUOTE,
		TYPE_CODE_BLOCK,
		TYPE_TABLE,
		TYPE_IMAGE,
		TYPE_THEMATIC_BREAK,
	};

	Type type = TYPE_PARAGRAPH;
	int heading_level = 0;
	bool ordered_list = false;
	String language;
	String plain_text;
	String source;
	String title;
	Vector<MarkdownViewerInline> inlines;
	Vector<MarkdownViewerBlock> children;
	Vector<MarkdownViewerTableRow> table_rows;
};

struct MarkdownViewerDocument {
	Vector<MarkdownViewerBlock> blocks;

	void clear() {
		blocks.clear();
	}
};

class MarkdownViewerDocumentBuilder {
	String _flatten_inlines(const Vector<MarkdownViewerInline> &p_inlines) const;
	Vector<MarkdownViewerInline> _parse_inline_markdown(const String &p_markdown) const;
	void _append_markdown_range(const String &p_markdown, MarkdownViewerDocument &r_document) const;
	bool _try_parse_pipe_table(const Vector<String> &p_lines, int p_start, MarkdownViewerBlock &r_block, int &r_end) const;

public:
	MarkdownViewerDocument build(const String &p_markdown) const;
	String flatten_inlines_for_test(const Vector<MarkdownViewerInline> &p_inlines) const { return _flatten_inlines(p_inlines); }
};
