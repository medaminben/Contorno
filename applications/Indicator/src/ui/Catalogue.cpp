#include "ui/Catalogue.h"

#include <wx/arrstr.h>
#include <wx/intl.h>
#include <wx/translation.h>

#include <cstddef>
#include <cstring>

#ifdef INDICATOR_WX_CATALOGUES
namespace indicator {
struct EmbeddedCatalogue {
    char const* language;
    unsigned char const* data;
    std::size_t size;
};
extern const EmbeddedCatalogue embedded_wx_catalogues[];
extern const std::size_t embedded_wx_catalogue_count;
}  // namespace indicator
#endif

namespace {

bool installed = false;

#ifdef INDICATOR_WX_CATALOGUES
/// Serves the embedded wxstd catalogues. Other domains (wx also asks for the port's own, "gtk")
/// have none.
class EmbeddedLoader : public wxTranslationsLoader {
public:
    wxMsgCatalog* LoadCatalog(wxString const& domain, wxString const& lang) override {
        if (!is_std(domain))
            return nullptr;
        for (std::size_t i = 0; i < indicator::embedded_wx_catalogue_count; ++i) {
            auto const& entry = indicator::embedded_wx_catalogues[i];
            if (lang == entry.language) {
                return wxMsgCatalog::CreateFromData(
                    wxScopedCharBuffer::CreateNonOwned(reinterpret_cast<char const*>(entry.data),
                                                       entry.size),
                    domain);
            }
        }
        return nullptr;
    }
    wxArrayString GetAvailableTranslations(wxString const& domain) const override {
        wxArrayString languages;
        if (!is_std(domain))
            return languages;
        for (std::size_t i = 0; i < indicator::embedded_wx_catalogue_count; ++i) {
            languages.Add(indicator::embedded_wx_catalogues[i].language);
        }
        return languages;
    }

private:
    static bool is_std(wxString const& domain) { return domain.StartsWith("wxstd"); }
};
#endif

}  // namespace

bool wx_catalogues_embedded() {
#ifdef INDICATOR_WX_CATALOGUES
    return indicator::embedded_wx_catalogue_count > 0;
#else
    return false;
#endif
}

void install_wx_catalogues() {
    installed = true;
    select_wx_catalogue("en");
}

void select_wx_catalogue(std::string const& code) {
    if (!installed)
        return;
    // a fresh wxTranslations: wx picks the language when a catalogue is added
    auto* translations = new wxTranslations;
#ifdef INDICATOR_WX_CATALOGUES
    translations->SetLoader(new EmbeddedLoader);
#endif
    translations->SetLanguage(wxString::FromUTF8(code));
    translations->AddStdCatalog();
    wxTranslations::Set(translations);
}
