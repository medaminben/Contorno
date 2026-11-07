#ifndef INDICATOR_IMAGESIZE_H
#define INDICATOR_IMAGESIZE_H
#include <Vision/Image.h>

#include <filesystem>
#include <optional>

/// The photo's size as shown, with its EXIF rotation (the stored size is the
/// size before it): the size Vision analyses. None when the file cannot be
/// read or is not an image.
std::optional<Vision::ImageSize> shown_size(std::filesystem::path const& photo);

/// True when the photo is shown rotated by 90° (EXIF orientation 5–8).
bool shown_rotated(std::filesystem::path const& photo);

#endif  // INDICATOR_IMAGESIZE_H
