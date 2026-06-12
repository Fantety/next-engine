/**************************************************************************/
/*  ai_composer_input.cpp                                                 */
/**************************************************************************/

#include "ai_composer_input.h"

#include "core/core_bind.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_mp.h"
#include "editor/agent_ui/component/ai_reference_resolver.h"
#include "editor/agent_ui/component/ai_reference_text_edit.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/texture.h"
#include "servers/text/text_server.h"

namespace {

constexpr int PREVIEW_SIZE = 56;
constexpr int CHIP_MAX_WIDTH = 108;
constexpr int REFERENCE_GAP = 4;
constexpr int IMAGE_CHIP_RADIUS = 6;
constexpr int TEXT_CHIP_RADIUS = 4;

Ref<StyleBoxFlat> _make_rounded_style(const Color &p_bg, const Color &p_border, int p_radius, int p_border_width = 1) {
	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(p_bg);
	style->set_border_color(p_border);
	style->set_border_width_all(p_border_width);
	style->set_corner_radius_all(p_radius);
	style->set_content_margin_all(0);
	return style;
}

Ref<StyleBoxFlat> _make_input_style(const Control *p_control, bool p_focus) {
	const Color base = p_control->get_theme_color(SNAME("dark_color_2"), EditorStringName(Editor));
	const Color accent = p_control->get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	Color bg = base;
	bg.a = 0.92;
	Color border = accent.lerp(p_control->get_theme_color(SNAME("font_color"), EditorStringName(Editor)), p_focus ? 0.18 : 0.58);
	border.a = p_focus ? 0.9 : 0.36;
	return _make_rounded_style(bg, border, 8 * EDSCALE, p_focus ? 2 * EDSCALE : 1 * EDSCALE);
}

Ref<StyleBoxFlat> _make_chip_style(const Control *p_control, bool p_image) {
	const Color accent = p_control->get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	const Color base = p_control->get_theme_color(SNAME("dark_color_1"), EditorStringName(Editor));
	Color bg = accent.lerp(base, p_image ? 0.86 : 0.78);
	bg.a = p_image ? 0.72 : 0.68;
	Color border = accent;
	border.a = p_image ? 0.36 : 0.42;
	return _make_rounded_style(bg, border, p_image ? IMAGE_CHIP_RADIUS * EDSCALE : TEXT_CHIP_RADIUS * EDSCALE);
}

String _data_url_mime(const String &p_data_url) {
	if (!p_data_url.begins_with("data:")) {
		return String();
	}
	const int comma = p_data_url.find(",");
	if (comma < 0) {
		return String();
	}
	const String metadata = p_data_url.substr(5, comma - 5).to_lower();
	const int semicolon = metadata.find(";");
	return semicolon >= 0 ? metadata.substr(0, semicolon) : metadata;
}

PackedByteArray _data_url_bytes(const String &p_data_url) {
	const int comma = p_data_url.find(",");
	if (comma < 0) {
		return PackedByteArray();
	}
	CoreBind::Marshalls *marshalls = CoreBind::Marshalls::get_singleton();
	if (!marshalls) {
		return PackedByteArray();
	}
	return marshalls->base64_to_raw(p_data_url.substr(comma + 1));
}

Ref<Texture2D> _texture_from_data_url(const String &p_data_url) {
	const String mime = _data_url_mime(p_data_url);
	const PackedByteArray bytes = _data_url_bytes(p_data_url);
	if (mime.is_empty() || bytes.is_empty()) {
		return Ref<Texture2D>();
	}

	Ref<Image> image;
	image.instantiate();
	Error err = ERR_UNAVAILABLE;
	if (mime == "image/png") {
		err = image->load_png_from_buffer(bytes);
	} else if (mime == "image/jpeg" || mime == "image/jpg") {
		err = image->load_jpg_from_buffer(bytes);
	} else if (mime == "image/webp") {
		err = image->load_webp_from_buffer(bytes);
	}
	if (err != OK || image->is_empty()) {
		return Ref<Texture2D>();
	}
	return ImageTexture::create_from_image(image);
}

Ref<Texture2D> _texture_from_path(const String &p_path) {
	if (p_path.is_empty()) {
		return Ref<Texture2D>();
	}

	Ref<Texture2D> texture = ResourceLoader::load(p_path, "Texture2D");
	if (texture.is_valid()) {
		return texture;
	}

	Ref<Image> image;
	image.instantiate();
	if (image->load(p_path) != OK || image->is_empty()) {
		return Ref<Texture2D>();
	}
	return ImageTexture::create_from_image(image);
}

Ref<Texture2D> _thumbnail_texture(const Dictionary &p_attachment) {
	const String data_url = String(p_attachment.get("data_url", String()));
	if (!data_url.is_empty()) {
		return _texture_from_data_url(data_url);
	}
	return _texture_from_path(String(p_attachment.get("path", String())));
}

bool _is_image_attachment(const Dictionary &p_attachment) {
	const String type = String(p_attachment.get("type", String())).strip_edges().to_lower();
	const String mime = String(p_attachment.get("mime_type", p_attachment.get("mime", String()))).strip_edges().to_lower();
	return type == "image" || mime.begins_with("image/");
}

void _append_unique(Array &r_attachments, const Dictionary &p_attachment) {
	if (p_attachment.is_empty()) {
		return;
	}
	for (int i = 0; i < r_attachments.size(); i++) {
		if (Variant(r_attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		if (AIReferenceResolver::attachments_equivalent(r_attachments[i], p_attachment)) {
			return;
		}
	}
	r_attachments.push_back(p_attachment);
}

} // namespace

void AIComposerInput::_bind_methods() {
}

void AIComposerInput::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
		_apply_theme();
		_refresh_references();
	} else if (p_what == NOTIFICATION_FOCUS_ENTER || p_what == NOTIFICATION_FOCUS_EXIT) {
		_apply_theme();
	}
}

