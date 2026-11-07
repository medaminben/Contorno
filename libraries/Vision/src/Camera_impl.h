#ifndef VISION_CAMERA_IMPL_H
#define VISION_CAMERA_IMPL_H
#include <Vision/Camera.h>

#include <utility>

namespace Vision {

/// "camera: <reason>".
inline Core::Problem camera_problem(Core::Problem reason) {
    return Core::wrapped("camera.failed", "camera: {reason}", std::move(reason));
}

Result<double> mm_per_px_impl(CameraConfig const& camera, int image_width);
Pot::PointD to_mm_impl(PixelFrame const& frame, double col, double row) noexcept;
Pot::PointD to_px_impl(PixelFrame const& frame, Pot::PointD mm) noexcept;
double px_for_mm_impl(PixelFrame const& frame, double mm) noexcept;

}  // namespace Vision

#endif  // VISION_CAMERA_IMPL_H
