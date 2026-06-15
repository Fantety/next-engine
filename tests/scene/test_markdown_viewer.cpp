/**************************************************************************/
/*  test_markdown_viewer.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_markdown_viewer)

#include "core/io/image.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/object/class_db.h"
#include "modules/modules_enabled.gen.h"
#include "scene/gui/markdown_viewer.h"
#include "scene/gui/markdown_viewer_code_highlighter.h"
#include "scene/gui/markdown_viewer_document.h"
#include "scene/gui/markdown_viewer_draw.h"
#include "scene/gui/markdown_viewer_image_loader.h"
#include "scene/gui/markdown_viewer_layout.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/font.h"
#include "scene/theme/theme_db.h"
#include "tests/display_server_mock.h"
#include "tests/signal_watcher.h"
#include "tests/test_tools.h"

namespace TestMarkdownViewer {

static bool _has_inline_type(const Vector<MarkdownViewerInline> &p_inlines, MarkdownViewerInline::Type p_type) {
	for (const MarkdownViewerInline &run : p_inlines) {
		if (run.type == p_type || _has_inline_type(run.children, p_type)) {
			return true;
		}
	}
	return false;
}

TEST_CASE("[SceneTree][MarkdownViewer] Runtime class is registered and defaults are conservative") {
	CHECK(ClassDB::class_exists("MarkdownViewer"));

	MarkdownViewer *viewer = memnew(MarkdownViewer);

	CHECK(viewer->get_markdown().is_empty());
	CHECK_FALSE(viewer->is_remote_images_enabled());
	CHECK_FALSE(viewer->is_open_links_enabled());
	CHECK(viewer->is_scroll_enabled());
	CHECK(viewer->is_syntax_highlighting_enabled());
	CHECK(viewer->is_code_copy_enabled());
	CHECK(viewer->is_clipping_contents());
	CHECK(viewer->get_content_height() == doctest::Approx(0.0));

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Markdown property stores source text") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);

	viewer->set_markdown("# Title\n\nBody");

	CHECK(viewer->get_markdown() == "# Title\n\nBody");

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Document builder preserves common Markdown blocks") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("# Title\n\nBody with [Godot](https://godotengine.org) and `code`.\n\n- One\n- Two\n\n```gdscript\nextends Node\n```\n\n![Alt](res://icon.svg)");

	REQUIRE(document.blocks.size() == 5);
	CHECK(document.blocks[0].type == MarkdownViewerBlock::TYPE_HEADING);
	CHECK(document.blocks[0].heading_level == 1);
	CHECK(document.blocks[0].plain_text == "Title");
	CHECK(document.blocks[1].type == MarkdownViewerBlock::TYPE_PARAGRAPH);
	CHECK(document.blocks[1].inlines.size() >= 4);
	CHECK(document.blocks[2].type == MarkdownViewerBlock::TYPE_LIST);
	CHECK(document.blocks[3].type == MarkdownViewerBlock::TYPE_CODE_BLOCK);
	CHECK(document.blocks[3].language == "gdscript");
	CHECK(document.blocks[3].plain_text == "extends Node\n");
	CHECK(document.blocks[4].type == MarkdownViewerBlock::TYPE_IMAGE);
	CHECK(document.blocks[4].source == "res://icon.svg");
	CHECK(document.blocks[4].plain_text == "Alt");
}

TEST_CASE("[SceneTree][MarkdownViewer] Document builder recognizes pipe tables") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("Before\n\n| Name | Value |\n| --- | --- |\n| HP | 100 |\n| MP | 50 |\n\nAfter");

	REQUIRE(document.blocks.size() == 3);
	CHECK(document.blocks[0].type == MarkdownViewerBlock::TYPE_PARAGRAPH);
	CHECK(document.blocks[1].type == MarkdownViewerBlock::TYPE_TABLE);
	REQUIRE(document.blocks[1].table_rows.size() == 3);
	CHECK(document.blocks[1].table_rows[0].cells[0].plain_text == "Name");
	CHECK(document.blocks[1].table_rows[1].cells[1].plain_text == "100");
	CHECK(document.blocks[2].type == MarkdownViewerBlock::TYPE_PARAGRAPH);
}

TEST_CASE("[SceneTree][MarkdownViewer] Document builder recognizes thematic breaks and inline styles") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("Before\n\n---\n\n**Bold** *Italic* ***Both*** ~~Gone~~ `code`");

	REQUIRE(document.blocks.size() == 3);
	CHECK(document.blocks[0].type == MarkdownViewerBlock::TYPE_PARAGRAPH);
	CHECK(document.blocks[1].type == MarkdownViewerBlock::TYPE_THEMATIC_BREAK);
	REQUIRE(document.blocks[2].type == MarkdownViewerBlock::TYPE_PARAGRAPH);
	CHECK(_has_inline_type(document.blocks[2].inlines, MarkdownViewerInline::TYPE_STRONG));
	CHECK(_has_inline_type(document.blocks[2].inlines, MarkdownViewerInline::TYPE_EMPHASIS));
	CHECK(_has_inline_type(document.blocks[2].inlines, MarkdownViewerInline::TYPE_STRIKETHROUGH));
	CHECK(_has_inline_type(document.blocks[2].inlines, MarkdownViewerInline::TYPE_CODE));
	CHECK(document.blocks[2].plain_text == "Bold Italic Both Gone code");
}

TEST_CASE("[SceneTree][MarkdownViewer] Document builder preserves nested lists and blockquotes") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("- One\n  1. First\n  2. Second\n- Two\n\n> Outer\n> > Inner");

	REQUIRE(document.blocks.size() == 2);
	REQUIRE(document.blocks[0].type == MarkdownViewerBlock::TYPE_LIST);
	REQUIRE(document.blocks[0].children.size() == 2);
	CHECK(document.blocks[0].children[0].plain_text == "One");
	REQUIRE(document.blocks[0].children[0].children.size() == 1);
	CHECK(document.blocks[0].children[0].children[0].type == MarkdownViewerBlock::TYPE_LIST);
	CHECK(document.blocks[0].children[0].children[0].ordered_list);
	REQUIRE(document.blocks[0].children[0].children[0].children.size() == 2);
	CHECK(document.blocks[0].children[0].children[0].children[0].plain_text == "First");

	REQUIRE(document.blocks[1].type == MarkdownViewerBlock::TYPE_BLOCK_QUOTE);
	REQUIRE(document.blocks[1].children.size() == 2);
	CHECK(document.blocks[1].children[0].type == MarkdownViewerBlock::TYPE_PARAGRAPH);
	CHECK(document.blocks[1].children[1].type == MarkdownViewerBlock::TYPE_BLOCK_QUOTE);
}

TEST_CASE("[SceneTree][MarkdownViewer] Image loader blocks remote URLs by default") {
	MarkdownViewerImageLoader *loader = memnew(MarkdownViewerImageLoader);
	loader->set_remote_images_enabled(false);

	MarkdownViewerImageRequestResult result = loader->ensure_image("https://example.com/image.png");

	CHECK(result.status == MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED);
	CHECK_FALSE(loader->has_pending_requests());

	memdelete(loader);
}

TEST_CASE("[SceneTree][MarkdownViewer] Image loader decodes PNG buffers") {
	Ref<Image> image = memnew(Image(2, 3, false, Image::FORMAT_RGBA8));
	image->fill(Color(1, 0, 0, 1));
	Vector<uint8_t> png = image->save_png_to_buffer();

	MarkdownViewerImageLoader *loader = memnew(MarkdownViewerImageLoader);
	MarkdownViewerImageRequestResult result = loader->decode_buffer_for_test("memory.png", png);

	CHECK(result.status == MarkdownViewerImageStatus::STATUS_LOADED);
	REQUIRE(result.texture.is_valid());
	CHECK(result.size == Size2(2, 3));

	memdelete(loader);
}

#ifdef MODULE_JPG_ENABLED
TEST_CASE("[SceneTree][MarkdownViewer] Image loader decodes JPG buffers without PNG probe errors") {
	Ref<Image> image = memnew(Image(2, 3, false, Image::FORMAT_RGB8));
	image->fill(Color(0, 0, 1, 1));
	Vector<uint8_t> jpg = image->save_jpg_to_buffer();
	REQUIRE_FALSE(jpg.is_empty());

	MarkdownViewerImageLoader *loader = memnew(MarkdownViewerImageLoader);
	ErrorDetector error_detector;
	MarkdownViewerImageRequestResult result = loader->decode_buffer_for_test("memory.jpg", jpg);

	CHECK_FALSE(error_detector.has_error);
	CHECK(result.status == MarkdownViewerImageStatus::STATUS_LOADED);
	REQUIRE(result.texture.is_valid());
	CHECK(result.size == Size2(2, 3));

	memdelete(loader);
}
#endif // MODULE_JPG_ENABLED

TEST_CASE("[SceneTree][MarkdownViewer] Image loader rejects unknown buffers without decoder probe errors") {
	Vector<uint8_t> data;
	data.resize(4);
	data.write[0] = 't';
	data.write[1] = 'e';
	data.write[2] = 'x';
	data.write[3] = 't';

	MarkdownViewerImageLoader *loader = memnew(MarkdownViewerImageLoader);
	ErrorDetector error_detector;
	MarkdownViewerImageRequestResult result = loader->decode_buffer_for_test("memory.txt", data);

	CHECK_FALSE(error_detector.has_error);
	CHECK(result.status == MarkdownViewerImageStatus::STATUS_FAILED);

	memdelete(loader);
}

TEST_CASE("[SceneTree][MarkdownViewer] Image loader marks remote image as loading when enabled") {
	MarkdownViewerImageLoader *loader = memnew(MarkdownViewerImageLoader);
	loader->set_remote_images_enabled(true);

	MarkdownViewerImageRequestResult result = loader->ensure_image("https://example.com/image.png");

	CHECK(result.status == MarkdownViewerImageStatus::STATUS_LOADING);
	CHECK(loader->has_pending_requests());

	memdelete(loader);
}

TEST_CASE("[SceneTree][MarkdownViewer] Image loader can enable a previously blocked remote URL") {
	MarkdownViewerImageLoader *loader = memnew(MarkdownViewerImageLoader);

	MarkdownViewerImageRequestResult blocked = loader->ensure_image("https://example.com/image.png");
	CHECK(blocked.status == MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED);

	loader->set_remote_images_enabled(true);
	MarkdownViewerImageRequestResult loading = loader->ensure_image("https://example.com/image.png");
	CHECK(loading.status == MarkdownViewerImageStatus::STATUS_LOADING);
	CHECK(loader->has_pending_requests());

	memdelete(loader);
}

TEST_CASE("[SceneTree][MarkdownViewer] Image loader handles completed remote buffers") {
	Ref<Image> image = memnew(Image(4, 5, false, Image::FORMAT_RGBA8));
	image->fill(Color(0, 1, 0, 1));
	Vector<uint8_t> png = image->save_png_to_buffer();

	MarkdownViewerImageLoader *loader = memnew(MarkdownViewerImageLoader);
	loader->set_remote_images_enabled(true);
	loader->ensure_image("https://example.com/image.png");
	loader->complete_request_for_test("https://example.com/image.png", HTTPRequest::RESULT_SUCCESS, 200, PackedStringArray(), png);

	MarkdownViewerImageRequestResult result = loader->ensure_image("https://example.com/image.png");

	CHECK(result.status == MarkdownViewerImageStatus::STATUS_LOADED);
	CHECK(result.size == Size2(4, 5));

	memdelete(loader);
}

TEST_CASE("[SceneTree][MarkdownViewer] Code highlighter identifies GDScript comments and keywords") {
	MarkdownViewerCodeHighlighter highlighter;
	Vector<MarkdownViewerCodeSpan> spans = highlighter.highlight_line("gdscript", "extends Node # comment");

	bool found_keyword = false;
	bool found_comment = false;
	for (const MarkdownViewerCodeSpan &span : spans) {
		if (span.type == MarkdownViewerCodeSpan::TYPE_KEYWORD && span.text == "extends") {
			found_keyword = true;
		}
		if (span.type == MarkdownViewerCodeSpan::TYPE_COMMENT && span.text == "# comment") {
			found_comment = true;
		}
	}
	CHECK(found_keyword);
	CHECK(found_comment);
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout computes content height and link hit records") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("# Title\n\nVisit [Godot](https://godotengine.org).");

	MarkdownViewerLayoutTheme theme;
	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(320, 240), theme);

	CHECK(layout.content_height > 0.0);
	bool found_link = false;
	for (const MarkdownViewerHitTest &hit : layout.hit_tests) {
		if (hit.type == MarkdownViewerHitTest::TYPE_LINK && hit.payload == "https://godotengine.org") {
			found_link = true;
			CHECK(hit.rect.size.x > 0.0);
			CHECK(hit.rect.size.y > 0.0);
		}
	}
	CHECK(found_link);
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout preserves inline spans and code syntax") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("Text with **bold**, *italic*, ~~gone~~, `code`, and [Godot](https://godotengine.org).");

	MarkdownViewerLayoutTheme theme;
	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(520, 240), theme);

	REQUIRE(layout.items.size() == 1);
	REQUIRE(layout.items[0].inline_lines.size() >= 1);

	bool found_strong = false;
	bool found_emphasis = false;
	bool found_strikethrough = false;
	bool found_code = false;
	bool found_link = false;
	for (const MarkdownViewerLayoutLine &line : layout.items[0].inline_lines) {
		for (const MarkdownViewerLayoutSpan &span : line.spans) {
			found_strong = found_strong || (span.style_flags & MarkdownViewerLayoutSpan::STYLE_STRONG);
			found_emphasis = found_emphasis || (span.style_flags & MarkdownViewerLayoutSpan::STYLE_EMPHASIS);
			found_strikethrough = found_strikethrough || (span.style_flags & MarkdownViewerLayoutSpan::STYLE_STRIKETHROUGH);
			found_code = found_code || (span.style_flags & MarkdownViewerLayoutSpan::STYLE_CODE);
			found_link = found_link || (span.style_flags & MarkdownViewerLayoutSpan::STYLE_LINK);
		}
	}

	CHECK(found_strong);
	CHECK(found_emphasis);
	CHECK(found_strikethrough);
	CHECK(found_code);
	CHECK(found_link);
	CHECK_FALSE(layout.hit_tests.is_empty());
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout gives wide tables scrollable content width") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("| Name | Description |\n| --- | --- |\n| Short | ThisIsAnIntentionallyVeryLongTableCellThatShouldNotBeClippedIntoTheVisibleColumn |\n");

	MarkdownViewerLayoutTheme theme;
	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(220, 120), theme);

	REQUIRE(layout.items.size() == 1);
	const MarkdownViewerLayoutItem &table = layout.items[0];
	REQUIRE(table.type == MarkdownViewerBlock::TYPE_TABLE);
	CHECK(table.scroll_viewport_width == doctest::Approx(220.0 - theme.document_margin * 2.0));
	CHECK(table.rect.size.x > table.scroll_viewport_width);
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout wraps wide glyph text inside item bounds") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");

	MarkdownViewerLayoutTheme theme;
	Ref<FontFile> test_font;
	test_font.instantiate();
	REQUIRE(test_font->load_dynamic_font("thirdparty/fonts/Inter_Regular.woff2") == OK);
	theme.font = test_font;
	Ref<Theme> default_theme = ThemeDB::get_singleton()->get_default_theme();
	if (default_theme.is_valid()) {
		theme.normal_font_size = default_theme->get_font_size(SceneStringName(font_size), SNAME("Label"));
	}
	if (theme.normal_font_size <= 0) {
		theme.normal_font_size = ThemeDB::get_singleton()->get_fallback_font_size();
	}
	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(420, 240), theme);

	REQUIRE(layout.items.size() == 1);
	REQUIRE(layout.items[0].inline_lines.size() > 1);

	const MarkdownViewerLayoutItem &item = layout.items[0];
	const real_t right_edge = item.rect.position.x + item.rect.size.x;
	for (const MarkdownViewerLayoutLine &line : item.inline_lines) {
		for (const MarkdownViewerLayoutSpan &span : line.spans) {
			CHECK(span.rect.position.x + span.rect.size.x <= doctest::Approx(right_edge).epsilon(0.01));
		}
	}
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout reserves real font height for wrapped lines") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("This paragraph is intentionally long enough to wrap across several visual lines when the viewer is narrow, so any per-line height underestimate accumulates in the final content height.");

	MarkdownViewerLayoutTheme theme;
	Ref<FontFile> test_font;
	test_font.instantiate();
	REQUIRE(test_font->load_dynamic_font("thirdparty/fonts/Inter_Regular.woff2") == OK);
	theme.font = test_font;
	theme.normal_font_size = 16;
	theme.line_spacing = 4;

	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(180, 240), theme);

	REQUIRE(layout.items.size() == 1);
	const MarkdownViewerLayoutItem &item = layout.items[0];
	REQUIRE(item.inline_lines.size() > 2);

	const real_t expected_line_height = theme.font->get_height(theme.normal_font_size) + theme.line_spacing;
	CHECK(item.rect.size.y >= doctest::Approx(item.inline_lines.size() * expected_line_height).epsilon(0.01));
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout creates nested list markers") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("- One\n  1. First\n  2. Second\n- Two");

	MarkdownViewerLayoutTheme theme;
	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(420, 240), theme);

	int unordered_markers = 0;
	bool found_ordered_marker = false;
	real_t outer_x = 0.0;
	real_t nested_x = 0.0;
	for (const MarkdownViewerLayoutItem &item : layout.items) {
		if (item.marker_unordered) {
			unordered_markers++;
			if (item.text == "One") {
				outer_x = item.rect.position.x;
			}
		}
		if (item.marker_text == "1.") {
			found_ordered_marker = true;
			nested_x = item.rect.position.x;
		}
	}

	CHECK(unordered_markers == 2);
	CHECK(found_ordered_marker);
	CHECK(nested_x > outer_x);
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout creates nested quote guides and thematic breaks") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("> Outer\n> > Inner\n\n---");

	MarkdownViewerLayoutTheme theme;
	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(420, 240), theme);

	int quote_guides = 0;
	bool nested_quote_indented = false;
	bool found_thematic_break = false;
	real_t outer_quote_x = -1.0;
	for (const MarkdownViewerLayoutItem &item : layout.items) {
		if (item.type == MarkdownViewerBlock::TYPE_BLOCK_QUOTE) {
			quote_guides++;
			if (outer_quote_x < 0.0) {
				outer_quote_x = item.rect.position.x;
			} else if (item.rect.position.x > outer_quote_x) {
				nested_quote_indented = true;
			}
		}
		if (item.type == MarkdownViewerBlock::TYPE_THEMATIC_BREAK) {
			found_thematic_break = true;
		}
	}

	CHECK(quote_guides >= 2);
	CHECK(nested_quote_indented);
	CHECK(found_thematic_break);
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout creates code copy hit records") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("```gdscript\nextends Node\n```");

	MarkdownViewerLayoutTheme theme;
	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(320, 240), theme);

	REQUIRE(layout.hit_tests.size() > 0);
	CHECK(layout.hit_tests[0].type == MarkdownViewerHitTest::TYPE_CODE_COPY);
	CHECK(layout.hit_tests[0].payload == "extends Node\n");
	CHECK(layout.hit_tests[0].secondary_payload == "gdscript");
}

TEST_CASE("[SceneTree][MarkdownViewer] Layout omits code copy hit records when disabled") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("```gdscript\nextends Node\n```");

	MarkdownViewerLayoutTheme theme;
	theme.code_copy_enabled = false;
	MarkdownViewerLayoutBuilder layout_builder;
	MarkdownViewerLayout layout = layout_builder.build(document, Size2(320, 240), theme);

	for (const MarkdownViewerHitTest &hit : layout.hit_tests) {
		CHECK(hit.type != MarkdownViewerHitTest::TYPE_CODE_COPY);
	}
}

TEST_CASE("[SceneTree][MarkdownViewer] Draw helper selects visible items") {
	MarkdownViewerLayout layout;

	MarkdownViewerLayoutItem first;
	first.rect = Rect2(0, 0, 100, 20);
	layout.items.push_back(first);

	MarkdownViewerLayoutItem second;
	second.rect = Rect2(0, 500, 100, 20);
	layout.items.push_back(second);

	Vector<int> visible = MarkdownViewerDrawHelper::collect_visible_item_indices_for_test(layout, Rect2(0, 0, 320, 240), 0.0);

	REQUIRE(visible.size() == 1);
	CHECK(visible[0] == 0);
}

TEST_CASE("[SceneTree][MarkdownViewer] Control updates content height after layout") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 240));
	viewer->set_markdown("# Title\n\nBody");
	viewer->force_layout_for_test();

	CHECK(viewer->get_content_height() > 0.0);

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Select all and copy uses rendered plain text") {
	DisplayServerMock *DS = static_cast<DisplayServerMock *>(DisplayServer::get_singleton());
	REQUIRE(DS != nullptr);
	DS->clipboard_set("");

	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(420, 240));
	viewer->set_markdown("# Title\n\nBody with **bold** and `code`.");
	root->add_child(viewer);
	viewer->force_layout_for_test();
	viewer->select_all();
	viewer->grab_focus();

	SEND_GUI_KEY_EVENT(Key::C | KeyModifierMask::CMD_OR_CTRL);

	CHECK(viewer->get_selected_text() == "Title\nBody with bold and code.");
	CHECK(DS->clipboard_get() == "Title\nBody with bold and code.");

	root->remove_child(viewer);
	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Mouse drag selection can be copied") {
	DisplayServerMock *DS = static_cast<DisplayServerMock *>(DisplayServer::get_singleton());
	REQUIRE(DS != nullptr);
	DS->clipboard_set("");

	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(420, 120));
	viewer->set_markdown("Alpha beta gamma");
	root->add_child(viewer);
	viewer->force_layout_for_test();

	const Point2 selection_start = viewer->get_text_caret_position_for_test(0);
	const Point2 selection_end = viewer->get_text_caret_position_for_test(5);
	SEND_GUI_MOUSE_BUTTON_EVENT(selection_start, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
	SEND_GUI_MOUSE_MOTION_EVENT(selection_end, MouseButtonMask::LEFT, Key::NONE);
	SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(selection_end, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);

	viewer->grab_focus();
	SEND_GUI_KEY_EVENT(Key::C | KeyModifierMask::CMD_OR_CTRL);

	CHECK(viewer->get_selected_text() == "Alpha");
	CHECK(DS->clipboard_get() == "Alpha");

	root->remove_child(viewer);
	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Link clicks are not emitted while selecting link text") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(420, 120));
	viewer->set_markdown("[Godot](https://godotengine.org) docs");
	root->add_child(viewer);
	viewer->force_layout_for_test();

	SIGNAL_WATCH(viewer, "link_clicked");

	const Point2 click_start = viewer->get_text_caret_position_for_test(0);
	SEND_GUI_MOUSE_BUTTON_EVENT(click_start, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
	SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(click_start, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
	Array signal_args = { { "https://godotengine.org" } };
	SIGNAL_CHECK("link_clicked", signal_args);

	const Point2 selection_start = viewer->get_text_caret_position_for_test(0);
	const Point2 selection_end = viewer->get_text_caret_position_for_test(5);
	SEND_GUI_MOUSE_BUTTON_EVENT(selection_start, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
	SEND_GUI_MOUSE_MOTION_EVENT(selection_end, MouseButtonMask::LEFT, Key::NONE);
	SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(selection_end, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);

	SIGNAL_CHECK_FALSE("link_clicked");
	CHECK(viewer->get_selected_text() == "Godot");

	SIGNAL_UNWATCH(viewer, "link_clicked");
	root->remove_child(viewer);
	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Content height query reuses current-width layout cache") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(220, 120));
	viewer->set_markdown("This paragraph is long enough to wrap when measured at the current viewer width, so the height query should populate the same layout cache used for drawing.");

	const real_t queried_height = viewer->get_content_height_for_width(220);

	CHECK(queried_height > 0.0);
	CHECK(viewer->get_content_height() == doctest::Approx(queried_height));

	viewer->force_layout_for_test();
	CHECK(viewer->get_content_height() == doctest::Approx(queried_height));

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Content height query caches repeated off-width layout") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 120));
	viewer->set_async_parsing_enabled(false);
	viewer->set_markdown("This paragraph is intentionally long enough to wrap differently when measured at a narrow width, so repeated minimum-size queries should not rebuild the Markdown layout every time a parent container asks for size.");

	const int initial_build_count = viewer->get_layout_build_count_for_test();
	const real_t first_height = viewer->get_content_height_for_width(180);
	const int after_first_query_count = viewer->get_layout_build_count_for_test();
	const real_t second_height = viewer->get_content_height_for_width(180);

	CHECK(first_height > 0.0);
	CHECK(second_height == doctest::Approx(first_height));
	CHECK(after_first_query_count > initial_build_count);
	CHECK(viewer->get_layout_build_count_for_test() == after_first_query_count);

	viewer->set_size(Size2(180, 120));
	viewer->force_layout_for_test();
	CHECK(viewer->get_layout_build_count_for_test() == after_first_query_count);
	CHECK(viewer->get_content_height() == doctest::Approx(first_height));

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Default async parse threshold offloads medium markdown") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 120));
	viewer->set_async_parsing_enabled(true);

	String markdown;
	for (int i = 0; i < 26; i++) {
		markdown += "This paragraph is long enough to represent a streaming assistant response with Markdown structure.\n\n";
	}
	REQUIRE(markdown.length() > 1024);
	REQUIRE(markdown.length() < 4096);

	viewer->set_markdown(markdown);
	CHECK(viewer->is_async_parse_pending_for_test());

	for (int i = 0; i < 200 && viewer->is_async_parse_pending_for_test(); i++) {
		if (OS::get_singleton()) {
			OS::get_singleton()->delay_usec(1000);
		}
		if (MessageQueue::get_singleton()) {
			MessageQueue::get_singleton()->flush();
		}
	}
	if (MessageQueue::get_singleton()) {
		MessageQueue::get_singleton()->flush();
	}

	CHECK_FALSE(viewer->is_async_parse_pending_for_test());

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Async parse applies only the latest markdown") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 120));
	viewer->set_async_parsing_enabled(true);
	viewer->set_async_parse_minimum_length_for_test(1);

	viewer->set_markdown("First async document that should be superseded.");
	viewer->set_markdown("# Latest\n\nThis document should win after async parsing settles.");

	for (int i = 0; i < 200 && viewer->is_async_parse_pending_for_test(); i++) {
		if (OS::get_singleton()) {
			OS::get_singleton()->delay_usec(1000);
		}
		if (MessageQueue::get_singleton()) {
			MessageQueue::get_singleton()->flush();
		}
	}
	if (MessageQueue::get_singleton()) {
		MessageQueue::get_singleton()->flush();
	}

	CHECK_FALSE(viewer->is_async_parse_pending_for_test());
	CHECK(viewer->get_markdown() == "# Latest\n\nThis document should win after async parsing settles.");

	viewer->force_layout_for_test();
	CHECK(viewer->get_content_height() > 0.0);

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Hit test resolves links with scroll offset") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 120));
	viewer->set_markdown("[Godot](https://godotengine.org)");
	viewer->force_layout_for_test();

	MarkdownViewerHitTest hit;
	bool found = viewer->get_hit_test_at_for_test(Point2(16, 16), hit);

	CHECK(found);
	CHECK(hit.type == MarkdownViewerHitTest::TYPE_LINK);
	CHECK(hit.payload == "https://godotengine.org");

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Scroll offset is clamped to content bounds") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(160, 40));
	viewer->set_markdown("# A\n\nB\n\nC\n\nD\n\nE\n\nF");
	viewer->force_layout_for_test();

	viewer->set_scroll_offset_for_test(100000.0);
	CHECK(viewer->get_scroll_offset_for_test() <= MAX(0.0, viewer->get_content_height() - viewer->get_size().y));

	viewer->set_scroll_offset_for_test(-100.0);
	CHECK(viewer->get_scroll_offset_for_test() == doctest::Approx(0.0));

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Table horizontal scroll offset is clamped to table overflow") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(220, 120));
	viewer->set_markdown("| Name | Description |\n| --- | --- |\n| Short | ThisIsAnIntentionallyVeryLongTableCellThatShouldNotBeClippedIntoTheVisibleColumn |\n");
	viewer->force_layout_for_test();

	viewer->set_table_horizontal_scroll_offset_for_test(100000.0);
	CHECK(viewer->get_table_horizontal_scroll_offset_for_test() > 0.0);
	CHECK(viewer->get_table_horizontal_scroll_offset_for_test() <= viewer->get_max_table_horizontal_scroll_offset_for_test());

	viewer->set_table_horizontal_scroll_offset_for_test(-100.0);
	CHECK(viewer->get_table_horizontal_scroll_offset_for_test() == doctest::Approx(0.0));

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Scroll disabled exposes content height through minimum size") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 120));
	viewer->set_scroll_enabled(false);
	viewer->set_markdown("# Title\n\nBody");
	viewer->force_layout_for_test();

	CHECK(viewer->get_minimum_size().y == doctest::Approx(viewer->get_content_height()));

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Scroll disabled reports minimum size after deferred wrap layout") {
	Window *root = SceneTree::get_singleton()->get_root();
	REQUIRE(root != nullptr);

	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(420, 120));
	viewer->set_scroll_enabled(false);
	viewer->set_markdown("Short");
	root->add_child(viewer);
	SceneTree::get_singleton()->process(0.0);
	viewer->force_layout_for_test();
	const real_t initial_height = viewer->get_minimum_size().y;
	SceneTree::get_singleton()->process(0.0);

	SIGNAL_WATCH(viewer, SceneStringName(minimum_size_changed));
	viewer->set_block_minimum_size_adjust(true);
	viewer->set_size(Size2(180, 120));
	viewer->set_markdown("This paragraph intentionally contains enough words to wrap across many more lines after the viewer becomes narrow, matching a MarkdownViewer layout pass that discovers a taller content height after the first minimum-size propagation.");
	viewer->set_block_minimum_size_adjust(false);
	viewer->force_layout_for_test();
	SceneTree::get_singleton()->process(0.0);

	Array signal_args = { {} };
	SIGNAL_CHECK(SceneStringName(minimum_size_changed), signal_args);
	CHECK(viewer->get_minimum_size().y > initial_height);

	SIGNAL_UNWATCH(viewer, SceneStringName(minimum_size_changed));
	root->remove_child(viewer);
	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Image max dimensions are settable") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);

	viewer->set_image_max_width(640);
	viewer->set_image_max_height(480);

	CHECK(viewer->get_image_max_width() == doctest::Approx(640));
	CHECK(viewer->get_image_max_height() == doctest::Approx(480));

	memdelete(viewer);
}

} // namespace TestMarkdownViewer
