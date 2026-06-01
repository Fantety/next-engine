/**************************************************************************/
/*  ai_next_marquee_settings.cpp                                          */
/**************************************************************************/

#include "ai_next_marquee_settings.h"

#include "editor/settings/editor_settings.h"

namespace {

const char *AURORA_SHADER = R"(
shader_type canvas_item;
render_mode unshaded;

uniform vec4 color_a : source_color = vec4(0.09, 0.68, 1.0, 1.0);
uniform vec4 color_b : source_color = vec4(0.53, 0.36, 1.0, 1.0);
uniform vec4 color_c : source_color = vec4(1.0, 0.33, 0.67, 1.0);
uniform vec4 color_d : source_color = vec4(0.18, 0.92, 0.78, 1.0);
uniform float speed = 0.95;

vec3 palette(float t) {
	vec3 a = color_a.rgb;
	vec3 b = color_b.rgb;
	vec3 c = color_c.rgb;
	vec3 d = color_d.rgb;
	return a + b * cos(TAU * (c * t + d));
}

void fragment() {
	float phase = TIME * speed;
	float wave = UV.x * 2.4 - phase;
	vec3 color = palette(wave);
	color += 0.08 * palette(wave + 0.17);
	color += 0.05 * palette(wave + 0.41);
	float edge = smoothstep(0.0, 0.08, min(UV.y, 1.0 - UV.y));
	COLOR = vec4(clamp(color, 0.0, 1.0), edge);
}
)";

const char *SIGNAL_SHADER = R"(
shader_type canvas_item;
render_mode unshaded;

uniform vec4 background : source_color = vec4(0.03, 0.08, 0.13, 1.0);
uniform vec4 highlight : source_color = vec4(0.25, 1.0, 0.72, 1.0);
uniform vec4 accent : source_color = vec4(1.0, 0.88, 0.30, 1.0);
uniform float speed = 1.2;

void fragment() {
	float sweep = fract(UV.x * 1.7 - TIME * speed);
	float beam = smoothstep(0.56, 0.74, sweep) * (1.0 - smoothstep(0.80, 1.0, sweep));
	float ribs = 0.35 + 0.65 * smoothstep(0.42, 0.48, sin((UV.x + TIME * 0.35) * 48.0));
	vec3 color = mix(background.rgb, highlight.rgb, beam);
	color = mix(color, accent.rgb, beam * ribs * 0.35);
	float edge = smoothstep(0.0, 0.1, min(UV.y, 1.0 - UV.y));
	COLOR = vec4(color, edge);
}
)";

const char *EMBER_SHADER = R"(
shader_type canvas_item;
render_mode unshaded;

uniform vec4 low : source_color = vec4(0.18, 0.04, 0.08, 1.0);
uniform vec4 mid : source_color = vec4(1.0, 0.28, 0.18, 1.0);
uniform vec4 high : source_color = vec4(1.0, 0.86, 0.42, 1.0);
uniform float speed = 0.8;

float band(float x, float center, float width) {
	return 1.0 - smoothstep(width, width + 0.12, abs(x - center));
}

void fragment() {
	float x = fract(UV.x * 1.4 - TIME * speed);
	float heat = band(x, 0.22, 0.11) + band(x, 0.62, 0.08) * 0.7;
	float flicker = 0.72 + 0.28 * sin((UV.x * 30.0) + TIME * 8.0);
	vec3 color = mix(low.rgb, mid.rgb, clamp(heat, 0.0, 1.0));
	color = mix(color, high.rgb, clamp((heat - 0.45) * flicker, 0.0, 1.0));
	float edge = smoothstep(0.0, 0.1, min(UV.y, 1.0 - UV.y));
	COLOR = vec4(color, edge);
}
)";

const char *CAUTION_SHADER = R"(
shader_type canvas_item;
render_mode unshaded;

uniform vec4 black : source_color = vec4(0.04, 0.035, 0.025, 1.0);
uniform vec4 yellow : source_color = vec4(1.0, 0.78, 0.06, 1.0);
uniform float speed = 0.85;
uniform float stripe_count = 9.0;

void fragment() {
	float diagonal = UV.x * stripe_count + UV.y * stripe_count * 0.65 - TIME * speed;
	float stripe = step(0.5, fract(diagonal));
	vec3 color = mix(black.rgb, yellow.rgb, stripe);
	float bevel = smoothstep(0.0, 0.12, min(UV.y, 1.0 - UV.y));
	float sheen = 0.88 + 0.12 * smoothstep(0.15, 0.85, UV.y);
	COLOR = vec4(color * sheen, bevel);
}
)";

} // namespace