AIComposerInput::AIComposerInput() {
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_SHRINK_END);
	set_custom_minimum_size(Size2(0, 104) * EDSCALE);

	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override(SNAME("margin_left"), 10 * EDSCALE);
	margin->add_theme_constant_override(SNAME("margin_top"), 8 * EDSCALE);
	margin->add_theme_constant_override(SNAME("margin_right"), 10 * EDSCALE);
	margin->add_theme_constant_override(SNAME("margin_bottom"), 8 * EDSCALE);
	add_child(margin);

	VBoxContainer *content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	margin->add_child(content);

	references_flow = memnew(HFlowContainer);
	references_flow->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	references_flow->add_theme_constant_override(SNAME("h_separation"), REFERENCE_GAP * EDSCALE);
	references_flow->add_theme_constant_override(SNAME("v_separation"), REFERENCE_GAP * EDSCALE);
	references_flow->hide();
	content->add_child(references_flow);

	text_edit = memnew(AIReferenceTextEdit);
	text_edit->set_custom_minimum_size(Size2(0, 72) * EDSCALE);
	text_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	text_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	text_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	text_edit->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	text_edit->set_placeholder(TTR("Ask about this project..."));
	text_edit->connect("text_changed", callable_mp(this, &AIComposerInput::_on_text_changed));
	text_edit->connect("focus_entered", callable_mp(this, &AIComposerInput::_apply_theme));
	text_edit->connect("focus_exited", callable_mp(this, &AIComposerInput::_apply_theme));
	content->add_child(text_edit);

	Ref<StyleBoxEmpty> empty_style;
	empty_style.instantiate();
	text_edit->add_theme_style_override(SNAME("normal"), empty_style);
	text_edit->add_theme_style_override(SNAME("focus"), empty_style);
	text_edit->add_theme_style_override(SNAME("read_only"), empty_style);
}

TextEdit *AIComposerInput::get_text_edit() const {
	return text_edit;
}

void AIComposerInput::set_changed_callback(const Callable &p_callback) {
	changed_callback = p_callback;
}

String AIComposerInput::get_text() const {
	return text_edit ? text_edit->get_text() : String();
}

void AIComposerInput::clear() {
	if (text_edit) {
		text_edit->clear();
	}
	references.clear();
	_refresh_references();
	_emit_changed();
}

bool AIComposerInput::has_references() const {
	return !references.is_empty();
}

Array AIComposerInput::get_references() const {
	return references.duplicate(true);
}

