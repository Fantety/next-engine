# MarkdownViewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a runtime `MarkdownViewer` Godot UI node that renders Markdown through a self-drawn `Control`, with tables, code blocks, links, local/remote images, and an extensible internal architecture. AI component migration is deliberately deferred until the runtime node has been confirmed inside the engine.

**Architecture:** `MarkdownViewer` is the public node and orchestration layer. Markdown source is converted into lightweight document blocks, laid out into retained geometry and hit-test records, then drawn directly through CanvasItem APIs. Parsing, layout, drawing, image loading, and code highlighting are split into focused helper files so future Markdown features can be added without growing the main node into a monolith.

**Tech Stack:** Godot C++ runtime scene UI, `Control`, `MarkdownParser`/`MarkdownNode`, `TextParagraph`/`TextLine`, `HTTPRequest`, `Image`, `ImageTexture`, doctest scene tests, SCons.

---

## Source Map

Create:

- `scene/gui/markdown_viewer.h`: public `MarkdownViewer : Control` API, properties, signals, dirty flags, input dispatch.
- `scene/gui/markdown_viewer.cpp`: binding, notification handling, public setters/getters, orchestration.
- `scene/gui/markdown_viewer_document.h`: internal document block/run structs and document builder declarations.
- `scene/gui/markdown_viewer_document.cpp`: source-to-document conversion using `MarkdownParser` plus pipe table detection.
- `scene/gui/markdown_viewer_layout.h`: layout item, hit-test, theme metric structs, layout builder declarations.
- `scene/gui/markdown_viewer_layout.cpp`: width-dependent layout using `TextParagraph`/`TextLine`.
- `scene/gui/markdown_viewer_draw.h`: draw helper declarations.
- `scene/gui/markdown_viewer_draw.cpp`: CanvasItem drawing for retained layout items.
- `scene/gui/markdown_viewer_image_loader.h`: image status/cache/request API.
- `scene/gui/markdown_viewer_image_loader.cpp`: local decode and remote `HTTPRequest` integration.
- `scene/gui/markdown_viewer_code_highlighter.h`: code token/span structs and highlighter API.
- `scene/gui/markdown_viewer_code_highlighter.cpp`: lightweight generic/GDScript/JSON/C++ tokenization.
- `tests/scene/test_markdown_viewer.cpp`: scene tests for public node and internal helpers.

Modify:

- `scene/register_scene_types.cpp`: include `scene/gui/markdown_viewer.h` and `GDREGISTER_CLASS(MarkdownViewer)` in the GUI registration section near `TextureRect`, `CodeEdit`, and `RichTextLabel`.

No `scene/gui/SCsub` edit is expected because it already adds `*.cpp` in `scene/gui`.

Deferred until user approval after engine validation:

- `editor/ai_component/ui/ai_markdown_label.h`
- `editor/ai_component/ui/ai_markdown_label.cpp`
- `editor/ai_component/ui/ai_message_bubble.h`
- `editor/ai_component/ui/ai_message_bubble.cpp`
- `tests/editor/test_ai_model_settings.cpp`

## Build And Test Policy

Do not execute builds or engine test binaries in the implementation session. The user will run `scons` and `bin\next.windows.editor.x86_64.console.exe` locally when they want to validate. Implementation workers should still write tests first and provide the exact commands the user can run, but must not run those commands themselves.

Any command block below that contains `scons` or `bin\next.windows.editor.x86_64.console.exe` is a user-side validation command. At red/green checkpoints, stop and ask the user to run the listed command if validation is needed before proceeding.

Use static checks that do not build when useful, such as:

```powershell
git diff --check
```

User-side focused test command after they build:

Use the existing Windows editor test binary after each compile:

```powershell
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Useful broader checks:

```powershell
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

User-side build command:

```powershell
scons platform=windows target=editor tests=yes
```

User-side final validation:

```powershell
git diff --check
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

---

### Task 1: Add Failing Registration And Default API Tests

**Files:**

- Create: `tests/scene/test_markdown_viewer.cpp`
- Create later in green step: `scene/gui/markdown_viewer.h`
- Create later in green step: `scene/gui/markdown_viewer.cpp`
- Modify later in green step: `scene/register_scene_types.cpp`

- [ ] **Step 1: Write the failing test**

Add `tests/scene/test_markdown_viewer.cpp`:

```cpp
/**************************************************************************/
/*  test_markdown_viewer.cpp                                              */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_markdown_viewer)

#include "core/object/class_db.h"
#include "scene/gui/markdown_viewer.h"

