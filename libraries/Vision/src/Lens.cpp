#include <Vision/Lens.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Vision {

CameraConfig to_camera_config(LensCamera const& lens, int image_width_px, bool rotated) noexcept {
    double const w = rotated ? lens.pixel_height_um : lens.pixel_width_um;
    double const h = rotated ? lens.pixel_width_um : lens.pixel_height_um;
    double const sensor_mm = image_width_px * w * 1e-3;
    double const fov = 2 * std::atan(sensor_mm / (2 * lens.focal_length_mm));
    return {lens.distance_mm, fov * 180 / std::numbers::pi, image_width_px, h / w};
}

PixelScale lens_scale(LensCamera const& lens, bool rotated) noexcept {
    double const k = lens.distance_mm * 1e-3 / lens.focal_length_mm;
    PixelScale const s {k * lens.pixel_width_um, k * lens.pixel_height_um};
    return rotated ? PixelScale {s.y, s.x} : s;
}

LensCamera from_legacy(CameraConfig const& old, double pixel_size_um) noexcept {
    LensCamera lens;
    lens.distance_mm = old.distance_mm;
    lens.pixel_width_um = lens.pixel_height_um = pixel_size_um;
    double const half_fov = old.fov_deg * std::numbers::pi / 360;
    lens.focal_length_mm = old.image_width_px * pixel_size_um * 1e-3 / (2 * std::tan(half_fov));
    return lens;
}

double sensor_pixel_um(double sensor_width_mm, double sensor_height_mm, int photo_width_px,
                       int photo_height_px) noexcept {
    double const sensor_long = std::max(sensor_width_mm, sensor_height_mm);
    int const photo_long = std::max(photo_width_px, photo_height_px);
    if (!(sensor_long > 0) || photo_long <= 0) {
        return 0;
    }
    return 1000 * sensor_long / photo_long;
}

}  // namespace Vision
