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

    require(
        '"interface/theme/style", "Next Mono", "Modern,Classic,Next Mono"' in settings,
        "Next Mono must be the default editor theme style.",
    )
    require(
        '"interface/theme/color_preset", "Next Engine (Mono)"' in settings,
        "Next Engine (Mono) must be the default editor color preset.",
    )
    require("Modern,Classic,Next Mono" in settings, "Next Mono must be registered in the theme style enum.")
    require("Next Engine (Mono)" in settings, "Next Engine (Mono) must be registered in the color preset enum.")
    require("Next Engine (Light Mono)" in settings, "Next Engine (Light Mono) must be registered in the color preset enum.")
    require('#include "editor/themes/theme_next_mono.h"' in manager, "EditorThemeManager must include ThemeNextMono.")
    require('config.style == "Next Mono"' in manager, "EditorThemeManager must branch on the Next Mono style.")
    require('config.preset == "Next Engine (Mono)"' in manager, "EditorThemeManager must define the Next Engine (Mono) preset.")
    require('config.preset == "Next Engine (Light Mono)"' in manager, "EditorThemeManager must define the Next Engine (Light Mono) preset.")
    require((ROOT / "editor/themes/theme_next_mono.h").exists(), "theme_next_mono.h must exist.")
    require((ROOT / "editor/themes/theme_next_mono.cpp").exists(), "theme_next_mono.cpp must exist.")

    theme_h = read("editor/themes/theme_next_mono.h")
    theme_cpp = read("editor/themes/theme_next_mono.cpp")
    require("class ThemeNextMono" in theme_h, "ThemeNextMono class must be declared.")
    require("ThemeNextMono::populate_shared_styles" in theme_cpp, "ThemeNextMono shared styles must be implemented.")
    require("ThemeNextMono::populate_standard_styles" in theme_cpp, "ThemeNextMono standard styles must be implemented.")
    require("ThemeNextMono::populate_editor_styles" in theme_cpp, "ThemeNextMono editor styles must be implemented.")
    require("icon_saturation" in theme_cpp, "ThemeNextMono should explicitly support monochrome icon behavior.")
    require(
        'p_config.preset == "Next Engine (Light Mono)"' in theme_cpp,
        "ThemeNextMono must recognize its light mono preset.",
    )
    require(
        "p_config.dark_icon_and_font = false;" in theme_cpp,
        "Next Mono light must keep dark_icon_and_font false so Godot converts SVG icons for light backgrounds.",
    )
    require(
        "p_config.success_color = Color(0.34, 0.86, 0.45);" in theme_cpp,
        "Next Mono must keep success_color green so completed/running AI plan rows stay semantic.",
    )
    require(
        'p_theme->set_color("font_pressed_color", "CheckButton", p_config.font_focus_color);' in theme_cpp,
        "Next Mono must keep pressed CheckButton text visible for compact toolbar toggles.",
    )
    require(
        'p_theme->set_color("font_hover_pressed_color", "CheckButton", p_config.font_focus_color);' in theme_cpp,
        "Next Mono must keep hover-pressed CheckButton text visible for compact toolbar toggles.",
    )
    require(
        'p_theme->set_color("font_pressed_color", "CheckBox", p_config.font_focus_color);' in theme_cpp,
        "Next Mono must keep pressed CheckBox text visible for selected project renderer choices.",
    )
    require(
        'p_theme->set_color("font_hover_pressed_color", "CheckBox", p_config.font_focus_color);' in theme_cpp,
        "Next Mono must keep hover-pressed CheckBox text visible for selected project renderer choices.",
    )
    require(
        'p_theme->set_color("font_selected_color", "ItemList", p_config.font_focus_color);' in theme_cpp,
        "Next Mono must keep selected ItemList text visible on dark selection backgrounds.",
    )
    require(
        'p_theme->set_color("font_hovered_selected_color", "ItemList", p_config.font_focus_color);' in theme_cpp,
        "Next Mono must keep hovered selected ItemList text visible on dark selection backgrounds.",
    )
    require(
        'p_theme->set_color("font_selected_color", "LineEdit", p_config.font_focus_color);' in theme_cpp,
        "Next Mono must keep selected LineEdit text visible on dark selection backgrounds.",
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