namespace TestMarkdownViewer {

TEST_CASE("[SceneTree][MarkdownViewer] Runtime class is registered and defaults are conservative") {
	CHECK(ClassDB::class_exists("MarkdownViewer"));

	MarkdownViewer *viewer = memnew(MarkdownViewer);

	CHECK(viewer->get_markdown().is_empty());
	CHECK_FALSE(viewer->is_remote_images_enabled());
	CHECK_FALSE(viewer->is_open_links_enabled());
	CHECK(viewer->is_scroll_enabled());
	CHECK(viewer->is_syntax_highlighting_enabled());
	CHECK(viewer->is_code_copy_enabled());
	CHECK(viewer->get_content_height() == doctest::Approx(0.0));

	memdelete(viewer);
}

TEST_CASE("[SceneTree][MarkdownViewer] Markdown property stores source text") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);

	viewer->set_markdown("# Title\n\nBody");

	CHECK(viewer->get_markdown() == "# Title\n\nBody");

	memdelete(viewer);
}

} // namespace TestMarkdownViewer
```

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: compile fails because `scene/gui/markdown_viewer.h` does not exist.

- [ ] **Step 3: Write minimal implementation**

Create `scene/gui/markdown_viewer.h`:

```cpp
/**************************************************************************/
/*  markdown_viewer.h                                                     */
/**************************************************************************/

#pragma once

#include "scene/gui/control.h"

class MarkdownViewer : public Control {
	GDCLASS(MarkdownViewer, Control);

	String markdown;
	bool remote_images_enabled = false;
	bool open_links_enabled = false;
	bool scroll_enabled = true;
	bool syntax_highlighting_enabled = true;
	bool code_copy_enabled = true;
	real_t content_height = 0.0;

protected:
	static void _bind_methods();

public:
	void set_markdown(const String &p_markdown);
	String get_markdown() const;

	void set_remote_images_enabled(bool p_enabled);
	bool is_remote_images_enabled() const;

	void set_open_links_enabled(bool p_enabled);
	bool is_open_links_enabled() const;

	void set_scroll_enabled(bool p_enabled);
	bool is_scroll_enabled() const;

	void set_syntax_highlighting_enabled(bool p_enabled);
	bool is_syntax_highlighting_enabled() const;

	void set_code_copy_enabled(bool p_enabled);
	bool is_code_copy_enabled() const;

	real_t get_content_height() const;

	MarkdownViewer();
};
```

Create `scene/gui/markdown_viewer.cpp`:

```cpp
/**************************************************************************/
/*  markdown_viewer.cpp                                                   */
/**************************************************************************/

#include "markdown_viewer.h"

void MarkdownViewer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_markdown", "markdown"), &MarkdownViewer::set_markdown);
	ClassDB::bind_method(D_METHOD("get_markdown"), &MarkdownViewer::get_markdown);
	ClassDB::bind_method(D_METHOD("set_remote_images_enabled", "enabled"), &MarkdownViewer::set_remote_images_enabled);
	ClassDB::bind_method(D_METHOD("is_remote_images_enabled"), &MarkdownViewer::is_remote_images_enabled);
	ClassDB::bind_method(D_METHOD("set_open_links_enabled", "enabled"), &MarkdownViewer::set_open_links_enabled);
	ClassDB::bind_method(D_METHOD("is_open_links_enabled"), &MarkdownViewer::is_open_links_enabled);
	ClassDB::bind_method(D_METHOD("set_scroll_enabled", "enabled"), &MarkdownViewer::set_scroll_enabled);
	ClassDB::bind_method(D_METHOD("is_scroll_enabled"), &MarkdownViewer::is_scroll_enabled);
	ClassDB::bind_method(D_METHOD("set_syntax_highlighting_enabled", "enabled"), &MarkdownViewer::set_syntax_highlighting_enabled);
	ClassDB::bind_method(D_METHOD("is_syntax_highlighting_enabled"), &MarkdownViewer::is_syntax_highlighting_enabled);
	ClassDB::bind_method(D_METHOD("set_code_copy_enabled", "enabled"), &MarkdownViewer::set_code_copy_enabled);
	ClassDB::bind_method(D_METHOD("is_code_copy_enabled"), &MarkdownViewer::is_code_copy_enabled);
	ClassDB::bind_method(D_METHOD("get_content_height"), &MarkdownViewer::get_content_height);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "markdown", PROPERTY_HINT_MULTILINE_TEXT), "set_markdown", "get_markdown");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "remote_images_enabled"), "set_remote_images_enabled", "is_remote_images_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "open_links_enabled"), "set_open_links_enabled", "is_open_links_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_enabled"), "set_scroll_enabled", "is_scroll_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "syntax_highlighting_enabled"), "set_syntax_highlighting_enabled", "is_syntax_highlighting_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "code_copy_enabled"), "set_code_copy_enabled", "is_code_copy_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "content_height", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_content_height");

	ADD_SIGNAL(MethodInfo("link_clicked", PropertyInfo(Variant::STRING, "url")));
	ADD_SIGNAL(MethodInfo("image_loaded", PropertyInfo(Variant::STRING, "source")));
	ADD_SIGNAL(MethodInfo("image_failed", PropertyInfo(Variant::STRING, "source"), PropertyInfo(Variant::STRING, "error")));
	ADD_SIGNAL(MethodInfo("code_block_copied", PropertyInfo(Variant::STRING, "language"), PropertyInfo(Variant::STRING, "code")));
}

