# NextEngine Mono Editor Theme Design

## Summary

NextEngine needs a new editor theme that can be enabled from the editor theme settings without invasive changes to Godot's original UI code. The chosen visual direction is **Monolith Workbench**: a black, white, and gray editor surface with a dark workspace, bright top-level affordances, hard edges, low saturation icons, and minimal rounded corners. The theme should look clearly unlike Godot's default blue/purple, soft, rounded visual language while staying usable for long editing sessions.

The implementation should add a new theme style rather than rewriting controls or changing feature code. Godot's existing editor theme generation already centralizes the UI surface in `EditorThemeManager`, `ThemeModern`, and `ThemeClassic`, so the new theme can be introduced as a third style builder with small registration changes.

## Goals

- Add a selectable NextEngine-specific editor theme with a black/white/gray minimalist identity.
- Keep the implementation low-intrusion by adding a new theme builder and narrow registration points.
- Avoid modifying AI feature logic, user system logic, scene GUI behavior, or existing Godot editor controls.
- Make the theme visibly distinct from Godot's default theme through color, shape, contrast, and spacing.
- Preserve usability in the editor, script editor, inspector, docks, output panel, and AI panels.

## Non-Goals

- Replacing Godot's full editor layout system.
- Reworking editor icons as new SVG assets in the first version.
- Creating a runtime project theme for user games.
- Changing MarkdownViewer rendering behavior beyond normal editor theme inheritance.
- Removing existing Modern, Classic, or custom theme support.

## Visual Direction

The theme uses a monochrome product-workbench style:

- **Backgrounds:** near-black workspace base, dark gray panels, and black viewport surfaces.
- **Top-level identity:** a white or very light top strip/selected state to create a strong NextEngine visual signature.
- **Selection:** white or bright gray selection borders and fills, with no blue accent.
- **Controls:** flatter buttons, lower corner radius, visible one-pixel borders, restrained hover/pressed states.
- **Icons:** grayscale by setting low icon saturation and monochrome theme colors.
- **Typography:** existing editor fonts are reused, but font colors are tuned toward high contrast and reduced secondary text brightness.
- **Semantic colors:** error/warning/success may remain lightly tinted only where removing color would harm readability. The default target is grayscale-first.

The reference mockup lives at:

`.superpowers/brainstorm/next-theme/visual-style-options.html`

The selected direction is option C, "Monolith Workbench".

## Architecture

Add a new theme builder beside the existing ones:

- `editor/themes/theme_next_mono.h`
- `editor/themes/theme_next_mono.cpp`

The class should follow the existing shape:

```cpp
class ThemeNextMono {
public:
	static void populate_shared_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config);
	static void populate_standard_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config);
	static void populate_editor_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config);
};
```

`EditorThemeManager::_create_base_theme()` should dispatch by `interface/theme/style`:

- `Modern` -> `ThemeModern`
- `Classic` -> `ThemeClassic`
- `Next Mono` -> `ThemeNextMono`

This keeps the existing theme generation pipeline intact:

1. Read `EditorSettings`.
2. Create `ThemeConfiguration`.
3. Populate shared colors and base style boxes.
4. Register icons and fonts.
5. Populate standard Control styles.
6. Populate editor-specific styles.
7. Apply text editor and visual shader styles.
8. Merge optional custom theme.

## Editor Settings Integration

Update `editor/settings/editor_settings.cpp`:

- Add `Next Mono` to `interface/theme/style`.
- Add `Next Engine (Mono)` to `interface/theme/color_preset`.

`Next Engine (Mono)` should set:

- `base_color`: near black, approximately `Color(0.07, 0.07, 0.07)`.
- `accent_color`: white or near white, approximately `Color(0.92, 0.92, 0.92)`.
- `contrast`: moderate positive contrast, approximately `0.22` to `0.30`.
- `draw_extra_borders`: `true`.
- `icon_saturation`: `0.0` or very close to `0.0`.

When `style == "Next Mono"` and `color_preset != "Custom"`, default structural settings should be enforced:

- `corner_radius`: `1` or `2`.
- `draw_relationship_lines`: `RELATIONSHIP_SELECTED_ONLY`.
- `border_size`: keep user setting unless visual testing shows the theme needs a visible default.

