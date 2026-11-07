#ifndef INDICATOR_THEME_H
#define INDICATOR_THEME_H
#include <wx/bmpbndl.h>
#include <wx/colour.h>
#include <wx/window.h>
#include <wx/string.h>

// the controls and dialogs of the theme, for callers that include only this header
#include "ui/FileDialog.h"
#include "ui/Key.h"

/// The colours of a theme, from the Qt Indicator's dialog stylesheet: the
/// panel and the words on it, the fields and lists, their frame and the frame
/// of the field in use, a selection, the green of a result line, and the keys'
/// text (the keys are dark in both themes).
struct Palette {
    wxColour panel, text, field, field_text, frame, focus, selection, selection_text, result, key_text, key_hover;
};
/// The light grey panel (`light`) or the black one.
Palette palette(bool light);

/// The theme in use: light panels (true) or black ones (the default). The
/// dialogs opened from then on follow it.
bool light_theme();
void set_light_theme(bool light);

/// Colours `top` and every window below it: panel, fields and lists, the drawn
/// keys' surround. Native controls take the colours the toolkit lets them take.
void apply_theme(wxWindow* top, bool light);

/// A tool icon of the theme ("back", "forward", "up", "new folder", "link",
/// "list", "details", "moon", "sun", "help", or "ok" and "cancel": a check
/// mark and a "no" sign), a line drawing `size` px square at every scale, in
/// the keys' green or `colour`.
wxBitmapBundle tinted_icon(wxString const& name, wxColour const& colour = wxColour(0x47, 0xFF, 0x86), int size = 20);

/// A warning in a dialog with the theme and one OK key.
void warn(wxWindow* parent, wxString const& title, wxString const& message);

/// The green of a dialog's result lines as "#rrggbb": the readout's on black,
/// a darker one that reads on the light panel.
wxString result_colour(bool light);

/// A question in a dialog with the theme and two keys, Yes and No; true for Yes.
bool confirm(wxWindow* parent, wxString const& title, wxString const& message);

/// The layout direction of the current language.
wxLayoutDirection layout_direction();

#endif  // INDICATOR_THEME_H
