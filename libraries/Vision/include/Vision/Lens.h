#ifndef VISION_LENS_H
#define VISION_LENS_H
#include <Vision/Vision_export.h>
#include <Vision/Camera.h>

namespace Vision {

/// The camera as the operator knows it: lens and pixel size, not a field of view.
/// Pixel width and height are in the sensor's orientation.
struct LensCamera {
    double distance_mm {1500};
    double focal_length_mm {13.02};
    double pixel_width_um {3.76};
    double pixel_height_um {3.76};
};

/// Millimetres per pixel on the hide plane along columns (x) and rows (y).
struct PixelScale {
    double x {0};
    double y {0};
};

enum class SensorInput { pixel, sensor };

/// The camera settings: the fallback for what the photo does not say.
struct CameraSettings {
    double distance_mm {1500};
    double focal_length_mm {13.02};
    /// Sensor size by default: right for a downscaled photo too; a pixel size
    /// holds only for a photo at the sensor's full resolution.
    SensorInput input {SensorInput::sensor};
    double pixel_width_um {3.76};
    double pixel_height_um {3.76};
    double sensor_width_mm {15.04};   ///< 4000 px x 3.76 µm
    double sensor_height_mm {11.28};  ///< 3000 px x 3.76 µm
};

/// The field of view that camera.json stores next to the lens values (format
/// v4), for a photo of width w: fov = 2 atan(w p / (2 f)), image width = w,
/// p the pixel width as shown (the sensor's pixel height when `rotated`),
/// pixel_aspect = height / width, inverted when `rotated`. Measuring does not
/// use it: find_objects takes lens_scale.
CameraConfig VISION_API to_camera_config(LensCamera const& lens, int image_width_px,
                                         bool rotated = false) noexcept;

/// d · pixel width / f, d · pixel height / f (mm/px), swapped when `rotated`.
PixelScale VISION_API lens_scale(LensCamera const& lens, bool rotated = false) noexcept;

/// A lens camera with the same scale as an old distance / field-of-view one
/// (the pixel size keeps the old full-width scale).
LensCamera VISION_API from_legacy(CameraConfig const& old, double pixel_size_um = 3.76) noexcept;

/// The square pixel (µm) of a photo showing the whole sensor: the sensor's
/// long side over the photo's long side, whatever the orientation of either;
/// 0 when either long side is not positive.
double VISION_API sensor_pixel_um(double sensor_width_mm, double sensor_height_mm,
                                  int photo_width_px, int photo_height_px) noexcept;

}  // namespace Vision

#endif  // VISION_LENS_H
