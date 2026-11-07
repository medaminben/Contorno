#ifndef INDICATOR_FILEDIALOG_H
#define INDICATOR_FILEDIALOG_H
#include <wx/string.h>

class wxWindow;

/// The file to import or open, chosen in a dialog with the theme (a generic
/// one: the system's cannot be themed), starting in `folder`, its tool icons
/// green. `filter` is "Images (*.jpg *.png)", pairs separated by ";;", or a
/// wx wildcard. Empty when cancelled.
wxString choose_open_file(wxWindow* parent, wxString const& title, wxString const& folder,
                          wxString const& filter);

/// The file to save to, as choose_open_file, starting at `suggested` (a folder
/// and a file name). `suffix` (no dot) is added to a name typed without one.
wxString choose_save_file(wxWindow* parent, wxString const& title, wxString const& suggested,
                          wxString const& filter, wxString const& suffix = {});

#endif  // INDICATOR_FILEDIALOG_H