void MarkdownViewer::set_markdown(const String &p_markdown) {
	if (markdown == p_markdown) {
		return;
	}
	markdown = p_markdown;
	queue_redraw();
}

String MarkdownViewer::get_markdown() const {
	return markdown;
}

void MarkdownViewer::set_remote_images_enabled(bool p_enabled) {
	remote_images_enabled = p_enabled;
}

bool MarkdownViewer::is_remote_images_enabled() const {
	return remote_images_enabled;
}

void MarkdownViewer::set_open_links_enabled(bool p_enabled) {
	open_links_enabled = p_enabled;
}

bool MarkdownViewer::is_open_links_enabled() const {
	return open_links_enabled;
}

void MarkdownViewer::set_scroll_enabled(bool p_enabled) {
	scroll_enabled = p_enabled;
}

bool MarkdownViewer::is_scroll_enabled() const {
	return scroll_enabled;
}

void MarkdownViewer::set_syntax_highlighting_enabled(bool p_enabled) {
	syntax_highlighting_enabled = p_enabled;
}

bool MarkdownViewer::is_syntax_highlighting_enabled() const {
	return syntax_highlighting_enabled;
}

void MarkdownViewer::set_code_copy_enabled(bool p_enabled) {
	code_copy_enabled = p_enabled;
}

bool MarkdownViewer::is_code_copy_enabled() const {
	return code_copy_enabled;
}

real_t MarkdownViewer::get_content_height() const {
	return content_height;
}

MarkdownViewer::MarkdownViewer() {
	set_mouse_filter(MOUSE_FILTER_STOP);
}
```

Modify `scene/register_scene_types.cpp`:

```cpp
#include "scene/gui/markdown_viewer.h"
```

Add near other GUI controls:

```cpp
GDREGISTER_CLASS(MarkdownViewer);
```

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: both tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer.h scene/gui/markdown_viewer.cpp scene/register_scene_types.cpp
git commit -m "feat: register markdown viewer control"
```

---

### Task 2: Add Document Model And Markdown Conversion

**Files:**

- Modify: `tests/scene/test_markdown_viewer.cpp`
- Create: `scene/gui/markdown_viewer_document.h`
- Create: `scene/gui/markdown_viewer_document.cpp`
- Modify: `scene/gui/markdown_viewer.h`
- Modify: `scene/gui/markdown_viewer.cpp`

- [ ] **Step 1: Write the failing tests**

Append tests for the internal document builder:

```cpp
#include "scene/gui/markdown_viewer_document.h"

TEST_CASE("[SceneTree][MarkdownViewer] Document builder preserves common Markdown blocks") {
	MarkdownViewerDocumentBuilder builder;
	MarkdownViewerDocument document = builder.build("# Title\n\nBody with [Godot](https://godotengine.org) and `code`.\n\n- One\n- Two\n\n```gdscript\nextends Node\n```\n\n![Alt](res://icon.svg)");

	CHECK(document.blocks.size() == 5);
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
	CHECK(document.blocks[1].table_rows.size() == 3);
	CHECK(document.blocks[1].table_rows[0].cells[0].plain_text == "Name");
	CHECK(document.blocks[1].table_rows[1].cells[1].plain_text == "100");
	CHECK(document.blocks[2].type == MarkdownViewerBlock::TYPE_PARAGRAPH);
}
```

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
```

Expected: compile fails because `markdown_viewer_document.h` does not exist.

- [ ] **Step 3: Implement document structs and builder**

Create `scene/gui/markdown_viewer_document.h` with focused data types:

```cpp
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
	void clear() { blocks.clear(); }
};

