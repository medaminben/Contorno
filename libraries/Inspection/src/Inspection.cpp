#include <Inspection/Inspection.h>

#include <Tools/Dxf.h>
#include <Tools/Json.h>
#include <Vision/Image.h>
#include <Vision/Lens.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <sstream>
#include <utility>

namespace Inspection {

using Tools::Json;

namespace {

using Value = Core::Problem::Value;

/// "<entry>: <reason>".
Core::Problem entry_failed(std::string const& entry, Core::Problem reason) {
    return Core::Problem("inspection.entry", "{entry}: {reason}",
                         {{"entry", entry}, {"reason", std::move(reason)}});
}

Json number(double value) {
    return Json(value);
}

Json ring_json(Pot::Path64 const& ring) {
    Json::Array points;
    for (auto const& p : ring) {
        points.push_back(
            Json(Json::Array {number(p.x / Pot::units_per_mm), number(p.y / Pot::units_per_mm)}));
    }
    return Json(std::move(points));
}

Json polygon_json(Pot::Polygon64 const& polygon) {
    Json::Array rings {ring_json(polygon.outer)};
    for (auto const& hole : polygon.holes)
        rings.push_back(ring_json(hole));
    return Json(std::move(rings));
}

/// Appends <name> holding json as indented text.
Result<> add_text_entry(std::vector<Tools::ZipEntry>& entries, std::string name, Json const& json) {
    auto text = Tools::to_json_text(json, 2);
    if (text.isError())
        return Result<>(Core::Error<>(entry_failed(name, std::move(text).error())));
    entries.push_back({std::move(name), std::move(text)() + "\n"});
    return Result<>(std::monostate {});
}

std::string source_name(Vision::CameraSource source) {
    return source == Vision::CameraSource::settings ? "settings"
                                                    : "photo";  // photo_35mm is still the photo's
}

/// Version 4: the lens with its pixel width and height (sensor orientation), where
/// its focal length and pixel size came from, and the derived field of view for
/// the analysed image of `image_width_px` (as shown: `rotated` swaps the pixel sides).
Json camera_json(Vision::PhotoCamera const& camera, int image_width_px, bool rotated) {
    auto const& lens = camera.lens;
    auto const derived = Vision::to_camera_config(lens, image_width_px, rotated);
    return Json(Json::Object {
        {"format", Json(std::string("contorno-camera"))},
        {"version", number(4)},
        {"distance_mm", number(lens.distance_mm)},
        {"focal_length_mm", number(lens.focal_length_mm)},
        {"pixel_width_um", number(lens.pixel_width_um)},
        {"pixel_height_um", number(lens.pixel_height_um)},
        {"focal_length_source", Json(source_name(camera.focal_source))},
        {"pixel_size_source", Json(source_name(camera.pixel_source))},
        {"image_width_px", number(derived.image_width_px)},
        {"fov_deg", number(derived.fov_deg)},
    });
}

Json parameters_json(Record const& record) {
    return Json(Json::Object {
        {"format", Json(std::string("contorno-objects-parameters"))},
        {"version", number(1)},
        {"min_area_mm2", number(record.min_area_mm2)},
        {"outline_tolerance_mm", number(record.outline_tolerance_mm)},
    });
}

Json result_json(Vision::ObjectsAnalysis const& a) {
    Json::Array objects;
    for (auto const& o : a.objects)
        objects.push_back(polygon_json(o));
    return Json(Json::Object {
        {"format", Json(std::string("contorno-objects-result"))},
        {"version", number(2)},
        {"image", Json(Json::Object {{"width", number(a.frame.width)},
                                     {"height", number(a.frame.height)},
                                     {"mm_per_px_x", number(a.frame.mm_per_px_x)},
                                     {"mm_per_px_y", number(a.frame.mm_per_px_y)}})},
        {"area_mm2", number(a.area_mm2)},
        {"objects", Json(std::move(objects))},
    });
}

/// The "format" of a JSON entry; empty when it has none.
std::string format_of(Json const& root) {
    Json const* v = root.find("format");
    std::string const* found = v != nullptr ? v->as_string() : nullptr;
    return found != nullptr ? *found : std::string();
}

bool starts_with(std::string const& text, char const* prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string text_of(double v) {
    std::ostringstream out;
    out << v;
    return out.str();
}

/// Reads the members of one JSON entry; the first problem names the entry and the key.
class Reader {
public:
    Reader(std::string entry, Json const& root) : entry_(std::move(entry)), root_(root) {}

    Json const* object(char const* key) {
        Json const* v = root_.find(key);
        if (v == nullptr || !v->is_object())
            note(key, Core::Problem("inspection.what.object", "an object"));
        return v != nullptr && v->is_object() ? v : nullptr;
    }
    double number(Json const* in, char const* key) {
        Json const* v = in != nullptr ? in->find(key) : nullptr;
        if (v == nullptr || !v->as_number()) {
            note(key, Core::Problem("inspection.what.number", "a number"));
            return 0;
        }
        return *v->as_number();
    }
    /// "version": a whole number from 1 to `newest` (0 when it is not).
    int version(int newest) {
        double const value = number(&root_, "version");
        if (value != std::floor(value) || value < 1 || value > newest) {
            note("version", newest == 1 ? Core::Problem("inspection.what.version_one", "1")
                                        : Core::Problem("inspection.what.version", "1 to {max}",
                                                        {{"max", std::to_string(newest)}}));
            return 0;
        }
        return static_cast<int>(value);
    }
    /// "settings" or "photo".
    Vision::CameraSource source(Json const* in, char const* key) {
        Json const* v = in != nullptr ? in->find(key) : nullptr;
        std::string const* text = v != nullptr ? v->as_string() : nullptr;
        if (text != nullptr && *text == "settings")
            return Vision::CameraSource::settings;
        if (text != nullptr && *text == "photo")
            return Vision::CameraSource::photo;
        note(key, Core::Problem("inspection.what.source", "\"settings\" or \"photo\""));
        return Vision::CameraSource::settings;
    }
    /// A finite number above zero.
    double positive(Json const* in, char const* key) {
        double const value = number(in, key);
        if (!(value > 0) || !std::isfinite(value))
            note(key, Core::Problem("inspection.what.positive", "a positive number"));
        return value;
    }
    /// A number from lo to hi (`whole`: a whole number), the camera dialog's ranges.
    double within(Json const* in, char const* key, double lo, double hi, bool whole = false) {
        double const value = number(in, key);
        if (!(value >= lo && value <= hi) || (whole && value != std::floor(value))) {
            std::vector<Value> range {{"low", text_of(lo)}, {"high", text_of(hi)}};
            note(key, whole ? Core::Problem("inspection.what.whole_range",
                                            "a whole number from {low} to {high}", std::move(range))
                            : Core::Problem("inspection.what.range",
                                            "a number from {low} to {high}", std::move(range)));
        }
        return value;
    }
    /// A finite number of at least zero.
    double non_negative(Json const* in, char const* key) {
        double const value = number(in, key);
        if (!(value >= 0) || !std::isfinite(value)) {
            note(key, Core::Problem("inspection.what.at_least_zero", "a number of at least 0"));
        }
        return value;
    }
    /// A whole number from 1 to the int range.
    int count(Json const* in, char const* key) {
        double const value = number(in, key);
        if (!(value >= 1) || value > std::numeric_limits<int>::max() ||
            value != std::floor(value)) {
            note(key, Core::Problem("inspection.what.whole_above_zero", "a whole number above 0"));
            return 0;
        }
        return static_cast<int>(value);
    }
    /// "contorno-<kind>"; inspections saved before the rename say "conqueror-<kind>".
    void format(char const* expected) {
        Json const* v = root_.find("format");
        std::string const* found = v != nullptr ? v->as_string() : nullptr;
        std::string const legacy =
            "conqueror" + std::string(expected).substr(std::strlen("contorno"));
        if (found == nullptr || (*found != expected && *found != legacy)) {
            note("format",
                 Core::Problem("inspection.what.format", "\"{format}\"", {{"format", expected}}));
        }
    }
    /// A polygon [ring, ...]: outer ring, then holes.
    Pot::Polygon64 polygon(Json const* json, char const* key) {
        Pot::Polygon64 out;
        auto const* rings = json != nullptr ? json->as_array() : nullptr;
        if (rings == nullptr || rings->empty()) {
            note(key, Core::Problem("inspection.what.rings", "a list of rings"));
            return out;
        }
        out.outer = ring_of(&rings->front(), key);
        for (std::size_t i = 1; i < rings->size(); ++i)
            out.holes.push_back(ring_of(&(*rings)[i], key));
        return out;
    }
    /// The first problem; none when everything read was valid.
    std::optional<Core::Problem> const& problem() const { return problem_; }

private:
    Pot::Path64 ring_of(Json const* json, char const* key) {
        Pot::Path64 out;
        auto const* points = json != nullptr ? json->as_array() : nullptr;
        auto const bad = [&] {
            note(key, Core::Problem("inspection.what.ring", "a ring of [x, y] points"));
        };
        if (points == nullptr) {
            bad();
            return out;
        }
        for (auto const& point : *points) {
            auto const* xy = point.as_array();
            if (xy == nullptr || xy->size() != 2 || !(*xy)[0].as_number() ||
                !(*xy)[1].as_number()) {
                bad();
                return {};
            }
            out.push_back({std::llround(*(*xy)[0].as_number() * Pot::units_per_mm),
                           std::llround(*(*xy)[1].as_number() * Pot::units_per_mm)});
        }
        return out;
    }
    void note(char const* key, Core::Problem expected) {
        if (!problem_) {
            problem_ =
                Core::Problem("inspection.must_be", "{entry}: \"{key}\" must be {expected}",
                              {{"entry", entry_}, {"key", key}, {"expected", std::move(expected)}});
        }
    }
    std::string entry_;
    Json const& root_;
    std::optional<Core::Problem> problem_;
};

Result<std::vector<Tools::ZipEntry>> to_entries_impl(Record const& record,
                                                     Vision::ObjectsAnalysis const& analysis,
                                                     bool rotated) {
    using Entries = std::vector<Tools::ZipEntry>;
    auto failure = [](Core::Problem problem) {
        return Result<Entries>(Core::Error<>(std::move(problem)));
    };
    Entries entries;
    entries.push_back({record.photo_name, record.photo});

    auto mask_png = Vision::encode_mask_png(analysis.mask);
    if (!mask_png)
        return failure(Core::Problem("inspection.no_mask",
                                     "Cannot write mask.png: the analysis has no mask."));
    entries.push_back({"mask.png", std::move(*mask_png)});

    if (auto added = add_text_entry(entries, "camera.json",
                                    camera_json(record.camera, analysis.frame.width, rotated));
        added.isError()) {
        return failure(std::move(added).error());
    }
    if (auto added = add_text_entry(entries, "parameters.json", parameters_json(record));
        added.isError()) {
        return failure(std::move(added).error());
    }
    if (!analysis.objects.empty()) {
        auto dxf = Tools::to_dxf(analysis.objects, "OBJECT");
        if (dxf.isError())
            return failure(entry_failed("objects.dxf", std::move(dxf).error()));
        entries.push_back({"objects.dxf", std::move(dxf)()});
    }
    if (auto added = add_text_entry(entries, "result.json", result_json(analysis));
        added.isError()) {
        return failure(std::move(added).error());
    }
    return Result<Entries>(std::move(entries));
}

Result<Record> from_entries_impl(std::vector<Tools::ZipEntry> const& entries,
                                 std::string const& zip_name) {
    auto failure = [](Core::Problem problem) {
        return Result<Record>(Core::Error<>(std::move(problem)));
    };
    // "original." (any image suffix) by prefix, every other entry by its exact name
    auto find = [&](std::string const& name) -> Tools::ZipEntry const* {
        bool const prefix = name.ends_with('.');
        for (auto const& e : entries) {
            if (prefix ? e.name.starts_with(name) : e.name == name)
                return &e;
        }
        return nullptr;
    };
    auto missing = [&](char const* entry) {
        return failure(Core::Problem("inspection.missing_file",
                                     "{zip} is not an inspection: it has no {entry}.",
                                     {{"zip", zip_name}, {"entry", entry}}));
    };
    auto parse = [&](Tools::ZipEntry const& e) { return Tools::parse_json(e.data); };

    Record record;
    auto const* original = find("original.");
    if (original == nullptr) {
        return failure(Core::Problem("inspection.missing_photo",
                                     "{zip} is not an inspection: it has no original image.",
                                     {{"zip", zip_name}}));
    }
    record.photo_name = original->name.substr(original->name.rfind('/') + 1);  // npos + 1 is 0
    record.photo = original->data;

    // the parameters tell the format: objects, or an older hide inspection
    auto const* parameters_entry = find("parameters.json");
    if (parameters_entry == nullptr)
        return missing("parameters.json");
    auto parameters = parse(*parameters_entry);
    if (parameters.isError())
        return failure(entry_failed("parameters.json", std::move(parameters).error()));
    std::string const format = format_of(parameters());
    record.legacy = starts_with(format, "contorno-hide-") || starts_with(format, "conqueror-");

    auto const* camera_entry = find("camera.json");
    if (camera_entry == nullptr)
        return missing("camera.json");
    auto camera = parse(*camera_entry);
    if (camera.isError())
        return failure(entry_failed("camera.json", std::move(camera).error()));
    Json const& cam = camera();
    Reader c("camera.json", cam);
    c.format("contorno-camera");
    int const version = c.version(4);
    double saved_width_px = 1;  // versions 1 and 2: the width the pixel size belongs to
    if (version >= 2) {
        auto& lens = record.camera.lens;
        lens.distance_mm = c.within(&cam, "distance_mm", 1, 100000);
        lens.focal_length_mm = c.within(&cam, "focal_length_mm", 0.1, 2000);
        // derived (see below): not limited to the dialog's range
        if (version == 4) {
            lens.pixel_width_um = c.positive(&cam, "pixel_width_um");
            lens.pixel_height_um = c.positive(&cam, "pixel_height_um");
        } else {
            lens.pixel_width_um = lens.pixel_height_um =
                c.positive(&cam, "pixel_size_um");  // square before version 4
        }
        if (version >= 3) {
            record.camera.focal_source = c.source(&cam, "focal_length_source");
            record.camera.pixel_source = c.source(&cam, "pixel_size_source");
            c.within(&cam, "image_width_px", 1, 100000,
                     true);  // checked, not used: the photo gives the width
        } else {
            saved_width_px = c.within(&cam, "sensor_width_px", 1, 100000, true);
        }
    } else {
        // version 1: distance and field of view, same scale with the default pixel size
        Vision::CameraConfig old;
        old.distance_mm = c.positive(&cam, "distance_mm");
        old.fov_deg = c.positive(&cam, "fov_deg");
        old.image_width_px = static_cast<int>(std::llround(c.positive(&cam, "image_width_px")));
        record.camera.lens = Vision::from_legacy(old);
        saved_width_px = old.image_width_px;
    }
    if (c.problem())
        return failure(*c.problem());
    if (version < 3) {
        // Older files scaled by the width they were saved with over the photo's width: keep the
        // area they showed
        auto const shown = Vision::shown_image_size(record.photo);
        if (!shown)
            return failure(
                Core::Problem("inspection.photo_unreadable", "original image: cannot be read."));
        double const k = saved_width_px / shown->width;
        record.camera.lens.pixel_width_um *= k;
        record.camera.lens.pixel_height_um *= k;
    }
    if (record.legacy)
        return Result<Record>(std::move(record));  // its parameters and result are ignored

    Json const& par = parameters();
    Reader p("parameters.json", par);
    p.format("contorno-objects-parameters");
    p.version(1);
    record.min_area_mm2 = Vision::snap_min_area(p.number(&par, "min_area_mm2"));
    record.outline_tolerance_mm = p.number(&par, "outline_tolerance_mm");
    if (p.problem())
        return failure(*p.problem());

    auto const* result_entry = find("result.json");
    if (result_entry == nullptr)
        return missing("result.json");
    auto result = parse(*result_entry);
    if (result.isError())
        return failure(entry_failed("result.json", std::move(result).error()));
    Json const& root = result();
    Reader r("result.json", root);
    r.format("contorno-objects-result");
    int const result_version = r.version(2);
    Vision::ObjectsAnalysis a;
    Json const* image = r.object("image");
    a.frame.width = r.count(image, "width");
    a.frame.height = r.count(image, "height");
    if (result_version == 1) {                                 // one scale for both sides
        a.frame.mm_per_px_x = r.positive(image, "mm_per_px");  // to_px divides by it
        a.frame.mm_per_px_y = a.frame.mm_per_px_x;
    } else {
        a.frame.mm_per_px_x = r.positive(image, "mm_per_px_x");
        a.frame.mm_per_px_y = r.positive(image, "mm_per_px_y");
    }
    a.area_mm2 = r.non_negative(&root, "area_mm2");
    auto const* objects = root.find("objects");
    if (objects == nullptr || !objects->is_array()) {
        r.polygon(nullptr, "objects");
    } else {
        for (auto const& o : *objects->as_array())
            a.objects.push_back(r.polygon(&o, "objects"));
    }
    if (r.problem())
        return failure(*r.problem());

    // the mask shown on the right: the readout still equals its white area
    auto const* mask_entry = find("mask.png");
    if (mask_entry == nullptr)
        return missing("mask.png");
    auto mask = Vision::decode_mask_png(mask_entry->data);
    if (!mask)
        return failure(Core::Problem("inspection.mask_not_png", "mask.png: not a PNG image."));
    if (mask->width != a.frame.width || mask->height != a.frame.height) {
        auto const size = [](int w, int h) {
            return std::to_string(w) + " x " + std::to_string(h);
        };
        return failure(
            Core::Problem("inspection.mask_size",
                          "mask.png: {mask} pixels, but the image in result.json is {image}.",
                          {{"mask", size(mask->width, mask->height)},
                           {"image", size(a.frame.width, a.frame.height)}}));
    }
    if (std::any_of(mask->data.begin(), mask->data.end(),
                    [](std::uint8_t v) { return v != 0 && v != 255; })) {
        return failure(Core::Problem("inspection.mask_values",
                                     "mask.png: pixels must be black (0) or white (255)."));
    }
    a.mask = std::move(*mask);
    record.result = std::move(a);
    return Result<Record>(std::move(record));
}

/// An exception from outside the libraries, its text as it is.
Core::Problem unexpected(std::exception const& e) {
    return Core::Problem::raw(e.what());
}

}  // namespace

std::string photo_name(std::string const& photo_path) {
    std::size_t const slash = photo_path.find_last_of("/\\");
    std::string const file = slash == std::string::npos ? photo_path : photo_path.substr(slash + 1);
    std::size_t const dot = file.rfind('.');
    if (dot == std::string::npos || dot + 1 == file.size())
        return "original";
    std::string suffix = file.substr(dot + 1);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return "original." + suffix;
}

Result<std::vector<Tools::ZipEntry>> to_entries(Record const& record,
                                                Vision::ObjectsAnalysis const& analysis,
                                                bool rotated) noexcept {
    try {
        return to_entries_impl(record, analysis, rotated);
    } catch (std::exception const& e) {
        return Core::error_result<std::vector<Tools::ZipEntry>>([&] { return unexpected(e); });
    } catch (...) {
        return Core::error_result<std::vector<Tools::ZipEntry>>(
            [] { return Core::Problem("unknown_error", "unknown error"); });
    }
}

Result<Record> from_entries(std::vector<Tools::ZipEntry> const& entries,
                            std::string const& zip_name) noexcept {
    try {
        return from_entries_impl(entries, zip_name);
    } catch (std::exception const& e) {
        return Core::error_result<Record>([&] { return unexpected(e); });
    } catch (...) {
        return Core::error_result<Record>(
            [] { return Core::Problem("unknown_error", "unknown error"); });
    }
}

}  // namespace Inspection
