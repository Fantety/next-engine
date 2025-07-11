#include "ai_accept_dialog.h"

void AIAcceptDialog::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            msg_label = memnew(Label);
            msg_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
            msg_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
            add_child(msg_label);
            set_title("Warning");
            set_min_size(Size2(100, 150));
            set_flag(FLAG_RESIZE_DISABLED, false);
            get_ok_button()->set_text("Confirm");
            get_cancel_button()->set_text("Cancel");
            //connect("confirmed", callable_mp(this, &AIAcceptDialog::on_confirmed));
        } break;
    }
}



void AIAcceptDialog::_bind_methods() {
    //ADD_SIGNAL(MethodInfo("ai_accept_dialog_confirmed"));
}

// void AIAcceptDialog::on_confirmed(){
//     call_deferred("emit_signal", SNAME("ai_accept_dialog_confirmed"));
// }

void AIAcceptDialog::set_dialog_message(const String text){
    msg_label->set_text(text);
}

AIAcceptDialog* AIAcceptDialog::get_singleton(){
    return singleton;
}

AIAcceptDialog::AIAcceptDialog(){
    singleton = this;
}