#ifndef VISION_IMAGE_H
#define VISION_IMAGE_H
#include <Vision/Vision_export.h>
#include <Vision/Camera.h>

#include <optional>
#include <string>
#include <string_view>

namespace Vision {

struct ImageSize {
    int width {0};
    int height {0};

    bool operator==(ImageSize const&) const = default;
};

/// The size of an encoded photo as shown: its EXIF orientation applied, as
/// the analysis reads it. None when the bytes are not an image OpenCV reads.
std::optional<ImageSize> VISION_API shown_image_size(std::string_view bytes) noexcept;

/// The mask as an 8-bit grey PNG. None for an empty mask or one whose data is
/// not width x height.
std::optional<std::string> VISION_API encode_mask_png(Mask const& mask) noexcept;

/// A PNG as an 8-bit grey mask (colour is converted to grey; the values are
/// kept, not thresholded). None when the bytes are not a PNG.
std::optional<Mask> VISION_API decode_mask_png(std::string_view bytes) noexcept;

}  // namespace Vision

#endif  // VISION_IMAGE_H
