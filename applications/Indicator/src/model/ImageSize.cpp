#include "model/ImageSize.h"

#include <Vision/Photo.h>

#include <fstream>
#include <iterator>
#include <string>

namespace {

/// The file's bytes; none when it cannot be opened (the path is opened as it is, any file name
/// works).
std::optional<std::string> read_bytes(std::filesystem::path const& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

std::optional<Vision::ImageSize> shown_size(std::filesystem::path const& photo) {
    auto const bytes = read_bytes(photo);
    if (!bytes)
        return std::nullopt;
    return Vision::shown_image_size(*bytes);
}

bool shown_rotated(std::filesystem::path const& photo) {
    auto const bytes = read_bytes(photo);
    if (!bytes)
        return false;
    auto const orientation = Vision::parse_photo_metadata(*bytes).orientation;
    return orientation && *orientation >= 5;
}
