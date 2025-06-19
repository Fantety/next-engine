/*
 * @FilePath: \editor\ai_dock.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-06-17 19:18:53
 * @LastEditors: Fantety
 * @LastEditTime: 2025-06-18 12:41:59
 */
#ifndef AI_DOCK_H
#define AI_DOCK_H

#include "scene/gui/box_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/button.h"
#include "ai_settings_dialog.h"
#include "plugins/chatapis/deepseek_api.h"


class AIDock : public VBoxContainer {
    GDCLASS(AIDock, VBoxContainer);

private:
    TextEdit *chat_display;
    LineEdit *input_box;
    OptionButton *model_selector;
    Button *send_button;
    Button *settings_button;
    AISettingsDialog *dialog;

    DeepSeekAPI *deepseek_api;
    String current_ai_response;

    void _send_message();
    void _update_model_list();
    void _show_settings();
    void _add_message(const String &message, bool is_user);  // 确保这个方法有声明
    void _add_message_without_flash(const String &text, bool is_user);  // 确保这个方法有声明
    void _handle_ai_response(const String& content, bool isFinal);
    void _handle_request_completed();
    void _handle_error(const String& message);

    void on_stream_response(const String &data);
protected:
    void _notification(int p_notification);
    static void _bind_methods();

public:
    AIDock();
    ~AIDock();


    void set_ai_settings_dialog(AISettingsDialog *i_dialog);
};

#endif // AI_DOCK_H