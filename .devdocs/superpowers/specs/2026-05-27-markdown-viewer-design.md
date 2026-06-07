# MarkdownViewer Design

## Summary

Add a runtime `MarkdownViewer` UI node that renders Markdown directly from a custom `Control`, without `RichTextLabel` and without building a tree of existing UI controls. The node is intended for general Godot use and will later replace the current AI chat Markdown label.

The first version should prioritize a polished document surface, low per-viewer overhead, and a code structure that can grow to support richer Markdown features without turning the control into one large renderer file.

## Confirmed Decisions

- `MarkdownViewer` is a runtime scene node, not an editor-only AI component.
- The implementation must not be based on `RichTextLabel`.
- The implementation must not render by composing many existing controls such as `Label`, `CodeEdit`, or `TextureRect`.
- The rendering model is a self-drawn `Control` with retained parse, layout, draw, and hit-test data.
- Remote images are supported, but automatic remote image loading is disabled by default.
- The default presentation follows the "Full Document Surface" direction: readable spacing, polished tables, code panels, images, and visible links.

## Non-Goals For The First Version

- Editing Markdown source text.
- Full text selection across arbitrary Markdown content.
- Complete GitHub Flavored Markdown parity.
- Web browser behavior for all HTML embedded in Markdown.
- Replacing the existing `core/markdown` parser in this task.

## Runtime Node API

Class:

- `MarkdownViewer : Control`

Primary properties:

- `markdown: String`
- `remote_images_enabled: bool = false`
- `open_links_enabled: bool = false`
- `scroll_enabled: bool = true`
- `syntax_highlighting_enabled: bool = true`
- `code_copy_enabled: bool = true`
- `image_max_width: float`
- `image_max_height: float`
- `content_height: float` read-only

Signals:

- `link_clicked(url: String)`
- `image_loaded(source: String)`
- `image_failed(source: String, error: String)`
- `code_block_copied(language: String, code: String)`

Theme items should cover normal/bold/italic/mono fonts, heading sizes, text/link/muted/code/table colors, margins, block spacing, table cell padding, code padding, image spacing, and scrollbar colors.

## File Structure

Keep the public node small and split implementation into focused helpers under `scene/gui/`:

- `markdown_viewer.h/.cpp`: public Godot node, exported properties, signals, theme integration, invalidation, input dispatch.
- `markdown_viewer_document.h/.cpp`: converts Markdown text into lightweight viewer blocks and inline runs, using `MarkdownParser` plus a pipe-table prepass.
- `markdown_viewer_layout.h/.cpp`: measures and lays out blocks for the current width using `TextParagraph`, `TextLine`, and image dimensions.
- `markdown_viewer_draw.h/.cpp`: draws retained layout blocks with `CanvasItem` drawing APIs.
- `markdown_viewer_image_loader.h/.cpp`: handles local and remote image loading, status, size limits, and cache.
- `markdown_viewer_code_highlighter.h/.cpp`: lightweight tokenization and color spans for code blocks.

If the first implementation becomes too large, prefer adding a focused helper file over growing `markdown_viewer.cpp`.

## Internal Model

The viewer keeps two retained structures:

- Document model: semantic blocks and inline runs derived from Markdown.
- Layout model: exact rectangles, shaped text lines, image sizes, code button rects, link hit rects, and scrollbar state for the current width.

Representative block types:

- Paragraph
- Heading
- List and list item
- Block quote
- Code block
- Table
- Image
- Horizontal rule if added later

Representative inline run types:

- Text
- Strong
- Emphasis
- Inline code
- Link
- Image reference

The main node should not know the details of every block. Its responsibility is to manage lifecycle:

1. `set_markdown()` marks parse and layout dirty.
2. Resize or theme change marks layout and draw dirty.
3. Image load completion updates image state and marks layout dirty if dimensions changed.
4. `_draw()` asks the draw helper to paint visible retained layout items.
5. `_gui_input()` uses retained hit-test rects for links, code copy, scrolling, and hover state.

## Parsing And Tables

Use `core/markdown/MarkdownParser` for common Markdown AST nodes:

- document
- paragraph
- heading
- list and list item
- text
- inline code
- code block
- link
- image
- emphasis
- strong
- block quote

The current parser does not expose table nodes. For first-version table support, `markdown_viewer_document` should scan Markdown source for pipe table blocks before regular AST conversion, preserve their source range, and convert them into table blocks. Non-table ranges continue through `MarkdownParser`.

This keeps table rendering in the viewer while leaving room to move table support into `core/markdown` later.

## Layout

Layout is width-dependent and should be cached until markdown, theme, image dimensions, or control width changes.

