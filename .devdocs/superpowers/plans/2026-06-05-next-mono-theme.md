# Next Mono Editor Theme Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a selectable `Next Mono` editor theme style and `Next Engine (Mono)` color preset without changing existing Godot editor feature behavior.

**Architecture:** The implementation adds an isolated `ThemeNextMono` builder beside `ThemeModern` and `ThemeClassic`, then routes `EditorThemeManager` to it when `interface/theme/style` is `Next Mono`. The new builder reuses Modern's complete control coverage but overrides shared palette/style construction so the theme has a monochrome, hard-edged NextEngine identity.

**Tech Stack:** Godot editor C++, SCons source globbing, doctest-adjacent Python source contract test, PowerShell verification commands.

---

## File Structure

- Create: `editor/themes/theme_next_mono.h`
  - Declares the new isolated theme builder.
- Create: `editor/themes/theme_next_mono.cpp`
  - Implements monochrome shared styles and delegates broad control coverage to existing Modern population routines where appropriate.
- Modify: `editor/themes/editor_theme_manager.cpp`
  - Includes the new builder and dispatches by `interface/theme/style`.
  - Adds `Next Engine (Mono)` color preset.
  - Applies Next Mono structural defaults for relationship lines and corner radius.
- Modify: `editor/settings/editor_settings.cpp`
  - Adds `Next Mono` to theme style enum.
  - Adds `Next Engine (Mono)` to color preset enum.
- Create: `tests/python_build/test_next_mono_theme_contract.py`
  - Source-level contract test that fails when registration, preset, dispatch, or builder files are missing.

## Task 1: Add Source Contract Test

**Files:**
- Create: `tests/python_build/test_next_mono_theme_contract.py`

- [ ] **Step 1: Write the failing source contract test**

```python
#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    settings = read("editor/settings/editor_settings.cpp")
    manager = read("editor/themes/editor_theme_manager.cpp")

    require("Modern,Classic,Next Mono" in settings, "Next Mono must be registered in the theme style enum.")
    require("Next Engine (Mono)" in settings, "Next Engine (Mono) must be registered in the color preset enum.")
    require('#include "editor/themes/theme_next_mono.h"' in manager, "EditorThemeManager must include ThemeNextMono.")
    require('config.style == "Next Mono"' in manager, "EditorThemeManager must branch on the Next Mono style.")
    require('config.preset == "Next Engine (Mono)"' in manager, "EditorThemeManager must define the Next Engine (Mono) preset.")
    require((ROOT / "editor/themes/theme_next_mono.h").exists(), "theme_next_mono.h must exist.")
    require((ROOT / "editor/themes/theme_next_mono.cpp").exists(), "theme_next_mono.cpp must exist.")

    theme_h = read("editor/themes/theme_next_mono.h")
    theme_cpp = read("editor/themes/theme_next_mono.cpp")
    require("class ThemeNextMono" in theme_h, "ThemeNextMono class must be declared.")
    require("ThemeNextMono::populate_shared_styles" in theme_cpp, "ThemeNextMono shared styles must be implemented.")
    require("ThemeNextMono::populate_standard_styles" in theme_cpp, "ThemeNextMono standard styles must be implemented.")
    require("ThemeNextMono::populate_editor_styles" in theme_cpp, "ThemeNextMono editor styles must be implemented.")
    require("icon_saturation" in theme_cpp, "ThemeNextMono should explicitly support monochrome icon behavior.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2: Run the test and verify RED**

Run: `uv run python tests/python_build/test_next_mono_theme_contract.py`

Expected: FAIL because `Next Mono`, `Next Engine (Mono)`, and `theme_next_mono.*` do not exist yet.

## Task 2: Register Next Mono Settings And Dispatch

**Files:**
- Modify: `editor/settings/editor_settings.cpp`
- Modify: `editor/themes/editor_theme_manager.cpp`

- [ ] **Step 1: Add settings enum entries**

Update:

```cpp
EDITOR_SETTING_BASIC(Variant::STRING, PROPERTY_HINT_ENUM, "interface/theme/style", "Modern", "Modern,Classic,Next Mono")
EDITOR_SETTING_BASIC(Variant::STRING, PROPERTY_HINT_ENUM, "interface/theme/color_preset", "Default", "Default,Next Engine (Light),Next Engine (Mono),Breeze Dark,Godot 2,Godot 3,Gray,Light,Solarized (Dark),Solarized (Light),Black (OLED),Custom")
```

- [ ] **Step 2: Add manager include and dispatch**

Add:

```cpp
#include "editor/themes/theme_next_mono.h"
```

Replace the boolean Modern/Classic dispatch with explicit style checks:

```cpp
const bool is_next_mono_style = config.style == "Next Mono";
const bool is_classic_style = config.style == "Classic";