class MarkdownViewerDocumentBuilder {
	String _flatten_inlines(const Vector<MarkdownViewerInline> &p_inlines) const;
	void _append_markdown_range(const String &p_markdown, MarkdownViewerDocument &r_document);
	bool _try_parse_pipe_table(const PackedStringArray &p_lines, int p_start, MarkdownViewerBlock &r_block, int &r_end);

public:
	MarkdownViewerDocument build(const String &p_markdown);
};
```

Implement `markdown_viewer_document.cpp`:

- Use `MarkdownParser parser; Ref<MarkdownNode> root = parser.parse_markdown(range);`.
- Convert node types to blocks/inlines with small private helper functions in an anonymous namespace.
- For paragraph with one image inline and no other text, emit `TYPE_IMAGE` block.
- For pipe tables, scan lines for header line plus separator line. Accept cells split by `|`, trim edges, and parse each cell through inline Markdown by using a small builder helper on the cell text.
- Preserve code fence info as `language` and literal as `plain_text`.
- Keep list items as children of a `TYPE_LIST` block.

Update `MarkdownViewer` to hold:

```cpp
MarkdownViewerDocument document;
bool parse_dirty = true;
void _ensure_document();
```

Call `_ensure_document()` from new debug/test helper methods only if needed. Keep these helpers private unless tests require them. Prefer testing `MarkdownViewerDocumentBuilder` directly for document conversion.

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: document builder tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer_document.h scene/gui/markdown_viewer_document.cpp scene/gui/markdown_viewer.h scene/gui/markdown_viewer.cpp
git commit -m "feat: parse markdown viewer document blocks"
```

---

### Task 3: Add Image Loader Status And Local Decode

**Files:**

- Modify: `tests/scene/test_markdown_viewer.cpp`
- Create: `scene/gui/markdown_viewer_image_loader.h`
- Create: `scene/gui/markdown_viewer_image_loader.cpp`

- [ ] **Step 1: Write failing tests**

Add tests focused on deterministic helper behavior:

```cpp
#include "scene/gui/markdown_viewer_image_loader.h"

TEST_CASE("[SceneTree][MarkdownViewer] Image loader blocks remote URLs by default") {
	MarkdownViewerImageLoader loader;
	loader.set_remote_images_enabled(false);

	MarkdownViewerImageRequestResult result = loader.ensure_image("https://example.com/image.png");

	CHECK(result.status == MarkdownViewerImageStatus::STATUS_REMOTE_DISABLED);
	CHECK_FALSE(loader.has_pending_requests());
}

TEST_CASE("[SceneTree][MarkdownViewer] Image loader decodes PNG buffers") {
	Ref<Image> image = memnew(Image(2, 3, false, Image::FORMAT_RGBA8));
	image->fill(Color(1, 0, 0, 1));
	Vector<uint8_t> png = image->save_png_to_buffer();

	MarkdownViewerImageLoader loader;
	MarkdownViewerImageRequestResult result = loader.decode_buffer_for_test("memory.png", png);

	CHECK(result.status == MarkdownViewerImageStatus::STATUS_LOADED);
	REQUIRE(result.texture.is_valid());
	CHECK(result.size == Size2(2, 3));
}
```

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
```

Expected: compile fails because image loader header does not exist.

- [ ] **Step 3: Implement image loader helper**

Create `markdown_viewer_image_loader.h`:

- `enum class MarkdownViewerImageStatus { STATUS_EMPTY, STATUS_LOADING, STATUS_LOADED, STATUS_FAILED, STATUS_REMOTE_DISABLED };`
- `struct MarkdownViewerImageRequestResult { MarkdownViewerImageStatus status; Ref<Texture2D> texture; Size2 size; String error; };`
- `class MarkdownViewerImageLoader : public Node` if it owns `HTTPRequest`, otherwise `RefCounted` with an attach method. Prefer `Node` for the remote phase.
- Methods:
  - `set_remote_images_enabled(bool)`
  - `is_remote_images_enabled()`
  - `ensure_image(const String &p_source)`
  - `has_pending_requests()`
  - `decode_buffer_for_test(const String &p_source, const Vector<uint8_t> &p_data)`

Implement local deterministic decoding:

- Try `Image::load_png_from_buffer`.
- If it fails, try `load_jpg_from_buffer`.
- If it fails, try `load_webp_from_buffer`.
- Convert loaded image with `ImageTexture::create_from_image`.
- Return failed status with a short error if all decoders fail.
- Do not start network requests when remote images are disabled.

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: image loader tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer_image_loader.h scene/gui/markdown_viewer_image_loader.cpp
git commit -m "feat: add markdown viewer image loader"
```

