/**************************************************************************/
/*  ai_change_review_panel.cpp                                             */
/**************************************************************************/

#include "ai_change_review_panel.h"

#include "core/object/callable_mp.h"
#include "editor/ai_component/ui/ai_text_diff_viewer.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "servers/text/text_server.h"

void AIChangeReviewPanel::_bind_methods() {
}

AIChangeReviewPanel::AIChangeReviewPanel() {
	store = AIChangeSetStore::get_singleton();
	if (store.is_valid()) {
		store->connect("changed", callable_mp(this, &AIChangeReviewPanel::_store_changed), CONNECT_DEFERRED);
	}

	set_h_size_flags(Control::SIZE_EXPAND_FILL);

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_theme_constant_override("separation", 4 * EDSCALE);
	add_child(root);

	HBoxContainer *header = memnew(HBoxContainer);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_child(header);

	title_label = memnew(Label);
	title_label->set_text(TTR("AI Changes"));
	title_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_label->add_theme_font_size_override(SceneStringName(font_size), int(12 * EDSCALE));
	header->add_child(title_label);

	Button *refresh_button = memnew(Button);
	refresh_button->set_text(TTR("Refresh"));
	refresh_button->connect(SceneStringName(pressed), callable_mp(this, &AIChangeReviewPanel::refresh));
	header->add_child(refresh_button);

	content = memnew(VBoxContainer);
	content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	content->add_theme_constant_override("separation", 2 * EDSCALE);
	root->add_child(content);

	empty_label = memnew(Label);
	empty_label->set_text(TTR("No pending AI changes."));
	empty_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
	empty_label->set_modulate(Color(1, 1, 1, 0.65));
	content->add_child(empty_label);

	diff_dialog = memnew(AcceptDialog);
	diff_dialog->set_title(TTR("AI Change Diff"));
	diff_dialog->set_min_size(Size2(1040, 680) * EDSCALE);
	add_child(diff_dialog);

	diff_viewer = memnew(AITextDiffViewer);
	diff_dialog->add_child(diff_viewer);

	error_dialog = memnew(AcceptDialog);
	error_dialog->set_title(TTR("AI Change Error"));
	add_child(error_dialog);

	revert_dialog = memnew(ConfirmationDialog);
	revert_dialog->set_title(TTR("Revert AI Change"));
	revert_dialog->set_ok_button_text(TTR("Revert"));
	revert_dialog->set_cancel_button_text(TTR("Cancel"));
	revert_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AIChangeReviewPanel::_confirm_revert_change_set));
	add_child(revert_dialog);

	_refresh();
}

void AIChangeReviewPanel::_store_changed() {
	_refresh();
}

void AIChangeReviewPanel::refresh() {
	_refresh();
}

void AIChangeReviewPanel::_refresh() {
	if (!content || store.is_null()) {
		return;
	}

	while (content->get_child_count() > 0) {
		Node *child = content->get_child(0);
		content->remove_child(child);
		child->queue_free();
	}

	Array changes = store->list_change_sets("pending");
	title_label->set_text(vformat(TTR("AI Changes (%d)"), changes.size()));
	set_visible(!changes.is_empty());
	if (changes.is_empty()) {
		empty_label = memnew(Label);
		empty_label->set_text(TTR("No pending AI changes."));
		empty_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		empty_label->set_modulate(Color(1, 1, 1, 0.65));
		content->add_child(empty_label);
		return;
	}

	for (int i = 0; i < changes.size(); i++) {
		if (Variant(changes[i]).get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary change_set = changes[i];
		const String id = change_set.get("id", String());
		Array file_changes = change_set.get("changes", Array());
		String path = TTR("Unknown file");
		if (!file_changes.is_empty() && Variant(file_changes[0]).get_type() == Variant::DICTIONARY) {
			Dictionary file_change = file_changes[0];
			path = file_change.get("path", path);
		}

		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_theme_constant_override("separation", 4 * EDSCALE);
		content->add_child(row);

		Label *path_label = memnew(Label);
		path_label->set_text(path);
		path_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		path_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		path_label->add_theme_font_size_override(SceneStringName(font_size), int(11 * EDSCALE));
		path_label->set_tooltip_text(path);
		row->add_child(path_label);

		Button *view_button = memnew(Button);
		view_button->set_button_icon(get_editor_theme_icon(SNAME("Diff")));
		view_button->set_tooltip_text(TTR("View Diff"));
		view_button->connect(SceneStringName(pressed), callable_mp(this, &AIChangeReviewPanel::_view_change_set).bind(id), CONNECT_DEFERRED);
		row->add_child(view_button);

		Button *keep_button = memnew(Button);
		keep_button->set_button_icon(get_editor_theme_icon(SNAME("Keep")));
		keep_button->set_tooltip_text(TTR("Keep Change"));
		keep_button->connect(SceneStringName(pressed), callable_mp(this, &AIChangeReviewPanel::_keep_change_set).bind(id), CONNECT_DEFERRED);
		row->add_child(keep_button);

		Button *revert_button = memnew(Button);
		revert_button->set_button_icon(get_editor_theme_icon(SNAME("Reject")));
		revert_button->set_tooltip_text(TTR("Revert Change"));
		revert_button->connect(SceneStringName(pressed), callable_mp(this, &AIChangeReviewPanel::_revert_change_set).bind(id), CONNECT_DEFERRED);
		row->add_child(revert_button);
	}
}

void AIChangeReviewPanel::_view_change_set(const String &p_change_set_id) {
	if (store.is_null() || !diff_dialog || !diff_viewer) {
		return;
	}

	Dictionary change_set = store->get_change_set(p_change_set_id);
	if (change_set.is_empty()) {
		_show_error(TTR("AI change was not found."));
		return;
	}
	diff_dialog->set_title(TTR("AI Change Diff"));
	diff_viewer->set_change_set(change_set);
	diff_dialog->popup_centered_ratio(0.82);
}

void AIChangeReviewPanel::_keep_change_set(const String &p_change_set_id) {
	if (store.is_null()) {
		return;
	}
	String error;
	if (!store->keep_change_set(p_change_set_id, error)) {
		_show_error(error);
	}
	_refresh();
}

void AIChangeReviewPanel::_revert_change_set(const String &p_change_set_id) {
	pending_revert_change_set_id = p_change_set_id;
	if (!revert_dialog) {
		_confirm_revert_change_set();
		return;
	}
	revert_dialog->set_text(TTR("Revert this AI change?\n\nThis restores the file content captured before the AI edit when it is still safe to do so."));
	revert_dialog->popup_centered();
}

void AIChangeReviewPanel::_confirm_revert_change_set() {
	if (store.is_null() || pending_revert_change_set_id.is_empty()) {
		pending_revert_change_set_id.clear();
		return;
	}
	String error;
	if (!store->revert_change_set(pending_revert_change_set_id, error)) {
		_show_error(error);
	}
	pending_revert_change_set_id.clear();
	_refresh();
}

void AIChangeReviewPanel::_show_error(const String &p_error) {
	if (!error_dialog) {
		return;
	}
	error_dialog->set_text(p_error);
	error_dialog->popup_centered_ratio(0.45);
}
