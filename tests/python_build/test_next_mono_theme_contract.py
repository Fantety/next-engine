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
