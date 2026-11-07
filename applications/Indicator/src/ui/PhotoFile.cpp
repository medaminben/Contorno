#include "ui/PhotoFile.h"

#include <wx/log.h>
#include <wx/mstream.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string_view>

void ensure_image_handlers() {
    static bool const added = [] {
        wxInitAllImageHandlers();
        return true;
    }();
    (void)added;
}

namespace {

/// `image` turned as EXIF `orientation` (1-8) says it is shown.
wxImage oriented(wxImage const& image, int orientation) {
    switch (orientation) {
    case 2: return image.Mirror(true);
    case 3: return image.Rotate180();
    case 4: return image.Mirror(false);
    case 5: return image.Rotate90(true).Mirror(true);   // transposed
    case 6: return image.Rotate90(true);
    case 7: return image.Rotate90(false).Mirror(true);  // transversed
    case 8: return image.Rotate90(false);
    default: return image;
    }
}

}  // namespace

PhotoFile read_photo(std::filesystem::path const& path) {
    PhotoFile photo;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        photo.error = std::strerror(errno);
        return photo;
    }
    std::string const bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    ensure_image_handlers();
    wxMemoryInputStream stream(bytes.data(), bytes.size());
    wxImage image;
    {
        wxLogNull const quiet;  // the reason is reported by the caller, not in a wx log window
        image.LoadFile(stream, wxBITMAP_TYPE_ANY);
    }
    if (!image.IsOk()) {
        photo.error = "Unsupported image format";
        return photo;
    }
    photo.metadata = Vision::parse_photo_metadata(std::string_view(bytes));
    photo.image = oriented(image, photo.metadata.orientation.value_or(1));
    return photo;
}
