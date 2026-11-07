#include "Objects_impl.h"

#include <Vision/Objects.h>

#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>

namespace Vision {

Result<ObjectsAnalysis> find_objects(std::string const& image_path, PixelScale scale,
                                     ObjectOptions const& options) noexcept {
    try {
        return find_objects_impl(image_path, scale, options);
    } catch (cv::Exception const& e) {
        return Core::error_result<ObjectsAnalysis>(
            [&] { return objects_problem(Core::Problem::raw(e.what())); });
    } catch (std::exception const& e) {
        return Core::error_result<ObjectsAnalysis>(
            [&] { return objects_problem(Core::Problem::raw(e.what())); });
    } catch (...) {
        return Core::error_result<ObjectsAnalysis>(
            [] { return objects_problem(Core::Problem("unknown_error", "unknown error")); });
    }
}

double min_area_pct(double min_area_mm2, int width_px, int height_px, PixelScale scale) noexcept {
    if (!(std::isfinite(min_area_mm2) && min_area_mm2 > 0 && std::isfinite(scale.x) &&
          scale.x > 0 && std::isfinite(scale.y) && scale.y > 0) ||
        width_px <= 0 || height_px <= 0) {
        return 0;
    }
    double const pct =
        100.0 * min_area_mm2 / (static_cast<double>(width_px) * height_px * scale.x * scale.y);
    return std::min(pct, 100.0);  // larger than the photo: no object, not an invalid option
}

std::span<double const> min_area_presets() noexcept {
    static constexpr std::array<double, 9> presets {0, 10, 50, 100, 500, 1000, 5000, 10000, 50000};
    return presets;
}

double snap_min_area(double min_area_mm2) noexcept {
    auto const presets = min_area_presets();
    double best = presets.front();
    for (double const p : presets) {
        if (std::abs(p - min_area_mm2) < std::abs(best - min_area_mm2))
            best = p;
    }
    return best;
}

bool ai_available() noexcept {
#ifdef CONTORNO_WITH_TORCH
    return true;
#else
    return false;
#endif
}

}  // namespace Vision
