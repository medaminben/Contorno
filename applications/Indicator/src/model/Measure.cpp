#include "model/Measure.h"

#include <algorithm>
#include <charconv>

namespace {

/// `value` with a fixed number of decimals, whatever the locale.
std::string fixed(double value, int decimals) {
    char buffer[400];  // the largest double needs 309 digits before the point
    auto const result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, decimals);
    return std::string(buffer, result.ptr);
}

}  // namespace

std::string scale_text(Vision::PixelScale scale, int decimals) {
    std::string const x = fixed(scale.x, decimals);
    std::string const y = fixed(scale.y, decimals);
    return scale.x == scale.y ? x : x + " × " + y;
}

AreaText format_area(double area_mm2) {
    if (!(area_mm2 > 0)) {
        return {"0.0", "cm²"};
    }
    if (area_mm2 < 100000) {
        return {fixed(area_mm2 / 100, 1), "cm²"};
    }
    double const m2 = area_mm2 / 1e6;
    std::string const unit = "m²";
    for (int decimals = 3; decimals >= 0; --decimals) {
        std::string const text = fixed(m2, decimals);
        auto const digits = static_cast<long>(text.size()) - std::count(text.begin(), text.end(), '.');
        if (digits <= 5) {
            return {text, unit};
        }
    }
    return {"99999", unit};
}
