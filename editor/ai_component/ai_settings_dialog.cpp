/*
 * @FilePath: \editor\ai_component\ai_settings_dialog.cpp
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-10 18:34:02
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-11 15:35:20
 */
#include "ai_settings_dialog.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/label.h"
#include "scene/gui/separator.h"
#include "scene/gui/margin_container.h"

AISettingsDialog* AISettingsDialog::get_singleton() {
	return singleton;
}

void AISettingsDialog::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_save_settings"), &AISettingsDialog::_save_settings);
    ADD_SIGNAL(MethodInfo("ai_settings_changed"));
}

void AISettingsDialog::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            set_title("AI Model Settings");
            set_min_size(Size2(750, 400));
            
            // 使用set_flag代替set_resizable
            set_flag(FLAG_RESIZE_DISABLED, false);  // 启用窗口大小调整

            // Create main scroll container
            scroll_container = memnew(ScrollContainer);
            scroll_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            scroll_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            add_child(scroll_container);

            // Create main container inside scroll
            main_container = memnew(VBoxContainer);
            main_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            scroll_container->add_child(main_container);
            // Add provider sections
            _add_provider_section(
                "DeepSeek",
                "deepseek/api_key",
                "deepseek/url",
                {
                    "deepseek-chat", 
                    "deepseek-reasoner"
                },
                "https://api.deepseek.com/v1"
            );

            _add_provider_section(
                "OpenAI",
                "openai/api_key",
                "openai/url",
                {
                    "gpt-4-turbo",
                    "gpt-4",
                    "gpt-3.5-turbo",
                    "dall-e-3",
                    "whisper",
                    "moderation"
                },
                "https://api.openai.com/v1"
            );

            _add_provider_section(
                "Gemini",
                "gemini/api_key",
                "gemini/url",
                {
                    "gemini-pro",
                    "gemini-ultra", 
                    "gemini-nano",
                    "gemini-vision"
                },
                "https://generativelanguage.googleapis.com/v1"
            );

            // Add more providers as needed...
            _add_provider_section(
                "Anthropic",
                "anthropic/api_key",
                "anthropic/url",
                {
                    "claude-3-opus",
                    "claude-3-sonnet",
                    "claude-2.1"
                },
                "https://api.anthropic.com/v1"
            );

            _add_provider_section(
                "Mistral",
                "mistral/api_key",
                "mistral/url",
                {
                    "mistral-large",
                    "mistral-medium",
                    "mistral-small"
                },
                "https://api.mistral.ai/v1"
            );

            // Add stretch to push content up
            Control *spacer = memnew(Control);
            spacer->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            main_container->add_child(spacer);

            // Configure dialog buttons
            get_ok_button()->set_text("Save");
            get_cancel_button()->set_text("Cancel");
            connect("confirmed", callable_mp(this, &AISettingsDialog::_save_settings));
        } break;
    }
}

// _add_provider_section and _save_settings implementations remain the same as before
// (with the same functionality but now inside a scrollable container)

