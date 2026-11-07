#ifndef INDICATOR_APPICON_H
#define INDICATOR_APPICON_H
#include <wx/iconbndl.h>

class wxTopLevelWindow;

/// The Contorno C without background: white for the dark scheme, black
/// otherwise. Each bundle holds the SVG's renderings and the 256 px PNG.
wxIconBundle app_icon(bool dark_scheme);

/// Sets the window's icons for the system's colour scheme and again whenever
/// the scheme changes.
void follow_system_scheme(wxTopLevelWindow* window);

#endif  // INDICATOR_APPICON_H
