#ifndef INDICATOR_LOCALISE_H
#define INDICATOR_LOCALISE_H
#include <wx/event.h>
#include <wx/intl.h>

#include <string>

/// Sent by `apply_language` to every window, once each, after the language has changed.
wxDECLARE_EVENT(EVT_LANGUAGE_CHANGED, wxCommandEvent);

/// The layout direction of the current language. wx 3.2 derives wxApp's own
/// from the locale and has no setter, so the application object returns this
/// from its `GetLayoutDirection` override.
wxLayoutDirection language_layout_direction();

/// Makes `code` (unknown: "en") the language of the application: the layout
/// direction of every window (and of the ones made later), then an
/// EVT_LANGUAGE_CHANGED to every top-level window and every descendant, each
/// reached once and the event not passed on to its parent.
void apply_language(std::string const& code);

#endif  // INDICATOR_LOCALISE_H
