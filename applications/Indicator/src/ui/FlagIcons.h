#ifndef INDICATOR_FLAGICONS_H
#define INDICATOR_FLAGICONS_H
#include <wx/bmpbndl.h>

#include <string>

/// The flag for a language code, a disc 24 px across at scale 1 and sharp at
/// any other size, from embedded SVG rather than shipped artwork: "en" the
/// United Kingdom, "de" Germany, "es" Spain (its bands only: the arms are lost
/// at this size), "it" Italy, "fr" France, "nl" the Netherlands, "ar" (Arabic,
/// Tunisian) Tunisia. With `disabled` it is the dimmed variant (45 % opacity, as the
/// Qt Indicator: a grey flag no longer says which language); the caller sets it
/// as the disabled bitmap of its button. An empty bundle for any other code.
wxBitmapBundle flag_icon(std::string const& code, bool disabled = false);

#endif  // INDICATOR_FLAGICONS_H