if (is_next_mono_style) {
	ThemeNextMono::populate_shared_styles(theme, config);
} else if (is_classic_style) {
	ThemeClassic::populate_shared_styles(theme, config);
} else {
	ThemeModern::populate_shared_styles(theme, config);
}
```

Repeat the same dispatch for `populate_standard_styles()` and `populate_editor_styles()`.

- [ ] **Step 3: Add preset and structural defaults**

In `_create_theme_config()`, add `Next Mono` handling:

```cpp
if (config.style == "Classic") {
	config.draw_relationship_lines = RELATIONSHIP_ALL;
	config.corner_radius = 3;
} else if (config.style == "Next Mono") {
	config.draw_relationship_lines = RELATIONSHIP_SELECTED_ONLY;
	config.corner_radius = 1;
} else {
	config.draw_relationship_lines = config.default_relationship_lines;
	config.corner_radius = config.default_corner_radius;
}
```

Add color preset:

```cpp
} else if (config.preset == "Next Engine (Mono)") {
	preset_accent_color = Color(0.92, 0.92, 0.92);
	preset_base_color = Color(0.07, 0.07, 0.07);
	preset_contrast = 0.26;
	preset_draw_extra_borders = true;
	preset_icon_saturation = 0.0;
```

## Task 3: Add ThemeNextMono Builder

**Files:**
- Create: `editor/themes/theme_next_mono.h`
- Create: `editor/themes/theme_next_mono.cpp`

- [ ] **Step 1: Create header**

Use the same public API as Modern/Classic:

```cpp
#pragma once

#include "editor/themes/editor_theme_manager.h"

class ThemeNextMono {
public:
	static void populate_shared_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config);
	static void populate_standard_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config);
	static void populate_editor_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config);
};
```

- [ ] **Step 2: Implement monochrome shared styles**

Implement `populate_shared_styles()` by calling `ThemeModern::populate_shared_styles()` first for full key coverage, then override the palette and shared style boxes with monochrome values:

```cpp
ThemeModern::populate_shared_styles(p_theme, p_config);

p_config.mono_color = Color(1, 1, 1);
p_config.mono_color_font = Color(1, 1, 1);
p_config.mono_color_inv = Color(0, 0, 0);
p_config.dark_theme = true;
p_config.dark_icon_and_font = true;
p_config.icon_saturation = 0.0;
```

Set core colors:

```cpp
p_config.surface_popup_color = Color(0.08, 0.08, 0.08);
p_config.surface_lowest_color = Color(0.05, 0.05, 0.05);
p_config.surface_lower_color = Color(0.07, 0.07, 0.07);
p_config.surface_low_color = Color(0.09, 0.09, 0.09);
p_config.surface_base_color = Color(0.11, 0.11, 0.11);
p_config.surface_high_color = Color(0.15, 0.15, 0.15);
p_config.surface_higher_color = Color(0.20, 0.20, 0.20);
p_config.surface_highest_color = Color(0.92, 0.92, 0.92);
```

Set font/icon colors and regenerate shared style boxes using `EditorThemeManager::make_flat_stylebox()`.

- [ ] **Step 3: Delegate complete standard and editor styles**

Use:

```cpp
void ThemeNextMono::populate_standard_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config) {
	ThemeModern::populate_standard_styles(p_theme, p_config);
}

void ThemeNextMono::populate_editor_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config) {
	ThemeModern::populate_editor_styles(p_theme, p_config);
}
```

This gives broad editor coverage while keeping Next Mono's palette/stylebox differences isolated in `populate_shared_styles()`.

## Task 4: Verify Green And Build Surface

**Files:**
- All touched files.

- [ ] **Step 1: Run source contract test**

Run: `uv run python tests/python_build/test_next_mono_theme_contract.py`

Expected: PASS.

- [ ] **Step 2: Run formatting/syntax checks**

Run: `git diff --check`

Expected: no output and exit code 0.

- [ ] **Step 3: Run focused build**

Run: `scons platform=windows target=editor dev_build=yes -j2`

Expected: build reaches exit code 0. If this build is too slow or blocked by local environment, report the exact failure.

## Task 5: Commit Theme Implementation

**Files:**
- Stage only files touched by this theme work.

- [ ] **Step 1: Review diff**

Run: `git diff -- editor/settings/editor_settings.cpp editor/themes/editor_theme_manager.cpp editor/themes/theme_next_mono.h editor/themes/theme_next_mono.cpp tests/python_build/test_next_mono_theme_contract.py`

Expected: diff contains only Next Mono theme work.

- [ ] **Step 2: Commit implementation**

Run:

```bash
git add editor/settings/editor_settings.cpp editor/themes/editor_theme_manager.cpp editor/themes/theme_next_mono.h editor/themes/theme_next_mono.cpp tests/python_build/test_next_mono_theme_contract.py
git commit -m "feat(editor): add next mono theme"
```

Expected: commit succeeds. Existing unrelated AI UI working tree changes must remain unstaged.
