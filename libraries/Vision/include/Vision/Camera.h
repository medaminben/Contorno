#ifndef VISION_CAMERA_H
#define VISION_CAMERA_H
#include <Vision/Vision_export.h>
#include <Core/Core.h>
#include <Pot/Pot.h>
#include <cstdint>
#include <vector>

namespace Vision {

/// Pinhole camera looking straight down at the sheet.
struct CameraConfig {
    double distance_mm {1500};
    double fov_deg {60};
    int image_width_px {4000};
    double pixel_aspect {1.0};  ///< a pixel's height / width on the photo as loaded
};

/// One image's pixel grid in millimetres: mm per pixel along columns (x) and rows (y).
struct PixelFrame {
    int width {0};
    int height {0};
    double mm_per_px_x {0};
    double mm_per_px_y {0};
};

/// Row-major 8-bit mask, 0 or 255, size width*height.
struct Mask {
    int width {0};
    int height {0};
    std::vector<uint8_t> data {};
};

/**
 * @brief Millimetres per pixel of an image of the given width, from a distance and field of view:
 * the camera model of camera.json v1–v3 and the camera/fov_deg setting
 *
 * 2 * distance * tan(fov / 2) / image_width_px * image_width_px / image_width.
 * This is the x scale. Error if distance <= 0, fov outside (0, 180), image_width_px <= 0,
 * image_width <= 0 or pixel_aspect is not finite and > 0.
 */
Result<double> VISION_API mm_per_px(CameraConfig const& camera, int image_width) noexcept;

/// Pixel (col, row) to millimetres: x = (col + 0.5) sx, y = (height - row - 0.5) sy (y up).
Pot::PointD VISION_API to_mm(PixelFrame const& frame, double col, double row) noexcept;

/// Inverse of to_mm: returns {col, row}.
Pot::PointD VISION_API to_px(PixelFrame const& frame, Pot::PointD mm) noexcept;

/// A length in mm as a pixel count along both axes: mm / min(sx, sy).
double VISION_API px_for_mm(PixelFrame const& frame, double mm) noexcept;

}  // namespace Vision

#endif  // VISION_CAMERA_H