## Theme Builder Strategy

The first version should not copy every line from `ThemeModern` blindly. It should reuse the existing ThemeConfiguration fields and implement only the minimum complete surface:

- Shared palette fields:
  - `mono_color`
  - `mono_color_font`
  - `surface_*`
  - `font_*`
  - `icon_*`
  - `button_*`
  - `selection_color`
  - `separator_color`
  - `shadow_color`

- Shared StyleBoxes:
  - `base_style`
  - `focus_style`
  - `button_style*`
  - `flat_button*`
  - `popup_style`
  - `window_style`
  - `dialog_style`
  - `panel_container_style`
  - `content_panel_style`
  - `tree_panel_style`
  - `tab_container_style`
  - `foreground_panel`

- Standard controls:
  - Button
  - CheckBox/CheckButton/RadioButton
  - LineEdit/TextEdit/CodeEdit inherited styling
  - Tree/ItemList
  - TabBar/TabContainer
  - PopupMenu
  - ScrollBar/Slider
  - Panel/PanelContainer

- Editor-specific surfaces:
  - Editor top bars and main screen tabs
  - Docks
  - Inspector
  - FileSystem dock
  - Output/log areas
  - Graph/visual editor surfaces
  - AI component panels through inherited editor styles where possible

If a full style implementation becomes too large, the acceptable fallback is to copy the Modern builder into `ThemeNextMono` and then tune the palette and key style boxes. That is still low-intrusion because the copy is isolated in a new file and does not alter existing Modern behavior.

## Data Flow

Theme selection data flow:

1. User selects `Next Mono` in editor settings.
2. `EditorSettings` stores `interface/theme/style = "Next Mono"`.
3. `EditorThemeManager::is_generated_theme_outdated()` detects changes under `interface/theme`.
4. `EditorThemeManager::generate_theme()` calls `_create_base_theme()`.
5. `_create_theme_config()` resolves `Next Engine (Mono)` preset values and structural defaults.
6. `_create_base_theme()` invokes `ThemeNextMono` population methods.
7. Existing editor UI receives the generated `EditorTheme`.

No new runtime state or persistent project metadata is required.

## Error Handling

- Unknown style values should continue to fall back to Modern or Classic behavior rather than crashing.
- If `Next Engine (Mono)` is not selected, `Next Mono` should still produce a coherent theme from custom user colors.
- Custom theme merging must remain unchanged.
- If icon generation with zero saturation makes semantic icons ambiguous, raise icon saturation slightly or preserve a small set of semantic colors for error/warning/success.

## Testing And Verification

Manual verification should cover:

- Theme appears in the editor settings style dropdown.
- Color preset appears in the color preset dropdown.
- Switching between Modern, Classic, and Next Mono regenerates the theme without restart.
- Editor startup with Next Mono selected succeeds.
- Main editor, scene dock, inspector, filesystem dock, output panel, script editor, project settings, popups, and AI panels are readable.
- Focus rings, selected rows, disabled controls, and hover states are visible.
- Text editor syntax highlighting remains legible.
- No accidental changes to AI runtime behavior, user auth behavior, or MarkdownViewer node behavior.

Build verification:

- Run a focused editor build command if available for the current platform.
- Run existing relevant tests if they are already part of the local workflow.
- At minimum run `git diff --check` before finalizing.

## Implementation Boundaries

Expected files:

- `editor/themes/theme_next_mono.h`
- `editor/themes/theme_next_mono.cpp`
- `editor/themes/editor_theme_manager.cpp`
- `editor/settings/editor_settings.cpp`

Potentially touched only if needed:

- `editor/themes/editor_theme_manager.h`

Do not modify:

- `editor/ai_component/**` behavior files.
- `editor/user_system/**`.
- `scene/gui/markdown_viewer*`.
- Existing `theme_modern.*` and `theme_classic.*` behavior except for harmless includes or shared helper adjustments that are strictly necessary.

## Open Decisions

- Final exact grayscale palette should be tuned after the first in-editor screenshot pass.
- Whether semantic colors are fully grayscale or retain minimal tint should be decided by readability.
- Whether the white top-level surface should apply globally or only to selected top navigation elements should be adjusted after seeing it inside the real editor.
