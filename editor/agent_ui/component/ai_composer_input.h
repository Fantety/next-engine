/**************************************************************************/
/*  ai_composer_input.h                                                   */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "scene/gui/panel_container.h"

class AIReferenceTextEdit;
class HFlowContainer;
class TextEdit;

class AIComposerInput : public PanelContainer {
	GDCLASS(AIComposerInput, PanelContainer);

	AIReferenceTextEdit *text_edit = nullptr;
	HFlowContainer *references_flow = nullptr;
	Array references;
	Callable changed_callback;
	bool applying_theme = false;

	void _on_text_changed();
	void _remove_reference(int p_index);
	void _refresh_references();
	void _apply_theme();
	bool _append_reference(const Dictionary &p_attachment);
	void _emit_changed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIComposerInput();

	TextEdit *get_text_edit() const;
	void set_changed_callback(const Callable &p_callback);
	String get_text() const;
	void clear();
	bool has_references() const;
	Array get_references() const;
	Array get_attachments_for_send() const;

	bool add_reference_path(const String &p_path);
	bool add_clipboard_reference();
	bool add_canvas_reference();
};
