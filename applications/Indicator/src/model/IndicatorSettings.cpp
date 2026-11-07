#include "model/IndicatorSettings.h"

#include "model/Language.h"

#include <Vision/Objects.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/// The text as a number: surrounding white space and a leading '+' are
/// allowed (as QString::toDouble does), nothing else after the number.
std::optional<double> parse_number(std::string const& text) {
    char const* first = text.data();
    char const* last = first + text.size();
    while (first != last && is_space(*first))
        ++first;
    while (first != last && is_space(*(last - 1)))
        --last;
    if (first != last && *first == '+') {
        ++first;
        if (first != last && *first == '-')
            return std::nullopt;
    }
    double v = 0;
    auto const result = std::from_chars(first, last, v);
    if (result.ec != std::errc {} || result.ptr != last)
        return std::nullopt;
    return v;
}

std::string text_of(double v) {
    char buffer[64];
    auto const result = std::to_chars(buffer, buffer + sizeof buffer, v);
    return std::string(buffer, result.ptr);
}

/// The stored number, or `fallback` when absent, not numeric or outside [lo, hi]
/// (`inclusive` false: the bounds themselves are invalid too).
double number(SettingsStore const& store, std::string const& key, double fallback, double lo,
              double hi, bool inclusive = true) {
    auto const text = store.value(key);
    if (!text)
        return fallback;
    auto const v = parse_number(*text);
    if (!v || !std::isfinite(*v))
        return fallback;
    if (inclusive ? (*v < lo || *v > hi) : (*v <= lo || *v >= hi))
        return fallback;
    return *v;
}

bool contains(SettingsStore const& store, std::string const& key) {
    return store.value(key).has_value();
}

}  // namespace

IndicatorSettings load_indicator_settings(SettingsStore const& store) {
    IndicatorSettings s;
    Vision::CameraSettings const defaults {};
    if (!contains(store, "camera/focal_length_mm") && contains(store, "camera/fov_deg")) {
        Vision::CameraConfig old {};
        old.distance_mm = number(store, "camera/distance_mm", old.distance_mm, 1, 100000);
        old.fov_deg = number(store, "camera/fov_deg", -1, 0, 180, false);
        old.image_width_px =
            static_cast<int>(number(store, "camera/image_width_px", old.image_width_px, 1, 100000));
        if (old.fov_deg > 0 && old.fov_deg < 180) {
            Vision::LensCamera const lens = Vision::from_legacy(old);
            s.camera.distance_mm = lens.distance_mm;
            s.camera.focal_length_mm = lens.focal_length_mm;
            s.camera.pixel_width_um = s.camera.pixel_height_um = lens.pixel_width_um;
        }
    } else {
        s.camera.distance_mm = number(store, "camera/distance_mm", defaults.distance_mm, 1, 100000);
        s.camera.focal_length_mm =
            number(store, "camera/focal_length_mm", defaults.focal_length_mm, 0.1, 2000);
        double const old_pixel =
            number(store, "camera/pixel_size_um", defaults.pixel_width_um, 0.1, 100);
        s.camera.pixel_width_um = number(store, "camera/pixel_width_um", old_pixel, 0.1, 100);
        s.camera.pixel_height_um = number(store, "camera/pixel_height_um", old_pixel, 0.1, 100);
        s.camera.input = store.value("camera/sensor_input").value_or("") == "pixel"
                             ? Vision::SensorInput::pixel
                             : defaults.input;
        s.camera.sensor_width_mm =
            number(store, "camera/sensor_width_mm", defaults.sensor_width_mm, 0.1, 100);
        s.camera.sensor_height_mm =
            number(store, "camera/sensor_height_mm", defaults.sensor_height_mm, 0.1, 100);
        if (!contains(store, "camera/sensor_input") && contains(store, "camera/sensor_width_px")) {
            // from before the sensor size: the same scale as a sensor of
            // pixel size x sensor width, taken as 4:3
            double const width_px = number(store, "camera/sensor_width_px", 0, 1, 100000);
            double const width_mm = s.camera.pixel_width_um * width_px / 1000;
            if (width_mm >= 0.1 && width_mm <= 100) {
                s.camera.sensor_width_mm = width_mm;
                s.camera.sensor_height_mm = std::max(0.1, width_mm * 3 / 4);
            }
        }
    }
    // a converted camera can still fall outside what the dialog accepts
    auto const in = [](double v, double lo, double hi) { return v >= lo && v <= hi; };
    if (!in(s.camera.distance_mm, 1, 100000))
        s.camera.distance_mm = defaults.distance_mm;
    if (!in(s.camera.focal_length_mm, 0.1, 2000))
        s.camera.focal_length_mm = defaults.focal_length_mm;
    if (!in(s.camera.pixel_width_um, 0.1, 100))
        s.camera.pixel_width_um = defaults.pixel_width_um;
    if (!in(s.camera.pixel_height_um, 0.1, 100))
        s.camera.pixel_height_um = defaults.pixel_height_um;
    double constexpr unbounded = std::numeric_limits<double>::infinity();
    s.min_area_mm2 = Vision::snap_min_area(
        number(store, "objects/min_area_mm2", s.min_area_mm2, -unbounded, unbounded));
    s.model_path = store.value("ai/model_path").value_or(s.model_path);
    s.last_folder = store.value("dialogs/last_folder").value_or("");
    std::string const language = store.value("ui/language").value_or("");
    auto const codes = language_codes();
    if (std::find(codes.begin(), codes.end(), language) != codes.end())
        s.language = language;
    s.light = store.value("ui/theme").value_or("") == "light";
    return s;
}

void save_indicator_settings(SettingsStore& store, IndicatorSettings const& s) {
    store.set_value("camera/distance_mm", text_of(s.camera.distance_mm));
    store.set_value("camera/focal_length_mm", text_of(s.camera.focal_length_mm));
    store.set_value("camera/sensor_input",
                    s.camera.input == Vision::SensorInput::sensor ? "sensor" : "pixel");
    store.set_value("camera/pixel_width_um", text_of(s.camera.pixel_width_um));
    store.set_value("camera/pixel_height_um", text_of(s.camera.pixel_height_um));
    store.set_value("camera/sensor_width_mm", text_of(s.camera.sensor_width_mm));
    store.set_value("camera/sensor_height_mm", text_of(s.camera.sensor_height_mm));
    store.set_value("objects/min_area_mm2", text_of(s.min_area_mm2));
    store.set_value("dialogs/last_folder", s.last_folder);
    store.set_value("ui/language", s.language);
    store.set_value("ui/theme", s.light ? "light" : "dark");
}
