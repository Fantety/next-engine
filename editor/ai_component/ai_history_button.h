/*
 * @FilePath: \editor\ai_component\ai_history_button.h
 * @Author: Fantety
 * @Descripttion: 
 * @Date: 2025-07-10 18:34:02
 * @LastEditors: Fantety
 * @LastEditTime: 2025-07-11 18:00:00
 */
#ifndef AI_HISTORY_BUTTON
#define AI_HISTORY_BUTTON

#include "core/object/callable_mp.h"
#include "scene/gui/button.h"


class AIHistoryButton: public Button{
    GDCLASS(AIHistoryButton, Button);
private:
    String uuid;

public:
    AIHistoryButton(){
        connect("pressed", callable_mp(this, &AIHistoryButton::history_button_pressed));
    }
    AIHistoryButton(String uuid){
        this->uuid = uuid;
        connect("pressed", callable_mp(this, &AIHistoryButton::history_button_pressed));
    }

    void set_uuid(const String& p_uuid){
        uuid = p_uuid;
    }
    String get_uuid() const{
        return uuid;
    }

    static void _bind_methods(){
        ADD_SIGNAL(MethodInfo("history_button_pressed", PropertyInfo(Variant::STRING, "uuid")));
    }

    void history_button_pressed(){
        call_deferred("emit_signal", SNAME("history_button_pressed"), uuid);
    }

};
#endif
