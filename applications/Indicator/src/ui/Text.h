#ifndef INDICATOR_TEXT_H
#define INDICATOR_TEXT_H
#include <wx/string.h>

#include <string>

/// The wxString for UTF-8 `utf8`; every text the model hands to the UI goes through here.
inline wxString wx(std::string const& utf8) {
    return wxString::FromUTF8(utf8.data(), utf8.size());
}

/// `s` as UTF-8, for the model.
inline std::string utf8(wxString const& s) {
    return s.utf8_string();
}

#endif  // INDICATOR_TEXT_H