---

### Task 4: Add Layout Model And Content Height

**Files:**

- Modify: `tests/scene/test_markdown_viewer.cpp`
- Create: `scene/gui/markdown_viewer_layout.h`
- Create: `scene/gui/markdown_viewer_layout.cpp`
- Modify: `scene/gui/markdown_viewer.h`
- Modify: `scene/gui/markdown_viewer.cpp`

- [ ] **Step 1: Write failing tests**

Add:

```cpp
#include "scene/gui/markdown_viewer_layout.h"

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

TEST_CASE("[SceneTree][MarkdownViewer] Control updates content height after layout") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 240));
	viewer->set_markdown("# Title\n\nBody");
	viewer->force_layout_for_test();

	CHECK(viewer->get_content_height() > 0.0);

	memdelete(viewer);
}
```

Add `force_layout_for_test()` only under a normal public method if acceptable, or a protected test friend pattern if the project already has one. If adding a public method, mark it internal in the header comment and do not bind it to scripting.

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
```

Expected: compile fails due to missing layout helper and `force_layout_for_test`.

- [ ] **Step 3: Implement minimal layout**

Create `markdown_viewer_layout.h`:

- `MarkdownViewerLayoutTheme` with `Ref<Font> font`, `Ref<Font> bold_font`, `Ref<Font> italic_font`, `Ref<Font> mono_font`, font sizes, margins, spacings, colors.
- `MarkdownViewerHitTest` with `TYPE_LINK`, `TYPE_CODE_COPY`, `TYPE_IMAGE`, `Rect2 rect`, `String payload`, `String secondary_payload`.
- `MarkdownViewerLayoutItem` with block type, `Rect2 rect`, optional `Ref<TextParagraph> paragraph`, text/color metadata.
- `MarkdownViewerLayout` with `Vector<MarkdownViewerLayoutItem> items`, `Vector<MarkdownViewerHitTest> hit_tests`, `real_t content_height`.
- `MarkdownViewerLayoutBuilder::build(document, viewport_size, theme)`.

Implement minimal layout:

- Use `TextParagraph` for paragraph/heading/list cell text where possible.
- Provide fallback to `ThemeDB::get_singleton()->get_default_theme()->get_font(...)` through the viewer when building the theme. If direct default lookup is awkward, use `Control::get_theme_font(SNAME("font"))` from `MarkdownViewer` and pass refs into layout theme.
- Apply margins and block spacing.
- For links, create approximate hit rects from the paragraph line bounds for first version; refine in later task if needed.
- For code blocks, reserve panel height from line count.
- For table blocks, reserve row heights and cell rects.
- For image blocks, reserve placeholder size when no texture is loaded.

Update `MarkdownViewer`:

- Dirty flags: `parse_dirty`, `layout_dirty`.
- `_ensure_document()`, `_ensure_layout()`, `_make_layout_theme()`.
- `force_layout_for_test()` calls `_ensure_layout()`.
- `set_markdown()` marks parse/layout dirty.
- `NOTIFICATION_RESIZED` marks layout dirty.
- `get_minimum_size()` returns `Size2(0, content_height)` when `scroll_enabled` is false, otherwise a sane default.

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: layout tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer_layout.h scene/gui/markdown_viewer_layout.cpp scene/gui/markdown_viewer.h scene/gui/markdown_viewer.cpp
git commit -m "feat: layout markdown viewer content"
```

---

### Task 5: Add Code Highlighter And Copy Hit Data

**Files:**

- Modify: `tests/scene/test_markdown_viewer.cpp`
- Create: `scene/gui/markdown_viewer_code_highlighter.h`
- Create: `scene/gui/markdown_viewer_code_highlighter.cpp`
- Modify: `scene/gui/markdown_viewer_layout.h`
- Modify: `scene/gui/markdown_viewer_layout.cpp`
- Modify: `scene/gui/markdown_viewer.cpp`

- [ ] **Step 1: Write failing tests**

Add:

