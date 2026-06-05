/**************************************************************************/
/*  theme_next_mono.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "theme_next_mono.h"

#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/theme_modern.h"
#include "scene/resources/style_box_flat.h"
#include "scene/scene_string_names.h"

static Ref<StyleBoxFlat> _make_mono_stylebox(Color p_color, Color p_border_color, int p_border_width, float p_margin_left, float p_margin_top, float p_margin_right, float p_margin_bottom, int p_corner_radius) {
	Ref<StyleBoxFlat> style = EditorThemeManager::make_flat_stylebox(p_color, p_margin_left, p_margin_top, p_margin_right, p_margin_bottom, p_corner_radius);
	style->set_border_color(p_border_color);
	style->set_border_width_all(p_border_width);
	return style;
}

void ThemeNextMono::populate_shared_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config) {
	ThemeModern::populate_shared_styles(p_theme, p_config);

	p_config.dark_theme = true;
	p_config.dark_icon_and_font = true;
	p_config.icon_saturation = 0.0;

	p_config.mono_color = Color(1, 1, 1);
	p_config.mono_color_font = Color(1, 1, 1);
	p_config.mono_color_inv = Color(0, 0, 0);

	p_config.base_color = Color(0.07, 0.07, 0.07);
	p_config.accent_color = Color(0.92, 0.92, 0.92);
	p_config.dark_color_1 = Color(0.045, 0.045, 0.045);
	p_config.dark_color_2 = Color(0, 0, 0, 0.42);
	p_config.dark_color_3 = Color(0.12, 0.12, 0.12);
	p_config.contrast_color_1 = Color(0.32, 0.32, 0.32);
	p_config.contrast_color_2 = Color(0.48, 0.48, 0.48);

	p_config.highlight_color = Color(1, 1, 1, 0.16);
	p_config.highlight_disabled_color = Color(1, 1, 1, 0.08);
	p_config.success_color = Color(0.78, 0.78, 0.78);
	p_config.warning_color = Color(0.68, 0.68, 0.68);
	p_config.error_color = Color(0.94, 0.94, 0.94);
	p_config.extra_border_color_1 = Color(1, 1, 1, 0.42);
	p_config.extra_border_color_2 = Color(1, 1, 1, 0.20);

	p_config.font_color = Color(0.88, 0.88, 0.88);
	p_config.font_secondary_color = Color(0.62, 0.62, 0.62);
	p_config.font_focus_color = Color(1, 1, 1);
	p_config.font_hover_color = Color(0.96, 0.96, 0.96);
	p_config.font_pressed_color = Color(0.05, 0.05, 0.05);
	p_config.font_hover_pressed_color = Color(0, 0, 0);
	p_config.font_disabled_color = Color(1, 1, 1, 0.34);
	p_config.font_readonly_color = Color(1, 1, 1, 0.56);
	p_config.font_placeholder_color = Color(1, 1, 1, 0.38);
	p_config.font_outline_color = Color(1, 1, 1, 0);

	p_config.font_dark_background_color = p_config.font_color;
	p_config.font_dark_background_focus_color = p_config.font_focus_color;
	p_config.font_dark_background_hover_color = p_config.font_hover_color;
	p_config.font_dark_background_pressed_color = p_config.font_pressed_color;
	p_config.font_dark_background_hover_pressed_color = p_config.font_hover_pressed_color;

	p_config.icon_normal_color = Color(0.9, 0.9, 0.9);
	p_config.icon_secondary_color = Color(0.64, 0.64, 0.64);
	p_config.icon_focus_color = Color(1, 1, 1);
	p_config.icon_hover_color = Color(1, 1, 1);
	p_config.icon_pressed_color = Color(0.02, 0.02, 0.02);
	p_config.icon_disabled_color = Color(1, 1, 1, 0.34);

	p_config.surface_popup_color = Color(0.08, 0.08, 0.08);
	p_config.surface_lowest_color = Color(0.05, 0.05, 0.05);
	p_config.surface_lower_color = Color(0.07, 0.07, 0.07);
	p_config.surface_low_color = Color(0.09, 0.09, 0.09);
	p_config.surface_base_color = Color(0.11, 0.11, 0.11);
	p_config.surface_high_color = Color(0.15, 0.15, 0.15);
	p_config.surface_higher_color = Color(0.20, 0.20, 0.20);
	p_config.surface_highest_color = Color(0.92, 0.92, 0.92);

	p_config.button_normal_color = Color(0.10, 0.10, 0.10);
	p_config.button_hover_color = Color(0.16, 0.16, 0.16);
	p_config.button_pressed_color = Color(0.92, 0.92, 0.92);
	p_config.button_disabled_color = Color(0.08, 0.08, 0.08);
	p_config.button_border_normal_color = Color(1, 1, 1, 0.20);
	p_config.button_border_hover_color = Color(1, 1, 1, 0.34);
	p_config.button_border_pressed_color = Color(1, 1, 1, 0.92);

	p_config.flat_button_hover_color = Color(1, 1, 1, 0.08);
	p_config.flat_button_pressed_color = Color(1, 1, 1, 0.16);
	p_config.flat_button_hover_pressed_color = Color(1, 1, 1, 0.22);

	p_config.shadow_color = Color(0, 0, 0, 0.38);
	p_config.selection_color = Color(1, 1, 1, 0.22);
	p_config.disabled_border_color = Color(1, 1, 1, 0.12);
	p_config.disabled_bg_color = Color(1, 1, 1, 0.04);
	p_config.separator_color = Color(1, 1, 1, 0.16);

	p_theme->set_color("base_color", EditorStringName(Editor), p_config.base_color);
	p_theme->set_color("accent_color", EditorStringName(Editor), p_config.accent_color);
	p_theme->set_color("mono_color", EditorStringName(Editor), p_config.mono_color);
	p_theme->set_color("dark_color_1", EditorStringName(Editor), p_config.dark_color_1);
	p_theme->set_color("dark_color_2", EditorStringName(Editor), p_config.dark_color_2);
	p_theme->set_color("dark_color_3", EditorStringName(Editor), p_config.dark_color_3);
	p_theme->set_color("contrast_color_1", EditorStringName(Editor), p_config.contrast_color_1);
	p_theme->set_color("contrast_color_2", EditorStringName(Editor), p_config.contrast_color_2);
	p_theme->set_color("highlight_color", EditorStringName(Editor), p_config.highlight_color);
	p_theme->set_color("highlight_disabled_color", EditorStringName(Editor), p_config.highlight_disabled_color);
	p_theme->set_color("success_color", EditorStringName(Editor), p_config.success_color);
	p_theme->set_color("warning_color", EditorStringName(Editor), p_config.warning_color);
	p_theme->set_color("error_color", EditorStringName(Editor), p_config.error_color);
	p_theme->set_color("success_color_dark_background", EditorStringName(Editor), p_config.success_color);
	p_theme->set_color("warning_color_dark_background", EditorStringName(Editor), p_config.warning_color);
	p_theme->set_color("error_color_dark_background", EditorStringName(Editor), p_config.error_color);
	p_theme->set_color("ruler_color", EditorStringName(Editor), Color(1, 1, 1, 0.22));
	p_theme->set_color("extra_border_color_1", EditorStringName(Editor), p_config.extra_border_color_1);
	p_theme->set_color("extra_border_color_2", EditorStringName(Editor), p_config.extra_border_color_2);
	p_theme->set_color(SceneStringName(font_color), EditorStringName(Editor), p_config.font_color);
	p_theme->set_color("font_focus_color", EditorStringName(Editor), p_config.font_focus_color);
	p_theme->set_color("font_hover_color", EditorStringName(Editor), p_config.font_hover_color);
	p_theme->set_color("font_pressed_color", EditorStringName(Editor), p_config.font_pressed_color);
	p_theme->set_color("font_hover_pressed_color", EditorStringName(Editor), p_config.font_hover_pressed_color);
	p_theme->set_color("font_disabled_color", EditorStringName(Editor), p_config.font_disabled_color);
	p_theme->set_color("font_readonly_color", EditorStringName(Editor), p_config.font_readonly_color);
	p_theme->set_color("font_placeholder_color", EditorStringName(Editor), p_config.font_placeholder_color);
	p_theme->set_color("font_outline_color", EditorStringName(Editor), p_config.font_outline_color);
	p_theme->set_color("font_dark_background_color", EditorStringName(Editor), p_config.font_dark_background_color);
	p_theme->set_color("font_dark_background_focus_color", EditorStringName(Editor), p_config.font_dark_background_focus_color);
	p_theme->set_color("font_dark_background_hover_color", EditorStringName(Editor), p_config.font_dark_background_hover_color);
	p_theme->set_color("font_dark_background_pressed_color", EditorStringName(Editor), p_config.font_dark_background_pressed_color);
	p_theme->set_color("font_dark_background_hover_pressed_color", EditorStringName(Editor), p_config.font_dark_background_hover_pressed_color);
	p_theme->set_color("icon_normal_color", EditorStringName(Editor), p_config.icon_normal_color);
	p_theme->set_color("icon_focus_color", EditorStringName(Editor), p_config.icon_focus_color);
	p_theme->set_color("icon_hover_color", EditorStringName(Editor), p_config.icon_hover_color);
	p_theme->set_color("icon_pressed_color", EditorStringName(Editor), p_config.icon_pressed_color);
	p_theme->set_color("icon_disabled_color", EditorStringName(Editor), p_config.icon_disabled_color);
	p_theme->set_color("selection_color", EditorStringName(Editor), p_config.selection_color);
	p_theme->set_color("disabled_border_color", EditorStringName(Editor), p_config.disabled_border_color);
	p_theme->set_color("disabled_bg_color", EditorStringName(Editor), p_config.disabled_bg_color);
	p_theme->set_color("separator_color", EditorStringName(Editor), p_config.separator_color);
	p_theme->set_color("icon_saturation", EditorStringName(Editor), Color(p_config.icon_saturation, p_config.icon_saturation, p_config.icon_saturation));

	const int border_width = MAX(1, p_config.border_width);
	const int radius = p_config.corner_radius;
	const Color hairline = Color(1, 1, 1, 0.18);
	const Color strong_line = Color(1, 1, 1, 0.44);

	p_config.base_style = _make_mono_stylebox(p_config.surface_low_color, hairline, border_width, p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin, radius);
	p_config.focus_style = EditorThemeManager::make_flat_stylebox(Color(1, 1, 1, 0.03), -1, -1, -1, -1, radius);
	p_config.focus_style->set_border_color(Color(1, 1, 1, 0.78));
	p_config.focus_style->set_border_width_all(2 * MAX(1, EDSCALE));
	p_config.base_empty_style = EditorThemeManager::make_empty_stylebox(p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin);
	p_config.base_empty_wide_style = EditorThemeManager::make_empty_stylebox(p_config.base_margin * 1.5, p_config.base_margin, p_config.base_margin * 1.5, p_config.base_margin);

	p_config.button_style = _make_mono_stylebox(p_config.button_normal_color, p_config.button_border_normal_color, border_width, p_config.base_margin * 2, p_config.base_margin * 1.5, p_config.base_margin * 2, p_config.base_margin * 1.5, radius);
	p_config.button_style_disabled = _make_mono_stylebox(p_config.button_disabled_color, p_config.disabled_border_color, border_width, p_config.base_margin * 2, p_config.base_margin * 1.5, p_config.base_margin * 2, p_config.base_margin * 1.5, radius);
	p_config.button_style_focus = p_config.focus_style;
	p_config.button_style_pressed = _make_mono_stylebox(p_config.button_pressed_color, p_config.button_border_pressed_color, border_width, p_config.base_margin * 2, p_config.base_margin * 1.5, p_config.base_margin * 2, p_config.base_margin * 1.5, radius);
	p_config.button_style_hover = _make_mono_stylebox(p_config.button_hover_color, p_config.button_border_hover_color, border_width, p_config.base_margin * 2, p_config.base_margin * 1.5, p_config.base_margin * 2, p_config.base_margin * 1.5, radius);

	p_config.flat_button = _make_mono_stylebox(Color(0, 0, 0, 0), Color(0, 0, 0, 0), 0, p_config.base_margin * 1.5, p_config.base_margin, p_config.base_margin * 1.5, p_config.base_margin, radius);
	p_config.flat_button_pressed = _make_mono_stylebox(p_config.flat_button_pressed_color, strong_line, border_width, p_config.base_margin * 1.5, p_config.base_margin, p_config.base_margin * 1.5, p_config.base_margin, radius);
	p_config.flat_button_hover_pressed = _make_mono_stylebox(p_config.flat_button_hover_pressed_color, strong_line, border_width, p_config.base_margin * 1.5, p_config.base_margin, p_config.base_margin * 1.5, p_config.base_margin, radius);
	p_config.flat_button_hover = _make_mono_stylebox(p_config.flat_button_hover_color, hairline, border_width, p_config.base_margin * 1.5, p_config.base_margin, p_config.base_margin * 1.5, p_config.base_margin, radius);

	p_config.popup_style = _make_mono_stylebox(p_config.surface_popup_color, strong_line, border_width, p_config.popup_margin, p_config.popup_margin, p_config.popup_margin, p_config.popup_margin, 0);
	p_config.popup_border_style = _make_mono_stylebox(p_config.surface_popup_color, strong_line, border_width, p_config.popup_margin, p_config.popup_margin, p_config.popup_margin, p_config.popup_margin, 0);
	p_config.popup_panel_style = _make_mono_stylebox(p_config.surface_popup_color, strong_line, border_width, p_config.popup_margin, p_config.popup_margin, p_config.popup_margin, p_config.popup_margin, 0);
	p_config.window_style = _make_mono_stylebox(p_config.surface_lowest_color, strong_line, border_width, p_config.window_border_margin, p_config.window_border_margin, p_config.window_border_margin, p_config.window_border_margin, 0);
	p_config.window_complex_style = _make_mono_stylebox(p_config.surface_lowest_color, strong_line, border_width, p_config.window_border_margin, p_config.window_border_margin, p_config.window_border_margin, p_config.window_border_margin, 0);
	p_config.dialog_style = _make_mono_stylebox(p_config.surface_lowest_color, strong_line, border_width, p_config.window_border_margin, p_config.window_border_margin, p_config.window_border_margin, p_config.window_border_margin, 0);
	p_config.panel_container_style = _make_mono_stylebox(p_config.surface_lower_color, Color(1, 1, 1, 0.10), 0, p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin, radius);
	p_config.content_panel_style = _make_mono_stylebox(p_config.surface_low_color, hairline, border_width, p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin, radius);
	p_config.tree_panel_style = _make_mono_stylebox(p_config.surface_lower_color, hairline, border_width, p_config.base_margin * 1.5, p_config.base_margin * 2.5, p_config.base_margin * 1.5, p_config.base_margin * 2.5, radius);
	p_config.tab_container_style = _make_mono_stylebox(p_config.surface_low_color, hairline, border_width, p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin, radius);
	p_config.foreground_panel = _make_mono_stylebox(p_config.surface_base_color, hairline, border_width, p_config.base_margin, p_config.base_margin, p_config.base_margin, p_config.base_margin, radius);
}

void ThemeNextMono::populate_standard_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config) {
	ThemeModern::populate_standard_styles(p_theme, p_config);
}

void ThemeNextMono::populate_editor_styles(const Ref<EditorTheme> &p_theme, EditorThemeManager::ThemeConfiguration &p_config) {
	ThemeModern::populate_editor_styles(p_theme, p_config);
}
