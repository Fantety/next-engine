/**************************************************************************/
/*  ai_agent_settings_dialog.h                                             */
/**************************************************************************/

#pragma once

#include "core/templates/hash_map.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"

class AIAgentSettingsDialog : public ConfirmationDialog {
	GDCLASS(AIAgentSettingsDialog, ConfirmationDialog);

	enum SettingsPage {
		PAGE_MODELS,
		PAGE_MCP,
		PAGE_SKILLS,
		PAGE_RULES,
	};

	struct ProviderControls {
		String provider_id;
		LineEdit *api_key = nullptr;
		LineEdit *base_url = nullptr;
		Vector<CheckButton *> preset_model_checks;
		Vector<String> preset_models;
		TextEdit *custom_models = nullptr;
	};

	ItemList *navigation = nullptr;
	TabContainer *pages = nullptr;
	Vector<ProviderControls> provider_controls;

	static inline AIAgentSettingsDialog *singleton = nullptr;

	void _build_ui();
	void _build_navigation(HBoxContainer *p_root);
	void _build_pages(HBoxContainer *p_root);
	void _build_models_page(Control *p_page);
	void _add_provider_section(VBoxContainer *p_parent, const String &p_provider_id, const String &p_display_name, const String &p_default_base_url, const Vector<String> &p_models);
	void _add_placeholder_page(Control *p_page, const String &p_title);
	void _navigation_selected(int p_index);
	void _save_settings();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	AIAgentSettingsDialog();
	static AIAgentSettingsDialog *get_singleton();
};
