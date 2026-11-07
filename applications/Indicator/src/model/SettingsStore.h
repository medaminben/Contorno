#ifndef INDICATOR_SETTINGSSTORE_H
#define INDICATOR_SETTINGSSTORE_H
#include <map>
#include <optional>
#include <string>

/// Where settings live: text values under keys like "camera/distance_mm". The
/// UI backs it with an INI file; the model only sees this interface.
class SettingsStore {
public:
    virtual ~SettingsStore() = default;
    /// The stored text; nullopt when the key is absent.
    virtual std::optional<std::string> value(std::string const& key) const = 0;
    virtual void set_value(std::string const& key, std::string const& value) = 0;
};

/// A store in memory, for tests.
class MemorySettingsStore final : public SettingsStore {
public:
    std::optional<std::string> value(std::string const& key) const override;
    void set_value(std::string const& key, std::string const& value) override;

private:
    std::map<std::string, std::string> values_;
};

#endif  // INDICATOR_SETTINGSSTORE_H
