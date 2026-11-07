#include "Camera_impl.h"

#include <Vision/Camera.h>

#include <exception>

namespace Vision {

Result<double> mm_per_px(CameraConfig const& camera, int image_width) noexcept {
    try {
        return mm_per_px_impl(camera, image_width);
    } catch (std::exception const& e) {
        return Core::error_result<double>([&] { return camera_problem(Core::Problem::raw(e.what())); });
    } catch (...) {
        return Core::error_result<double>(
            [] { return camera_problem(Core::Problem("unknown_error", "unknown error")); });
    }
}

Pot::PointD to_mm(PixelFrame const& frame, double col, double row) noexcept {
    return to_mm_impl(frame, col, row);
}

Pot::PointD to_px(PixelFrame const& frame, Pot::PointD mm) noexcept {
    return to_px_impl(frame, mm);
}

double px_for_mm(PixelFrame const& frame, double mm) noexcept {
    return px_for_mm_impl(frame, mm);
}

}  // namespace Vision
