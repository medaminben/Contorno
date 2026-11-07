#include <Vision/Photo.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>

namespace Vision {

namespace {

/// Bounds-checked reader over a TIFF block.
struct Tiff {
    std::string_view data;
    std::size_t begin;  ///< offset of the TIFF header in `data`
    std::size_t size;   ///< bytes of the TIFF block
    bool big;

    std::optional<uint32_t> u(uint64_t off, int n) const {
        if (off + static_cast<uint64_t>(n) > static_cast<uint64_t>(size))
            return std::nullopt;
        uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            auto const b = static_cast<unsigned char>(
                data[begin + static_cast<std::size_t>(off) + (big ? i : n - 1 - i)]);
            v = (v << 8) | b;
        }
        return v;
    }
};

struct Entry {
    uint16_t type {};
    uint32_t count {};
    uint64_t value_offset {};  ///< where the value bytes start
};

/// The entry `tag` of the IFD at `ifd`, if present and its value lies inside the block.
std::optional<Entry> find_entry(Tiff const& t, uint32_t ifd, uint16_t tag) {
    auto const n = t.u(ifd, 2);
    if (!n)
        return std::nullopt;
    for (uint32_t i = 0; i < *n; ++i) {
        uint64_t const e = uint64_t(ifd) + 2 + uint64_t(i) * 12;
        auto const id = t.u(e, 2), type = t.u(e + 2, 2), count = t.u(e + 4, 4);
        if (!id || !type || !count)
            return std::nullopt;
        if (*id != tag)
            continue;
        int const unit = *type == 3 ? 2 : (*type == 4 || *type == 13) ? 4 : *type == 5 ? 8 : 0;
        if (unit == 0 || *count == 0)
            return std::nullopt;
        uint64_t off = e + 8;
        if (uint64_t(unit) * *count > 4) {
            auto const o = t.u(e + 8, 4);
            if (!o)
                return std::nullopt;
            off = *o;
        }
        if (!t.u(off, unit))
            return std::nullopt;
        return Entry {static_cast<uint16_t>(*type), *count, off};
    }
    return std::nullopt;
}

std::optional<double> rational(Tiff const& t, uint32_t ifd, uint16_t tag) {
    auto const e = find_entry(t, ifd, tag);
    if (!e || e->type != 5)
        return std::nullopt;
    auto const num = t.u(e->value_offset, 4), den = t.u(e->value_offset + 4, 4);
    if (!num || !den || *den == 0)
        return std::nullopt;
    return double(*num) / double(*den);
}

std::optional<int> integer(Tiff const& t, uint32_t ifd, uint16_t tag) {
    auto const e = find_entry(t, ifd, tag);
    if (!e || (e->type != 3 && e->type != 4))
        return std::nullopt;
    auto const v = t.u(e->value_offset, e->type == 3 ? 2 : 4);
    if (!v || *v > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return std::nullopt;
    return static_cast<int>(*v);
}

PhotoMetadata parse_tiff(std::string_view data, std::size_t begin, std::size_t size) {
    PhotoMetadata m;
    if (size < 8)
        return m;
    char const* h = data.data() + begin;
    bool big;
    if (h[0] == 'I' && h[1] == 'I' && h[2] == '*' && h[3] == 0)
        big = false;
    else if (h[0] == 'M' && h[1] == 'M' && h[2] == 0 && h[3] == '*')
        big = true;
    else
        return m;
    Tiff const t {data, begin, size, big};
    auto const ifd0 = t.u(4, 4);
    if (!ifd0)
        return m;
    if (auto const o = integer(t, *ifd0, 0x0112); o && *o >= 1 && *o <= 8)
        m.orientation = o;
    auto const exif = find_entry(t, *ifd0, 0x8769);
    if (!exif || (exif->type != 4 && exif->type != 13))
        return m;  // LONG, or IFD
    auto const ifd = t.u(exif->value_offset, 4);
    if (!ifd)
        return m;

    m.focal_length_mm = rational(t, *ifd, 0x920A);
    m.focal_length_35mm = integer(t, *ifd, 0xA405);
    m.pixel_x_dimension = integer(t, *ifd, 0xA002);
    m.pixel_y_dimension = integer(t, *ifd, 0xA003);
    int const unit = integer(t, *ifd, 0xA210).value_or(2);
    auto const px_per_mm = [&](uint16_t tag) -> std::optional<double> {
        auto const res = rational(t, *ifd, tag);
        if (!res || !(*res > 0))
            return std::nullopt;
        switch (unit) {
            case 2:
                return *res / 25.4;
            case 3:
                return *res / 10.0;
            case 4:
                return *res;
            case 5:
                return *res * 1000.0;
            default:
                return std::nullopt;
        }
    };
    m.focal_plane_px_per_mm = px_per_mm(0xA20E);
    m.focal_plane_y_px_per_mm = px_per_mm(0xA20F);
    return m;
}

/// The TIFF block of the first APP1 "Exif" segment of a JPEG.
PhotoMetadata parse_jpeg(std::string_view d) {
    std::size_t pos = 2;
    while (pos + 4 <= d.size()) {
        if (static_cast<unsigned char>(d[pos]) != 0xFF)
            break;
        auto const marker = static_cast<unsigned char>(d[pos + 1]);
        if (marker == 0xFF) {
            ++pos;
            continue;
        }  // fill byte
        if (marker == 0xDA || marker == 0xD9)
            break;  // image data / end
        std::size_t const len =
            (static_cast<std::size_t>(static_cast<unsigned char>(d[pos + 2])) << 8) |
            static_cast<unsigned char>(d[pos + 3]);
        if (len < 2 || pos + 2 + len > d.size())
            break;
        if (marker == 0xE1 && len >= 8 && d.substr(pos + 4, 6) == std::string_view("Exif\0\0", 6))
            return parse_tiff(d, pos + 10, len - 8);
        pos += 2 + len;
    }
    return {};
}

constexpr double kFullFrameDiagonalMm = 43.27;

bool focal_valid(double f) {
    return f >= 0.1 && f <= 2000;
}
bool pixel_valid(double p) {
    return p >= 0.1 && p <= 100;
}

/// Notes are advice: one that cannot be stored (out of memory) is dropped.
void note(PhotoCamera& cam, CameraNoteKind kind, double value = 0) noexcept {
    try {
        cam.notes.push_back({kind, value});
    } catch (...) {}
}

}  // namespace

PhotoMetadata parse_photo_metadata(std::string_view file) noexcept {
    if (file.size() >= 4 && static_cast<unsigned char>(file[0]) == 0xFF &&
        static_cast<unsigned char>(file[1]) == 0xD8)
        return parse_jpeg(file);
    return parse_tiff(file, 0, file.size());
}

PhotoMetadata read_photo_metadata(std::string const& path) noexcept {
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};
        std::string const bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        return parse_photo_metadata(bytes);
    } catch (...) {
        return {};
    }
}

