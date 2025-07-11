#ifndef AI_ACCEPT_DIALOG
#define AI_ACCEPT_DIALOG

#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"

class AIAcceptDialog : public ConfirmationDialog {
    GDCLASS(AIAcceptDialog, ConfirmationDialog);

    static inline AIAcceptDialog *singleton = nullptr;

    Label* msg_label = nullptr;

protected:
    static void _bind_methods();
    void _notification(int p_what);

    //void on_confirmed();

public:
    AIAcceptDialog();
    static AIAcceptDialog* get_singleton();
    void set_dialog_message(const String text);
};



#endif