/*
 * @FilePath: \editor\ai_settings_dialog.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-13 16:50:54
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-14 17:48:07
 */
#ifndef AI_SETTINGS_DIALOG_H
#define AI_SETTINGS_DIALOG_H

#include "scene/gui/dialogs.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/check_button.h"
#include "scene/gui/grid_container.h"  // 添加GridContainer头文件
#include "scene/gui/box_container.h"   // 添加BoxContainer头文件

class AISettingsDialog : public ConfirmationDialog {
    GDCLASS(AISettingsDialog, ConfirmationDialog);

private:
    struct ProviderSettings {
        LineEdit *api_key_input;
        LineEdit *url_input;
        HashMap<String, CheckButton*> model_toggles;
    };

    ScrollContainer *scroll_container;
    VBoxContainer *main_container;
    HashMap<String, ProviderSettings> provider_settings;

    static inline AISettingsDialog *singleton = nullptr;

    void _save_settings();
    void _add_provider_section(const String &p_provider_name, 
                            const String &p_api_key_setting,
                            const String &p_url_setting,
                            const Vector<String> &p_models,
                            const String &p_default_url = "");

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    AISettingsDialog();

    static AISettingsDialog* get_singleton();
};

#endif // AI_SETTINGS_DIALOG_H