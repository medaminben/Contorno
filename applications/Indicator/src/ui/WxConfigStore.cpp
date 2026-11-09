#include "ui/WxConfigStore.h"

#include "model/DataFolder.h"
#include "model/Utf8Path.h"
#include "ui/Text.h"

#include <wx/fileconf.h>
#include <wx/log.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>

#include <system_error>

WxConfigStore::WxConfigStore(std::filesystem::path ini) {
    std::error_code ignored;
    if (ini.has_parent_path())
        std::filesystem::create_directories(ini.parent_path(), ignored);
    wxLogNull const quiet;
    config_ =
        std::make_unique<wxFileConfig>(wxEmptyString, wxEmptyString, wx(utf8(ini)), wxEmptyString,
                                       wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_NO_ESCAPE_CHARACTERS);
}

WxConfigStore::~WxConfigStore() {
    wxLogNull const quiet;  // the final write may fail like the others
    config_.reset();
}

std::optional<std::string> WxConfigStore::value(std::string const& key) const {
    wxLogNull const quiet;
    wxString text;
    if (!config_->Read(wx(key), &text))
        return std::nullopt;
    return utf8(text);
}

void WxConfigStore::set_value(std::string const& key, std::string const& value) {
    wxLogNull const quiet;
    config_->Write(wx(key), wx(value));
    config_->Flush();
}

std::filesystem::path default_settings_file() {
#if defined(__UNIX__) && !defined(__APPLE__)
    // $XDG_CONFIG_HOME (~/.config), not a folder in the home folder itself
    wxStandardPaths::Get().SetFileLayout(wxStandardPaths::FileLayout_XDG);
#endif
    return path_from_utf8(utf8(wxStandardPaths::Get().GetUserConfigDir())) / "Contorno" /
           "Indicator" / "indicator.ini";
}

std::filesystem::path user_data_folder() {
    wxString chosen;
    if (wxGetEnv("CONTORNO_DATA_FOLDER", &chosen) && !chosen.empty())
        return path_from_utf8(utf8(chosen));
    auto const& paths = wxStandardPaths::Get();
    return data_folder(path_from_utf8(utf8(paths.GetExecutablePath())).parent_path(),
                       path_from_utf8(utf8(paths.GetDocumentsDir())));
}
