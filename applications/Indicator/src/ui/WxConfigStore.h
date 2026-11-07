#ifndef INDICATOR_WXCONFIGSTORE_H
#define INDICATOR_WXCONFIGSTORE_H
#include "model/SettingsStore.h"

#include <filesystem>
#include <memory>

class wxFileConfig;

/// The settings in an INI file (wxFileConfig). Keys like "camera/distance_mm"
/// are a group and a name. A file that cannot be written is not reported: the
/// values stay in memory for the session.
class WxConfigStore final : public SettingsStore {
public:
    explicit WxConfigStore(std::filesystem::path ini);
    ~WxConfigStore() override;
    std::optional<std::string> value(std::string const& key) const override;
    void set_value(std::string const& key, std::string const& value) override;

private:
    std::unique_ptr<wxFileConfig> config_;
};

/// `<wxStandardPaths::GetUserConfigDir()>/Contorno/Indicator/indicator.ini`; on Linux in the XDG
/// layout, which this sets: `$XDG_CONFIG_HOME` or `~/.config`.
std::filesystem::path default_settings_file();

#endif  // INDICATOR_WXCONFIGSTORE_H