Text layout should use Godot text shaping resources instead of manual glyph math. Blocks produce line boxes, content rects, and hit-test rects. The layout engine also computes `content_height`, which drives internal scrolling and minimum size hints.

Large documents should avoid shaping all content every frame. First version can lay out the full document on invalidation, but drawing must cull to the visible viewport. The layout data shape should allow future incremental layout or virtualization.

## Drawing

All visual output is drawn from `MarkdownViewer`:

- paragraphs and headings draw shaped text lines
- links draw colored/underlined text
- inline code draws a small background plus mono text
- code blocks draw panel background, language label, copy button, and highlighted text
- tables draw header background, alternating rows, borders, and cell text
- images draw loaded textures or loading/disabled/error placeholders
- block quotes draw a vertical accent and indented content
- scrollbar is drawn by the viewer when `scroll_enabled` is true

Use restrained radii and theme colors. The node should look like a document viewer, not a collection of floating cards.

## Interaction

Hit-test data is produced during layout and consumed by `_gui_input()`.

Links:

- hover changes cursor/visual state
- click emits `link_clicked(url)`
- if `open_links_enabled` is true, also call `OS::shell_open(url)`

Code blocks:

- copy button hit rect copies raw code to clipboard through `DisplayServer::clipboard_set`
- emit `code_block_copied(language, code)`

Scrolling:

- mouse wheel adjusts internal `scroll_offset`
- keyboard/page navigation can be added if the control has focus
- drawn content is clipped to the control rect

Full document text selection is deferred. The first version should still provide practical copy affordances through code block copy and a future `copy_markdown()` method if needed.

## Images

Local images:

- support `res://`, `user://`, and project-relative paths where the engine has access
- load through engine image/resource APIs and convert to `ImageTexture`

Remote images:

- disabled by default through `remote_images_enabled = false`
- when disabled, draw a placeholder that indicates remote loading is disabled
- when enabled, load asynchronously using `HTTPRequest`
- support at least PNG, JPEG, and WebP buffers
- enforce timeout and maximum body size
- cache by URL inside the viewer or a small shared helper cache
- emit success/failure signals

Remote loading must never block layout or drawing. Image placeholders reserve a predictable size until real dimensions are known.

## Syntax Highlighting

Do not instantiate `CodeEdit` for code blocks. Implement a lightweight highlighter that returns colored spans per line.

First-version languages:

- generic fallback
- GDScript keywords/comments/strings/numbers
- JSON strings/numbers/booleans/null
- C/C++ style comments/strings/keywords

The highlighter should be a replaceable helper so future language support, theme-specific colors, or a TextServer-backed token pipeline can be added without changing the main node.

## Extensibility Rules

The code should keep these boundaries:

- parsing adapters create document blocks
- layout code computes geometry and text shaping
- draw code paints already-laid-out items
- image loading owns network/resource status
- interaction code consumes retained hit-test records
- `MarkdownViewer` orchestrates state and exposes the public API

New features should normally enter through a new block/inline type plus focused parser, layout, draw, and hit-test handling. Avoid adding feature-specific state directly to the public node unless it is part of the Godot API.

Likely future extensions:

- task lists
- horizontal rules
- footnotes
- Mermaid or diagram placeholders
- math blocks
- image click/preview behavior
- richer language highlighting
- document text selection
- incremental layout for very large documents

## AI Component Migration

After the runtime node works, update the AI message UI to use `MarkdownViewer` for assistant Markdown content. Keep `AIMarkdownLabel` only as a compatibility wrapper if existing editor code still depends on its API.

Old `AIMarkdownRenderer`/`RichTextLabel` tests should be replaced or narrowed to parser compatibility. New viewer behavior belongs in `tests/scene`, not only `tests/editor`, because the node is runtime scene UI.

## TDD And Verification Plan

Implementation must start with failing tests.

Initial test targets:

- `MarkdownViewer` is registered as a runtime class.
- `markdown` property stores source and invalidates parse/layout.
- basic Markdown produces paragraph, heading, list, link, image, and code block document items.
- pipe table source produces a table block with expected rows and cells.
- remote image URLs do not start requests when `remote_images_enabled` is false.
- remote image state transitions are testable through the image loader helper.
- code block hit-test data includes a copy action with the raw code.
- link hit-test data preserves the original URL.
- layout produces positive `content_height` for non-empty Markdown.

Build/update wiring:

- add source files to `scene/gui/SCsub`
- register class in `scene/register_scene_types.cpp`
- add tests under `tests/scene/test_markdown_viewer.cpp`
- migrate AI component tests after runtime viewer behavior is covered

Manual validation after automated tests:

- create a small scene or script that instantiates `MarkdownViewer`
- render headings, paragraphs, table, code block, local image, disabled remote image placeholder, and enabled remote image
- verify clicking links and code copy behavior
