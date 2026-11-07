#include "model/SettingsStore.h"

std::optional<std::string> MemorySettingsStore::value(std::string const& key) const {
    auto const found = values_.find(key);
    if (found == values_.end()) return std::nullopt;
    return found->second;
}

void MemorySettingsStore::set_value(std::string const& key, std::string const& value) {
    values_[key] = value;
}