Array AIComposerInput::get_attachments_for_send() const {
	Array result = references.duplicate(true);
	const Array inline_attachments = AIReferenceResolver::resolve_attachments(get_text());
	for (int i = 0; i < inline_attachments.size(); i++) {
		if (Variant(inline_attachments[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		_append_unique(result, inline_attachments[i]);
	}
	return result;
}

bool AIComposerInput::add_reference_path(const String &p_path) {
	return _append_reference(AIReferenceResolver::make_reference_attachment(p_path));
}

bool AIComposerInput::add_clipboard_reference() {
	return _append_reference(AIReferenceResolver::make_clipboard_reference_attachment());
}

bool AIComposerInput::add_canvas_reference() {
	return _append_reference(AIReferenceResolver::make_canvas_reference_attachment());
}

void AIComposerInput::_on_text_changed() {
	_emit_changed();
}

void AIComposerInput::_remove_reference(int p_index) {
	if (p_index < 0 || p_index >= references.size()) {
		return;
	}
	references.remove_at(p_index);
	_refresh_references();
	_emit_changed();
}

void AIComposerInput::_refresh_references() {
	if (!references_flow) {
		return;
	}

	while (references_flow->get_child_count() > 0) {
		Node *child = references_flow->get_child(0);
		references_flow->remove_child(child);
		child->queue_free();
	}

	references_flow->set_visible(!references.is_empty());
	for (int i = 0; i < references.size(); i++) {
		if (Variant(references[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}

		const Dictionary attachment = references[i];
		const bool is_image = _is_image_attachment(attachment);
		const String label_text = AIReferenceResolver::make_attachment_label(attachment);

		PanelContainer *chip = memnew(PanelContainer);
		chip->add_theme_style_override(SNAME("panel"), _make_chip_style(this, is_image));
		chip->set_tooltip_text(label_text);
		references_flow->add_child(chip);

		MarginContainer *margin = memnew(MarginContainer);
		margin->add_theme_constant_override(SNAME("margin_left"), is_image ? 2 * EDSCALE : 5 * EDSCALE);
		margin->add_theme_constant_override(SNAME("margin_top"), is_image ? 2 * EDSCALE : 3 * EDSCALE);
		margin->add_theme_constant_override(SNAME("margin_right"), 3 * EDSCALE);
		margin->add_theme_constant_override(SNAME("margin_bottom"), is_image ? 2 * EDSCALE : 3 * EDSCALE);
		chip->add_child(margin);

		HBoxContainer *row = memnew(HBoxContainer);
		row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		margin->add_child(row);

		if (is_image) {
			TextureRect *preview = memnew(TextureRect);
			preview->set_custom_minimum_size(Size2(PREVIEW_SIZE, PREVIEW_SIZE) * EDSCALE);
			preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
			preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
			preview->set_texture(_thumbnail_texture(attachment));
			row->add_child(preview);
		} else {
			Button *icon = memnew(Button);
			icon->set_flat(true);
			icon->set_disabled(true);
			icon->set_button_icon(get_editor_theme_icon(SNAME("Attachment")));
			row->add_child(icon);

			Label *label = memnew(Label);
			label->set_text(label_text);
			label->set_custom_minimum_size(Size2(CHIP_MAX_WIDTH, 0) * EDSCALE);
			label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
			label->set_clip_text(true);
			row->add_child(label);
		}

		Button *remove = memnew(Button);
		remove->set_flat(true);
		remove->set_button_icon(get_editor_theme_icon(SNAME("Close")));
		remove->set_tooltip_text(vformat(TTR("Remove %s"), label_text));
		remove->connect("pressed", callable_mp(this, &AIComposerInput::_remove_reference).bind(i), CONNECT_DEFERRED);
		row->add_child(remove);
	}
}

void AIComposerInput::_apply_theme() {
	if (applying_theme) {
		return;
	}
	applying_theme = true;
	add_theme_style_override(SNAME("panel"), _make_input_style(this, has_focus() || (text_edit && text_edit->has_focus())));
	applying_theme = false;
}

bool AIComposerInput::_append_reference(const Dictionary &p_attachment) {
	if (p_attachment.is_empty()) {
		return false;
	}
	const int before = references.size();
	_append_unique(references, p_attachment);
	if (references.size() == before) {
		return false;
	}
	_refresh_references();
	_emit_changed();
	return true;
}

void AIComposerInput::_emit_changed() {
	if (changed_callback.is_valid()) {
		changed_callback.call();
	}
}
