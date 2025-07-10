#ifndef AI_HISTORY_BUTTON
#define AI_HISTORY_BUTTON

#include "scene/gui/button.h"


class AIHistoryButton: public Button{
    GDCLASS(AIHistoryButton, Button);
private:
    String uuid;

public:
    AIHistoryButton(){}
    AIHistoryButton(String uuid){
        this->uuid = uuid;
    }

    void set_uuid(const String& p_uuid){
        uuid = p_uuid;
    }
    String get_uuid() const{
        return uuid;
    }
};



#endif