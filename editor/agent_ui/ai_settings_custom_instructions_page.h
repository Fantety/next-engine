/**************************************************************************/
/*  ai_settings_custom_instructions_page.h                                */
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

#pragma once

#include "editor/agent_v1/ui_adapter/ai_agent_v1_ui_bridge.h"
#include "scene/gui/margin_container.h"

class Button;
class Label;
class TextEdit;

class AISettingsCustomInstructionsPage : public MarginContainer {
	GDCLASS(AISettingsCustomInstructionsPage, MarginContainer);

	TextEdit *instructions_edit = nullptr;
	Button *save_button = nullptr;
	Label *status_label = nullptr;
	bool loading = false;

	Ref<AIAgentV1UIBridge> _get_adapter() const;
	void _build_ui();
	void _load_custom_instructions();
	String _get_custom_instructions() const;
	bool _patch_custom_instructions(const String &p_instructions, const String &p_scope = "project");
	void _save_pressed();
	void _config_changed(const String &p_scope, const Dictionary &p_config);
	void _set_status(const String &p_status, bool p_error);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AISettingsCustomInstructionsPage();

	void build_for_test();
	void set_custom_instructions_for_test(const String &p_instructions);
	String get_custom_instructions_for_test() const;
};