PhotoCamera resolve_camera(CameraSettings const& settings, PhotoMetadata const& metadata,
                           int shown_width, int shown_height) noexcept {
    PhotoCamera cam;
    cam.lens = {settings.distance_mm, settings.focal_length_mm, settings.pixel_width_um,
                settings.pixel_height_um};

    bool focal_ok = false;
    if (metadata.focal_length_mm) {
        double const f = *metadata.focal_length_mm;
        if (focal_valid(f)) {
            cam.lens.focal_length_mm = f;
            cam.focal_source = CameraSource::photo;
            focal_ok = true;
        } else {
            note(cam, CameraNoteKind::focal_length_invalid, f);
        }
    }

    double const L = std::max(shown_width, shown_height);
    double const S = std::min(shown_width, shown_height);

    // 1. the focal-plane resolution (y absent: as x), scaled when the photo was resized after
    // capture
    if (metadata.focal_plane_px_per_mm && *metadata.focal_plane_px_per_mm > 0) {
        double px = 1000 / *metadata.focal_plane_px_per_mm;
        double py = px;
        if (metadata.focal_plane_y_px_per_mm && *metadata.focal_plane_y_px_per_mm > 0)
            py = 1000 / *metadata.focal_plane_y_px_per_mm;
        if (metadata.pixel_x_dimension && metadata.pixel_y_dimension &&
            *metadata.pixel_x_dimension > 0 && *metadata.pixel_y_dimension > 0 && L > 0) {
            double const max_dim =
                std::max(*metadata.pixel_x_dimension, *metadata.pixel_y_dimension);
            if (max_dim != L) {
                px *= max_dim / L;
                py *= max_dim / L;
            }
        }
        if (pixel_valid(px) && pixel_valid(py)) {
            cam.lens.pixel_width_um = px;
            cam.lens.pixel_height_um = py;
            cam.pixel_source = CameraSource::photo;
        } else {
            note(cam, CameraNoteKind::focal_plane_invalid, pixel_valid(px) ? py : px);
        }
    }

    // 2. the 35 mm equivalent, with the photo's own focal length
    if (cam.pixel_source == CameraSource::settings && focal_ok && metadata.focal_length_35mm &&
        *metadata.focal_length_35mm > 0 && L > 0) {
        double const crop = *metadata.focal_length_35mm / cam.lens.focal_length_mm;
        double const diag = kFullFrameDiagonalMm / crop;
        double const long_mm = diag * L / std::hypot(L, S);
        double const p = 1000 * long_mm / L;
        if (pixel_valid(p)) {
            cam.lens.pixel_width_um = cam.lens.pixel_height_um = p;
            cam.pixel_source = CameraSource::photo_35mm;
        } else {
            note(cam, CameraNoteKind::equivalent_35mm_invalid, p);
        }
    }

    // 3. the settings
    if (cam.pixel_source == CameraSource::settings && settings.input == SensorInput::sensor &&
        L > 0) {
        double const p = sensor_pixel_um(settings.sensor_width_mm, settings.sensor_height_mm,
                                         shown_width, shown_height);
        cam.lens.pixel_width_um = cam.lens.pixel_height_um = p;
        double const long_side = std::max(settings.sensor_width_mm, settings.sensor_height_mm);
        double const short_side = std::min(settings.sensor_width_mm, settings.sensor_height_mm);
        if (short_side > 0 && S > 0) {
            double const sensor_ratio = long_side / short_side;
            if (std::abs(L / S - sensor_ratio) / sensor_ratio > 0.02)
                note(cam, CameraNoteKind::possibly_cropped);
        }
    }
    return cam;
}

}  // namespace Vision