```cpp
#include "scene/gui/markdown_viewer_code_highlighter.h"

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
```

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
```

Expected: compile fails because code highlighter header does not exist or copy hit is missing.

- [ ] **Step 3: Implement minimal highlighter and copy data**

Create `markdown_viewer_code_highlighter.h/.cpp`:

- `MarkdownViewerCodeSpan::Type`: normal, keyword, string, comment, number.
- `highlight_line(language, line)` returns spans.
- Implement:
  - GDScript: `extends`, `class_name`, `func`, `var`, `const`, `if`, `elif`, `else`, `for`, `while`, `return`, `match`, `await`, `signal`, `true`, `false`, `null`; `#` comments; quoted strings; numbers.
  - JSON: strings, numbers, booleans, null.
  - C/C++: `//` comments, quoted strings, a compact keyword set.
  - fallback: one normal span.

Update layout:

- Add `TYPE_CODE_COPY` hit record for every code block when copy is enabled in theme/options.
- Store raw code in `payload`, language in `secondary_payload`.

Update viewer input:

- On left click over `TYPE_CODE_COPY`, call `DisplayServer::get_singleton()->clipboard_set(payload)` when available and emit `code_block_copied`.

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: highlighter and copy hit tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer_code_highlighter.h scene/gui/markdown_viewer_code_highlighter.cpp scene/gui/markdown_viewer_layout.h scene/gui/markdown_viewer_layout.cpp scene/gui/markdown_viewer.cpp
git commit -m "feat: add markdown code highlighting metadata"
```

---

### Task 6: Add Direct Drawing

**Files:**

- Modify: `tests/scene/test_markdown_viewer.cpp`
- Create: `scene/gui/markdown_viewer_draw.h`
- Create: `scene/gui/markdown_viewer_draw.cpp`
- Modify: `scene/gui/markdown_viewer.cpp`
- Modify: `scene/gui/markdown_viewer_layout.h`
- Modify: `scene/gui/markdown_viewer_layout.cpp`

- [ ] **Step 1: Write failing tests**

Add a deterministic non-visual test to ensure draw culling can be reasoned about without screenshot infrastructure:

```cpp
#include "scene/gui/markdown_viewer_draw.h"

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
```

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
```

Expected: compile fails because draw helper does not exist.

- [ ] **Step 3: Implement draw helper**

Create `markdown_viewer_draw.h/.cpp`:

- `MarkdownViewerDrawHelper::draw(MarkdownViewer &, const MarkdownViewerLayout &, real_t scroll_offset)`.
- `collect_visible_item_indices_for_test(layout, viewport_rect, scroll_offset)` returns item indices whose rect intersects viewport after scroll offset.

Drawing implementation:

- Clip through the control's canvas item clip behavior or by culling items.
- Draw paragraph/heading/list `TextParagraph` instances using their `draw()` methods.
- Draw code block panel background, header, language label, copy button, and text lines.
- Draw tables with header background, row alternation, borders, and cell paragraphs.
- Draw image placeholder/texture rects.
- Draw link underlines from hit rects.
- Draw simple scrollbar when content height exceeds viewport and `scroll_enabled` is true.

Update `MarkdownViewer::_notification(NOTIFICATION_DRAW)`:

- `_ensure_layout()`.
- Call draw helper with current scroll offset.

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: draw helper tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer_draw.h scene/gui/markdown_viewer_draw.cpp scene/gui/markdown_viewer.cpp scene/gui/markdown_viewer_layout.h scene/gui/markdown_viewer_layout.cpp
git commit -m "feat: draw markdown viewer layout"
```

---

### Task 7: Add Scrolling, Link Clicks, And Cursor Hit Testing

**Files:**

- Modify: `tests/scene/test_markdown_viewer.cpp`
- Modify: `scene/gui/markdown_viewer.h`
- Modify: `scene/gui/markdown_viewer.cpp`
- Modify: `scene/gui/markdown_viewer_layout.h`
- Modify: `scene/gui/markdown_viewer_layout.cpp`

- [ ] **Step 1: Write failing tests**

Add tests around pure hit resolution:

```cpp
TEST_CASE("[SceneTree][MarkdownViewer] Hit test resolves links with scroll offset") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 120));
	viewer->set_markdown("[Godot](https://godotengine.org)");
	viewer->force_layout_for_test();

	MarkdownViewerHitTest hit;
	bool found = viewer->get_hit_test_at_for_test(Point2(8, 8), hit);

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
```

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
```

Expected: compile fails due to missing test helper methods or hit resolution.

- [ ] **Step 3: Implement interaction state**

Update `MarkdownViewer`:

