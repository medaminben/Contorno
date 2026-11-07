#ifndef VISION_PHOTO_H
#define VISION_PHOTO_H
#include <Vision/Vision_export.h>
#include <Vision/Lens.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Vision {

/// The camera tags of a photo's EXIF, as stored; absent when missing,
/// unreadable or (resolution) with an unknown unit.
struct PhotoMetadata {
    std::optional<double> focal_length_mm;          ///< FocalLength 0x920A
    std::optional<double> focal_plane_px_per_mm;    ///< FocalPlaneXResolution 0xA20E in px/mm (unit 0xA210: 2 inch (also when absent), 3 cm, 4 mm, 5 µm)
    std::optional<double> focal_plane_y_px_per_mm;  ///< FocalPlaneYResolution 0xA20F, same unit tag
    std::optional<double> focal_length_35mm;        ///< FocalLengthIn35mmFilm 0xA405
    std::optional<int> pixel_x_dimension;           ///< PixelXDimension 0xA002 (SHORT or LONG)
    std::optional<int> pixel_y_dimension;           ///< PixelYDimension 0xA003
    std::optional<int> orientation;                 ///< Orientation 0x0112 in IFD0, 1-8
};

/// JPEG (APP1 "Exif\0\0") or TIFF ("II*\0" / "MM\0*") bytes; IFD0 → Exif IFD (0x8769).
/// Never reads out of bounds; malformed data yields absent fields.
PhotoMetadata VISION_API parse_photo_metadata(std::string_view file) noexcept;

/// Reads the file; empty metadata when it cannot be read.
PhotoMetadata VISION_API read_photo_metadata(std::string const& path) noexcept;

enum class CameraSource { settings, photo, photo_35mm };

/// Why a photo value was not used, or a doubt about the result.
enum class CameraNoteKind {
    focal_length_invalid,      ///< value: the photo's focal length, mm
    focal_plane_invalid,       ///< value: the rejected focal-plane pixel size, µm
    equivalent_35mm_invalid,   ///< value: the pixel size from the 35 mm equivalent, µm
    possibly_cropped,          ///< the photo's side ratio differs from the sensor's by over 2 %; no value
};

struct CameraNote {
    CameraNoteKind kind {CameraNoteKind::possibly_cropped};
    double value {0};

    bool operator==(CameraNote const&) const = default;
};

/// The camera one photo is measured with, and where each value came from.
struct PhotoCamera {
    LensCamera lens {};
    CameraSource focal_source {CameraSource::settings};
    CameraSource pixel_source {CameraSource::settings};
    std::vector<CameraNote> notes {};  ///< in the order found
};

/**
 * @brief The photo's values where valid, else the settings
 *
 * The distance always comes from the settings. The focal length is the
 * photo's when within [0.1, 2000] mm. The pixel size, each within
 * [0.1, 100] µm, comes from the first that holds: the focal-plane
 * resolution (y absent: as x; scaled when PixelX/YDimension says the photo
 * was resized), the 35 mm equivalent with the photo's own focal length
 * (diagonal 43.27 mm), then the settings: their pixel size, or in sensor
 * mode sensor_pixel_um (noting a possible crop). `shown_width` and
 * `shown_height` are the photo's size as shown (after EXIF rotation); 0 for
 * no photo.
 */
PhotoCamera VISION_API resolve_camera(CameraSettings const& settings, PhotoMetadata const& metadata, int shown_width,
                                      int shown_height) noexcept;

}  // namespace Vision

#endif  // VISION_PHOTO_H
