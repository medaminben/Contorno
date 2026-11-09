#include "ui/Localise.h"

#include <wx/app.h>
#include <wx/window.h>

#include <unordered_set>
#include <vector>

#ifdef INDICATOR_GTK3
#include <gtk/gtk.h>
#endif

#include "model/Language.h"
#include "ui/Catalogue.h"

wxDEFINE_EVENT(EVT_LANGUAGE_CHANGED, wxCommandEvent);

namespace {

/// The layout direction and the event to `window` and its descendants, each once (a dialog is both
/// top-level and its owner's child).
void send_to(wxWindow* window, wxLayoutDirection direction, std::unordered_set<wxWindow*>& done) {
    if (!done.insert(window).second)
        return;
    window->SetLayoutDirection(direction);  // children do not inherit it from the parent
    wxCommandEvent event(EVT_LANGUAGE_CHANGED, window->GetId());
    event.SetEventObject(window);
    event.StopPropagation();  // a command event goes up to the parent by default: it would be
                              // counted twice
    window->GetEventHandler()->ProcessEvent(event);
    std::vector<wxWindow*> const children(window->GetChildren().begin(),
                                          window->GetChildren().end());
    for (wxWindow* child : children)
        send_to(child, direction, done);
}

/// Destroy() on a top-level window, or on its owner, deletes it when idle; until then it stays in
/// wxTopLevelWindows, and its handlers may refer to an owner that is already gone.
bool waiting_for_deletion(wxWindow* window) {
    for (; window; window = window->GetParent()) {
        if (window->IsBeingDeleted() || wxPendingDelete.Member(window))
            return true;
    }
    return false;
}

}  // namespace

wxLayoutDirection language_layout_direction() {
    return text_direction() == TextDirection::right_to_left ? wxLayout_RightToLeft
                                                            : wxLayout_LeftToRight;
}

void apply_language(std::string const& code) {
    set_current_language(code);
    select_wx_catalogue(current_language());
    wxLayoutDirection const direction = language_layout_direction();
#ifdef INDICATOR_GTK3
    gtk_widget_set_default_direction(direction == wxLayout_RightToLeft ? GTK_TEXT_DIR_RTL
                                                                       : GTK_TEXT_DIR_LTR);
#endif
    std::vector<wxWindow*> const tops(wxTopLevelWindows.begin(), wxTopLevelWindows.end());
    std::unordered_set<wxWindow*> done;
    for (wxWindow* top : tops) {
        if (!waiting_for_deletion(top))
            send_to(top, direction, done);
    }
}
