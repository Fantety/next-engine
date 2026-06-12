/**************************************************************************/
/*  ai_settings_placeholder_page.cpp                                       */
/**************************************************************************/

#include "ai_settings_placeholder_page.h"

#include "core/object/class_db.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/label.h"

void AISettingsPlaceholderPage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_placeholder_text", "text"), &AISettingsPlaceholderPage::set_placeholder_text);
}

AISettingsPlaceholderPage::AISettingsPlaceholderPage() {
	add_theme_constant_override("margin_left", 16 * EDSCALE);
	add_theme_constant_override("margin_right", 16 * EDSCALE);
	add_theme_constant_override("margin_top", 16 * EDSCALE);
	add_theme_constant_override("margin_bottom", 16 * EDSCALE);
}

void AISettingsPlaceholderPage::set_placeholder_text(const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(label);
}