String AINextMarqueeSettings::_get_preset_path() {
	return "ai_agent/next_marquee/preset";
}

String AINextMarqueeSettings::_get_custom_shader_path() {
	return "ai_agent/next_marquee/custom_shader";
}

String AINextMarqueeSettings::_get_default_preset_id() {
	return "aurora";
}

String AINextMarqueeSettings::_get_default_custom_shader_code() {
	return String(AURORA_SHADER).strip_edges() + "\n";
}

Vector<AINextMarqueePreset> AINextMarqueeSettings::get_presets() {
	Vector<AINextMarqueePreset> presets;

	AINextMarqueePreset aurora;
	aurora.id = "aurora";
	aurora.display_name = "Aurora";
	aurora.shader_code = String(AURORA_SHADER).strip_edges() + "\n";
	presets.push_back(aurora);

	AINextMarqueePreset signal;
	signal.id = "signal";
	signal.display_name = "Signal";
	signal.shader_code = String(SIGNAL_SHADER).strip_edges() + "\n";
	presets.push_back(signal);

	AINextMarqueePreset ember;
	ember.id = "ember";
	ember.display_name = "Ember";
	ember.shader_code = String(EMBER_SHADER).strip_edges() + "\n";
	presets.push_back(ember);

	AINextMarqueePreset caution;
	caution.id = "caution";
	caution.display_name = "Caution";
	caution.shader_code = String(CAUTION_SHADER).strip_edges() + "\n";
	presets.push_back(caution);

	AINextMarqueePreset custom;
	custom.id = CUSTOM_PRESET_ID;
	custom.display_name = "Custom";
	custom.shader_code = get_custom_shader_code();
	custom.custom = true;
	presets.push_back(custom);

	return presets;
}

AINextMarqueePreset AINextMarqueeSettings::get_preset(const String &p_preset_id) {
	Vector<AINextMarqueePreset> presets = get_presets();
	for (int i = 0; i < presets.size(); i++) {
		if (presets[i].id == p_preset_id) {
			return presets[i];
		}
	}
	return AINextMarqueePreset();
}

bool AINextMarqueeSettings::is_valid_preset_id(const String &p_preset_id) {
	return !get_preset(p_preset_id).id.is_empty();
}

String AINextMarqueeSettings::get_current_preset_id() {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return _get_default_preset_id();
	}

	const String path = _get_preset_path();
	if (!settings->has_setting(path)) {
		return _get_default_preset_id();
	}

	const String preset_id = String(settings->get(path));
	return is_valid_preset_id(preset_id) ? preset_id : _get_default_preset_id();
}

bool AINextMarqueeSettings::set_current_preset_id(const String &p_preset_id) {
	if (!is_valid_preset_id(p_preset_id)) {
		return false;
	}

	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL_V(settings, false);
	settings->set(_get_preset_path(), p_preset_id);
	return true;
}

String AINextMarqueeSettings::get_custom_shader_code() {
	EditorSettings *settings = EditorSettings::get_singleton();
	if (!settings) {
		return _get_default_custom_shader_code();
	}

	const String path = _get_custom_shader_path();
	if (!settings->has_setting(path)) {
		return _get_default_custom_shader_code();
	}

	const String shader_code = String(settings->get(path));
	return shader_code.strip_edges().is_empty() ? _get_default_custom_shader_code() : shader_code;
}

bool AINextMarqueeSettings::set_custom_shader_code(const String &p_shader_code) {
	if (p_shader_code.strip_edges().is_empty()) {
		return false;
	}

	EditorSettings *settings = EditorSettings::get_singleton();
	ERR_FAIL_NULL_V(settings, false);
	settings->set(_get_custom_shader_path(), p_shader_code);
	return true;
}

String AINextMarqueeSettings::get_effective_shader_code() {
	const String preset_id = get_current_preset_id();
	if (preset_id == CUSTOM_PRESET_ID) {
		return get_custom_shader_code();
	}

	AINextMarqueePreset preset = get_preset(preset_id);
	if (preset.id.is_empty() || preset.shader_code.strip_edges().is_empty()) {
		preset = get_preset(_get_default_preset_id());
	}
	return preset.shader_code;
}
