#ifndef VISION_TEST_EXIF_H
#define VISION_TEST_EXIF_H
#include <cstdint>
#include <string>
#include <vector>

namespace test_Exif {

/// One EXIF tag for `exif_tiff`: type 3 SHORT, 4 LONG, 5 RATIONAL (numerator / denominator).
struct ExifTag {
    uint16_t tag;
    uint16_t type;
    uint32_t numerator;
    uint32_t denominator = 1;
};

/// TIFF bytes ("II" or "MM"): IFD0 with only the Exif IFD pointer, the Exif IFD with `tags`.
inline std::string exif_tiff(std::vector<ExifTag> const& tags, bool big_endian = false) {
    std::string out;
    auto put = [&](uint32_t v, int n) {
        for (int i = 0; i < n; ++i) {
            int const shift = 8 * (big_endian ? n - 1 - i : i);
            out.push_back(static_cast<char>((v >> shift) & 0xFF));
        }
    };
    out.append(big_endian ? "MM" : "II", 2);
    put(42, 2);
    put(8, 4);
    put(1, 2);  // IFD0: the Exif IFD pointer
    put(0x8769, 2);
    put(4, 2);
    put(1, 4);
    put(26, 4);
    put(0, 4);
    put(static_cast<uint32_t>(tags.size()), 2);  // Exif IFD at 26
    uint32_t data_off = 26 + 2 + 12 * static_cast<uint32_t>(tags.size()) + 4;
    for (auto const& t : tags) {
        put(t.tag, 2);
        put(t.type, 2);
        put(1, 4);
        if (t.type == 5) {
            put(data_off, 4);
            data_off += 8;
        } else {
            put(t.numerator, t.type == 3 ? 2 : 4);
            if (t.type == 3) put(0, 2);
        }
    }
    put(0, 4);
    for (auto const& t : tags) {
        if (t.type != 5) continue;
        put(t.numerator, 4);
        put(t.denominator, 4);
    }
    return out;
}

/// JPEG markers only (no image data): SOI, an APP1 segment holding `payload`, EOI.
inline std::string jpeg_with_app1(std::string const& payload) {
    std::string jpeg("\xFF\xD8\xFF\xE1", 4);
    jpeg.push_back(static_cast<char>((payload.size() + 2) >> 8));
    jpeg.push_back(static_cast<char>((payload.size() + 2) & 0xFF));
    return jpeg + payload + std::string("\xFF\xD9", 2);
}

/// A JPEG whose APP1 "Exif\0\0" holds exif_tiff(tags).
inline std::string exif_jpeg(std::vector<ExifTag> const& tags, bool big_endian = false) {
    return jpeg_with_app1(std::string("Exif\0\0", 6) + exif_tiff(tags, big_endian));
}

/// A JPEG whose EXIF has only IFD0 with Orientation `value`: no camera tags.
inline std::string orientation_only_jpeg(unsigned char value = 6) {
    unsigned char tiff[] = {'I', 'I', 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x12, 0x01, 0x03,
                                         0x00, 0x01, 0x00, 0x00, 0x00, value, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    return jpeg_with_app1(std::string("Exif\0\0", 6) + std::string(reinterpret_cast<char const*>(tiff), sizeof tiff));
}

}  // namespace test_Exif

#endif  // VISION_TEST_EXIF_H