- `real_t scroll_offset = 0.0;`
- `_clamp_scroll_offset()`.
- `_resolve_hit_test(Point2 p_position, MarkdownViewerHitTest &r_hit) const`.
- Test helpers:
  - `bool get_hit_test_at_for_test(Point2, MarkdownViewerHitTest &) const;`
  - `set_scroll_offset_for_test(real_t)`.
  - `get_scroll_offset_for_test()`.
- `_gui_input(const Ref<InputEvent> &p_event)`:
  - mouse wheel updates scroll offset and queues redraw.
  - left click on link emits `link_clicked`.
  - if `open_links_enabled`, call `OS::get_singleton()->shell_open(url)`.
  - left click on code copy hit copies to clipboard and emits signal.
- `NOTIFICATION_MOUSE_EXIT` clears hover state.

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: interaction tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer.h scene/gui/markdown_viewer.cpp scene/gui/markdown_viewer_layout.h scene/gui/markdown_viewer_layout.cpp
git commit -m "feat: add markdown viewer interactions"
```

---

### Task 8: Add Remote Image Requests

**Files:**

- Modify: `tests/scene/test_markdown_viewer.cpp`
- Modify: `scene/gui/markdown_viewer_image_loader.h`
- Modify: `scene/gui/markdown_viewer_image_loader.cpp`
- Modify: `scene/gui/markdown_viewer.h`
- Modify: `scene/gui/markdown_viewer.cpp`
- Modify: `scene/gui/markdown_viewer_layout.cpp`

- [ ] **Step 1: Write failing tests**

Keep tests deterministic and avoid real network:

```cpp
TEST_CASE("[SceneTree][MarkdownViewer] Image loader marks remote image as loading when enabled") {
	MarkdownViewerImageLoader *loader = memnew(MarkdownViewerImageLoader);
	loader->set_remote_images_enabled(true);

	MarkdownViewerImageRequestResult result = loader->ensure_image("https://example.com/image.png");

	CHECK(result.status == MarkdownViewerImageStatus::STATUS_LOADING);
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
```

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
```

Expected: tests fail because remote loading is not implemented.

- [ ] **Step 3: Implement remote request state**

Update image loader:

- Inherit from `Node` if not already.
- Maintain a map from URL to entry: status, texture, size, error, optional `HTTPRequest *`.
- `ensure_image()`:
  - If loaded/failed/loading exists, return current status.
  - If source is remote and disabled, return `STATUS_REMOTE_DISABLED`.
  - If source is remote and enabled, create `HTTPRequest`, add as child, set timeout/body size limit, connect `request_completed`, call `request(url)`, store loading.
  - If request start fails, set failed status and error.
- `_request_completed(...)` decodes successful 2xx buffers and updates status.
- `complete_request_for_test(...)` directly invokes the same completion path without network.
- Emit loader-level callback/signal or let viewer poll/receive a callable. Prefer a signal such as `image_state_changed(source)` from loader if making it a `Node`.

Update `MarkdownViewer`:

- Own one `MarkdownViewerImageLoader *image_loader`.
- Add it as an internal child.
- Forward `remote_images_enabled`.
- On `image_state_changed`, mark layout dirty, queue redraw, emit `image_loaded` or `image_failed`.
- Layout image blocks using loaded texture size when available, otherwise placeholder size.

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: remote image state tests pass without external network.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer_image_loader.h scene/gui/markdown_viewer_image_loader.cpp scene/gui/markdown_viewer.h scene/gui/markdown_viewer.cpp scene/gui/markdown_viewer_layout.cpp
git commit -m "feat: support markdown remote image loading"
```

---

### Task 9: Polish Theme Items And Public Node Behavior

**Files:**

- Modify: `tests/scene/test_markdown_viewer.cpp`
- Modify: `scene/gui/markdown_viewer.h`
- Modify: `scene/gui/markdown_viewer.cpp`
- Modify: `scene/gui/markdown_viewer_layout.h`
- Modify: `scene/gui/markdown_viewer_layout.cpp`
- Modify: `scene/gui/markdown_viewer_draw.cpp`

- [ ] **Step 1: Write failing tests**

Add tests for public properties and minimum size:

```cpp
TEST_CASE("[SceneTree][MarkdownViewer] Scroll disabled exposes content height through minimum size") {
	MarkdownViewer *viewer = memnew(MarkdownViewer);
	viewer->set_size(Size2(320, 120));
	viewer->set_scroll_enabled(false);
	viewer->set_markdown("# Title\n\nBody");
	viewer->force_layout_for_test();

	CHECK(viewer->get_minimum_size().y == doctest::Approx(viewer->get_content_height()));

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
```

- [ ] **Step 2: User-side red verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
```

Expected: compile or assertion failure because properties/minimum size are incomplete.

- [ ] **Step 3: Implement theme and public behavior**

Add public properties:

- `image_max_width`
- `image_max_height`

Bind theme items with `BIND_THEME_ITEM`/project-local style following nearby controls, or equivalent methods used in this codebase:

- fonts: `normal_font`, `bold_font`, `italics_font`, `mono_font`
- font sizes: `normal_font_size`, `h1_font_size`, `h2_font_size`, `h3_font_size`, `code_font_size`
- colors: `font_color`, `muted_font_color`, `link_color`, `code_background_color`, `code_border_color`, `table_border_color`, `table_header_background_color`, `table_row_background_color`, `blockquote_color`, `image_background_color`, `image_error_color`
- constants: `document_margin`, `block_spacing`, `table_cell_padding`, `code_padding`, `image_spacing`

Update `get_minimum_size()`:

- If `scroll_enabled`, return `Control::get_minimum_size()` plus any custom minimum.
- If not scroll-enabled, ensure layout and return height at least `content_height`.

Update draw/layout to consume theme metrics consistently.

- [ ] **Step 4: User-side green verification**

Ask the user to run:

```powershell
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: theme/property behavior tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/scene/test_markdown_viewer.cpp scene/gui/markdown_viewer.h scene/gui/markdown_viewer.cpp scene/gui/markdown_viewer_layout.h scene/gui/markdown_viewer_layout.cpp scene/gui/markdown_viewer_draw.cpp
git commit -m "feat: expose markdown viewer theme behavior"
```

---

### Future Phase: AI Markdown Usage Migration

Do not execute AI component migration in this implementation pass. After the user validates `MarkdownViewer` inside the engine as a general runtime UI node, create a separate plan for replacing `AIMarkdownLabel`/`AIMessageBubble` usage and updating `tests/editor/test_ai_model_settings.cpp`.

---

### Task 10: Manual Verification Scene And Final Cleanup

**Files:**

- Modify only if needed: files touched in previous tasks
- Optional create if useful and acceptable: `tests/scene/test_markdown_viewer.cpp` additional regression cases

- [ ] **Step 1: Run focused checks**

```powershell
git diff --check
scons platform=windows target=editor tests=yes
bin\next.windows.editor.x86_64.console.exe --test --test-case="*[MarkdownViewer]*"
```

Expected: no whitespace errors, build passes, runtime MarkdownViewer tests pass.

- [ ] **Step 2: Manual runtime smoke test**

Launch the editor or use an existing minimal scene to instantiate `MarkdownViewer` and set Markdown containing:

````markdown
# MarkdownViewer

Paragraph with [Godot](https://godotengine.org), **bold**, *italic*, and `inline code`.

| Name | Value |
| --- | --- |
| HP | 100 |
| MP | 50 |

```gdscript
extends Node

func _ready():
	print("hello")
```

![Local](res://icon.svg)
![Remote](https://example.com/image.png)
````

Check:

- remote image placeholder appears while `remote_images_enabled = false`
- enabling remote images starts async load without blocking
- code block copy button copies raw code
- link click emits signal and only opens browser when `open_links_enabled = true`
- scrolling works when content exceeds viewport

- [ ] **Step 3: Inspect code structure**

Check:

- `markdown_viewer.cpp` remains orchestration-focused.
- No `RichTextLabel`, `Label`, `CodeEdit`, or `TextureRect` child controls are created for rendering.
- Parser, layout, draw, image loading, and code highlighting are in separate files.
- Test-only helpers are not bound to scripting and are clearly named.
- Remote requests enforce timeout and body size limit.

- [ ] **Step 4: Final commit if cleanup was needed**

```powershell
git add scene/gui/markdown_viewer* tests/scene/test_markdown_viewer.cpp scene/register_scene_types.cpp
git commit -m "chore: finalize markdown viewer runtime node"
```

Skip this commit if Step 1-3 required no additional changes after the previous task commits.

---

## Execution Notes

- Follow TDD strictly: each task starts with a failing test, then minimal implementation, then passing focused tests.
- Keep implementation helpers free of editor-only dependencies.
- Do not add network tests that rely on external internet access.
- Do not reintroduce `RichTextLabel` or a child-control tree for Markdown rendering.
- Prefer internal helper APIs that can be tested without drawing pixels.
- Treat rendering screenshot/manual validation as a complement to deterministic unit tests, not a replacement.
