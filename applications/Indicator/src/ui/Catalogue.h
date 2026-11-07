#ifndef INDICATOR_CATALOGUE_H
#define INDICATOR_CATALOGUE_H
#include <string>

/// Makes the wx catalogues embedded in the executable (wxstd: the file
/// dialog's texts) the ones wx translates from, for English. Call once after
/// the application object exists; `select_wx_catalogue` then follows the language.
void install_wx_catalogues();

/// Switches wx's own texts to `code` (a language of the application; one with no embedded catalogue: English).
/// Does nothing before `install_wx_catalogues`.
void select_wx_catalogue(std::string const& code);

/// True when the build embedded at least one catalogue.
bool wx_catalogues_embedded();

#endif  // INDICATOR_CATALOGUE_H
