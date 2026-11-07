#include "Camera_impl.h"

#include <algorithm>
#include <cmath>

namespace Vision {

Result<double> mm_per_px_impl(CameraConfig const& camera, int image_width) {
    constexpr double pi = 3.14159265358979323846;
    auto fail = [](Core::Problem reason) {
        return Core::error_result<double>([&] { return camera_problem(std::move(reason)); });
    };
    if (!(camera.distance_mm > 0) || !std::isfinite(camera.distance_mm)) {
        return fail(Core::Problem("camera.distance", "distance must be positive and finite"));
    }
    if (!(camera.fov_deg > 0 && camera.fov_deg < 180)) {
        return fail(Core::Problem("camera.fov", "field of view must be in {range} degrees", {{"range", "(0, 180)"}}));
    }
    if (camera.image_width_px <= 0 || image_width <= 0) {
        return fail(Core::Problem("camera.width", "image width must be positive"));
    }
    if (!std::isfinite(camera.pixel_aspect) || !(camera.pixel_aspect > 0)) {
        return fail(Core::Problem("camera.aspect", "pixel aspect must be positive and finite"));
    }
    double const full = 2.0 * camera.distance_mm * std::tan(camera.fov_deg * pi / 360.0);
    return full / camera.image_width_px * (static_cast<double>(camera.image_width_px) / image_width);
}

Pot::PointD to_mm_impl(PixelFrame const& frame, double col, double row) noexcept {
    return {(col + 0.5) * frame.mm_per_px_x, (frame.height - row - 0.5) * frame.mm_per_px_y};
}

Pot::PointD to_px_impl(PixelFrame const& frame, Pot::PointD mm) noexcept {
    return {mm.x / frame.mm_per_px_x - 0.5, frame.height - 0.5 - mm.y / frame.mm_per_px_y};
}

double px_for_mm_impl(PixelFrame const& frame, double mm) noexcept {
    return mm / std::min(frame.mm_per_px_x, frame.mm_per_px_y);
}

}  // namespace Vision
