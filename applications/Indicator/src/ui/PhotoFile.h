#ifndef INDICATOR_PHOTOFILE_H
#define INDICATOR_PHOTOFILE_H
#include <Vision/Photo.h>

#include <wx/image.h>

#include <filesystem>
#include <string>

/// Adds wx's image handlers (PNG, JPEG, BMP, TIFF and the rest) once.
void ensure_image_handlers();

/// A photo read from a file.
struct PhotoFile {
    /// As shown: its EXIF orientation applied (the size Vision analyses). Invalid when it cannot be
    /// read.
    wxImage image;
    /// The camera tags of the file (empty when it has none).
    Vision::PhotoMetadata metadata;
    /// Why it cannot be read ("" when it can): the system's reason, or that it is not an image wx
    /// reads.
    std::string error;
};

/// Reads the photo at `path` (any file name: the bytes are read through the path itself).
PhotoFile read_photo(std::filesystem::path const& path);

#endif  // INDICATOR_PHOTOFILE_H