// _add_provider_section 实现保持不变
void AISettingsDialog::_add_provider_section(const String &p_provider_name, 
                                            const String &p_api_key_setting,
                                            const String &p_url_setting,
                                            const Vector<String> &p_models,
                                            const String &p_default_url) {
    // Provider header
    MarginContainer *header_margin = memnew(MarginContainer);
    header_margin->add_theme_constant_override("margin_left", 5);
    main_container->add_child(header_margin);

    Label *provider_label = memnew(Label);
    provider_label->set_text(p_provider_name);
    provider_label->add_theme_font_override("font", get_theme_font("bold", "EditorFonts"));
    provider_label->add_theme_font_size_override("font_size", 16);
    header_margin->add_child(provider_label);

    // API settings grid
    GridContainer *api_grid = memnew(GridContainer);  // 现在GridContainer已正确定义
    api_grid->set_columns(2);
    api_grid->add_theme_constant_override("h_separation", 10);
    api_grid->add_theme_constant_override("v_separation", 5);
    main_container->add_child(api_grid);

    // API Key
    Label *key_label = memnew(Label);
    key_label->set_text("API Key:");
    api_grid->add_child(key_label);
    
    LineEdit *api_key_input = memnew(LineEdit);
    api_key_input->set_placeholder("Enter your " + p_provider_name + " API key");
    api_key_input->set_secret(true);
    api_key_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    api_grid->add_child(api_key_input);

    // API URL
    Label *url_label = memnew(Label);
    url_label->set_text("API URL:");
    api_grid->add_child(url_label);
    
    LineEdit *url_input = memnew(LineEdit);
    url_input->set_placeholder(p_default_url);
    url_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    api_grid->add_child(url_input);

    // Models header
    Label *models_label = memnew(Label);
    models_label->set_text("Available Models:");
    models_label->add_theme_constant_override("margin_top", 5);
    models_label->add_theme_constant_override("margin_left", 5);
    main_container->add_child(models_label);

    // Models container
    VBoxContainer *models_container = memnew(VBoxContainer);
    models_container->add_theme_constant_override("margin_left", 20);
    main_container->add_child(models_container);

    // Add model toggles
    for (const String &model_name : p_models) {
        HBoxContainer *model_row = memnew(HBoxContainer);
        models_container->add_child(model_row);

        CheckButton *model_toggle = memnew(CheckButton);
        model_toggle->set_text(model_name);
        model_toggle->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        model_row->add_child(model_toggle);

        // Load saved state
        String setting_path = p_provider_name.to_lower() + "/models/" + model_name;
        if (EditorSettings::get_singleton()->has_setting(setting_path)) {
            model_toggle->set_pressed(EditorSettings::get_singleton()->get(setting_path));
        } else {
            model_toggle->set_pressed(false); // Default enabled
        }

        provider_settings[p_provider_name].model_toggles[model_name] = model_toggle;
    }

    // Load saved API settings
    if (EditorSettings::get_singleton()->has_setting(p_api_key_setting)) {
        api_key_input->set_text(EditorSettings::get_singleton()->get(p_api_key_setting));
    }
    if (EditorSettings::get_singleton()->has_setting(p_url_setting)) {
        url_input->set_text(EditorSettings::get_singleton()->get(p_url_setting));
    } else if (!p_default_url.is_empty()) {
        url_input->set_text(p_default_url);
    }

    // Store control references
    ProviderSettings &settings = provider_settings[p_provider_name];
    settings.api_key_input = api_key_input;
    settings.url_input = url_input;

    // Add separator
    main_container->add_child(memnew(HSeparator));
}

void AISettingsDialog::_save_settings() {
    // Save DeepSeek settings
    if (provider_settings.has("DeepSeek")) {
        const ProviderSettings &settings = provider_settings["DeepSeek"];
        EditorSettings::get_singleton()->set("deepseek/api_key", settings.api_key_input->get_text().strip_edges());
        EditorSettings::get_singleton()->set("deepseek/url", settings.url_input->get_text().strip_edges());
        
        for (const KeyValue<String, CheckButton*> &model : settings.model_toggles) {
            EditorSettings::get_singleton()->set("deepseek/models/" + model.key, model.value->is_pressed());
        }
    }
    
    // Save OpenAI settings
    if (provider_settings.has("OpenAI")) {
        const ProviderSettings &settings = provider_settings["OpenAI"];
        EditorSettings::get_singleton()->set("openai/api_key", settings.api_key_input->get_text().strip_edges());
        EditorSettings::get_singleton()->set("openai/url", settings.url_input->get_text().strip_edges());
        
        for (const KeyValue<String, CheckButton*> &model : settings.model_toggles) {
            EditorSettings::get_singleton()->set("openai/models/" + model.key, model.value->is_pressed());
        }
    }
    
    // Save Gemini settings
    if (provider_settings.has("Gemini")) {
        const ProviderSettings &settings = provider_settings["Gemini"];
        EditorSettings::get_singleton()->set("gemini/api_key", settings.api_key_input->get_text().strip_edges());
        EditorSettings::get_singleton()->set("gemini/url", settings.url_input->get_text().strip_edges());
        
        for (const KeyValue<String, CheckButton*> &model : settings.model_toggles) {
            EditorSettings::get_singleton()->set("gemini/models/" + model.key, model.value->is_pressed());
        }
    }
    
    EditorSettings::get_singleton()->save();
    emit_signal("ai_settings_changed");
}

AISettingsDialog::AISettingsDialog() {
    singleton = this;
}
