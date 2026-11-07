#ifndef INDICATOR_INDICATORSETTINGS_H
#define INDICATOR_INDICATORSETTINGS_H
#include "model/Measure.h"
#include "model/SettingsStore.h"

#include <string>

/// What Indicator remembers between sessions.
struct IndicatorSettings {
    Vision::CameraSettings camera {};
    double min_area_mm2 {100};
    std::string model_path {};  ///< ai/model_path: read only, never written
    /// dialogs/last_folder: where the file dialogs start, the folder of the
    /// last file imported, opened or saved. Empty until one is stored; the UI
    /// then starts in the samples folder or the home folder.
    std::string last_folder {};
    std::string language {"en"};  ///< ui/language: a code of language_codes(); unknown reads as "en"
    bool light {false};  ///< ui/theme: "light" for light panels, anything else is "dark"
};

/// The settings in `store`; defaults for what is absent or invalid. An old
/// distance / field-of-view camera is converted; the minimum area is snapped
/// to its presets.
IndicatorSettings load_indicator_settings(SettingsStore const& store);
/// Writes every key but ai/model_path. Numbers in their shortest round-trip form.
void save_indicator_settings(SettingsStore& store, IndicatorSettings const& settings);

#endif  // INDICATOR_INDICATORSETTINGS_H
