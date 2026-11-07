#include <gtest/gtest.h>

#include "LegacyInspection.h"
#include "TempDir.h"
#include "model/CameraMessage.h"
#include "model/ImageSize.h"
#include "model/Inspection.h"
#include "model/LibraryText.h"
#include "model/Measure.h"
#include "model/Utf8Path.h"

#include <Tools/Dxf.h>
#include <Tools/Zip.h>
#include <Vision/Image.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace test_Inspection {

namespace fs = std::filesystem;

fs::path const calf = fs::path(INDICATOR_SAMPLES_DIR) / "leather/calf_black.jpg";
fs::path const bad_result_zip = fs::path(INDICATOR_SAMPLES_DIR) / "cowhide_analysis_bad_result.zip";

/// 200 x 100 pixels at 0.5 mm/px: a filled 60 px square (columns and rows
/// 20..79) with a 20 px hole (40..59), and its polygon in the image mm frame.
Vision::ObjectsAnalysis square_with_hole() {
    Vision::ObjectsAnalysis a;
    a.frame = {200, 100, 0.5, 0.5};
    a.mask = {200, 100, std::vector<std::uint8_t>(200 * 100, 0)};
    int pixels = 0;
    for (int row = 20; row < 80; ++row) {
        for (int col = 20; col < 80; ++col) {
            bool const hole = row >= 40 && row < 60 && col >= 40 && col < 60;
            if (!hole) {
                a.mask.data[static_cast<std::size_t>(row * 200 + col)] = 255;
                ++pixels;
            }
        }
    }
    a.area_mm2 = pixels * 0.25;
    auto const mm = [](double v) { return static_cast<std::int64_t>(v * Pot::units_per_mm); };
    Pot::Polygon64 polygon;
    polygon.outer = {{mm(10), mm(10)}, {mm(40), mm(10)}, {mm(40), mm(40)}, {mm(10), mm(40)}};    // CCW
    polygon.holes = {{{mm(20), mm(20)}, {mm(20), mm(30)}, {mm(30), mm(30)}, {mm(30), mm(20)}}};  // CW
    a.objects = {polygon};
    return a;
}

InspectionFiles files_for_calf() {
    InspectionFiles files;
    files.photo_path = calf;
    files.camera.lens = Vision::LensCamera {1200, 16, 3.45};
    files.min_area_mm2 = 500;
    return files;
}

std::vector<std::string> names(std::vector<Tools::ZipEntry> const& entries) {
    std::vector<std::string> out;
    for (auto const& e : entries) out.push_back(e.name);
    return out;
}

TEST(WxInspection, EntryNames) {
    auto const a = square_with_hole();
    auto entries = inspection_entries(files_for_calf(), a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    EXPECT_EQ(names(entries()), (std::vector<std::string> {"original.jpg", "mask.png", "camera.json", "parameters.json",
                                              "objects.dxf", "result.json"}));
    auto const& e = entries();
    auto const mask = Vision::decode_mask_png(e[1].data);
    ASSERT_TRUE(mask);
    EXPECT_EQ(mask->width, 200);
    EXPECT_EQ(mask->height, 100);
    EXPECT_EQ(mask->data, a.mask.data);  // 8-bit grey
    for (char const* key : {"\"contorno-camera\"", "\"version\": 4", "\"distance_mm\"", "\"focal_length_mm\"",
                            "\"pixel_width_um\"", "\"pixel_height_um\"", "\"focal_length_source\": \"settings\"",
                            "\"pixel_size_source\": \"settings\"", "\"fov_deg\"", "\"image_width_px\""}) {
        EXPECT_NE(e[2].data.find(key), std::string::npos) << key;
    }
    for (char const* key : {"\"contorno-objects-parameters\"", "\"min_area_mm2\": 500", "\"outline_tolerance_mm\": 0.5"}) {
        EXPECT_NE(e[3].data.find(key), std::string::npos) << key;
    }
    auto rings = Tools::parse_dxf(e[4].data);
    ASSERT_FALSE(rings.isError()) << rings.error();
    ASSERT_EQ(rings().size(), 2u);  // the outer ring and the hole
    EXPECT_EQ(rings()[0].vertices, a.objects[0].outer);  // in the image mm frame, not shifted
    EXPECT_NE(e[4].data.find("OBJECT"), std::string::npos);
    for (char const* key : {"\"contorno-objects-result\"", "\"version\": 2", "\"image\"", "\"mm_per_px_x\"", "\"mm_per_px_y\"", "\"area_mm2\"", "\"objects\""}) {
        EXPECT_NE(e[5].data.find(key), std::string::npos) << key;
    }
}

TEST(WxInspection, RoundTrip) {
    TempDir dir;
        auto const a = square_with_hole();
    auto const files = files_for_calf();
    auto entries = inspection_entries(files, a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    fs::path const zip = dir.path / "inspection.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries()).isError());

    TempDir into;
    auto read = unpack_inspection(zip, into.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    auto const& f = read();
    EXPECT_FALSE(f.legacy);
    EXPECT_TRUE(fs::exists(f.photo_path));
    EXPECT_EQ(f.camera.lens.distance_mm, files.camera.lens.distance_mm);
    EXPECT_EQ(f.camera.lens.focal_length_mm, files.camera.lens.focal_length_mm);
    EXPECT_EQ(f.camera.lens.pixel_width_um, files.camera.lens.pixel_width_um);
    EXPECT_EQ(f.camera.lens.pixel_height_um, files.camera.lens.pixel_height_um);
    EXPECT_EQ(f.min_area_mm2, 500);
    ASSERT_TRUE(f.result);
    auto const& r = *f.result;
    EXPECT_EQ(r.frame.width, 200);
    EXPECT_EQ(r.frame.height, 100);
    EXPECT_EQ(r.frame.mm_per_px_x, 0.5);
    EXPECT_EQ(r.frame.mm_per_px_y, 0.5);
    EXPECT_EQ(r.area_mm2, a.area_mm2);
    ASSERT_EQ(r.objects.size(), 1u);
    EXPECT_EQ(r.objects[0].outer, a.objects[0].outer);
    EXPECT_EQ(r.objects[0].holes, a.objects[0].holes);
    EXPECT_EQ(r.mask.width, 200);
    EXPECT_EQ(r.mask.height, 100);
    EXPECT_EQ(r.mask.data, a.mask.data);
}

TEST(WxInspection, MinAreaSnapsToPresets) {
    TempDir dir;
    auto const a = square_with_hole();
    auto files = files_for_calf();
    files.min_area_mm2 = 420;
    auto entries = inspection_entries(files, a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    fs::path const zip = dir.path / "snap.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries()).isError());
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    EXPECT_EQ(read().min_area_mm2, 500);
}

TEST(WxInspection, NoObjectsHasNoDxf) {
    Vision::ObjectsAnalysis a;
    a.frame = {200, 100, 0.5, 0.5};
    a.mask = {200, 100, std::vector<std::uint8_t>(200 * 100, 0)};
    auto entries = inspection_entries(files_for_calf(), a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    EXPECT_EQ(names(entries()), (std::vector<std::string> {"original.jpg", "mask.png", "camera.json", "parameters.json",
                                              "result.json"}));

    // and it opens again with no objects
    TempDir dir;
    fs::path const zip = dir.path / "empty.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries()).isError());
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    ASSERT_TRUE(read().result);
    EXPECT_TRUE(read().result->objects.empty());
    EXPECT_EQ(read().result->area_mm2, 0);
}

TEST(WxInspection, LegacyHideZipOpens) {
    TempDir dir;
        Vision::CameraConfig const old {1500, 60, 4000};
    auto const entries = legacy_inspection_entries(calf, old);
    ASSERT_FALSE(entries.empty());
    fs::path const zip = dir.path / "hide.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries).isError());

    TempDir into;
    auto read = unpack_inspection(zip, into.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    auto const& f = read();
    EXPECT_TRUE(f.legacy);
    EXPECT_FALSE(f.result);
    EXPECT_TRUE(fs::exists(f.photo_path));
    auto const scale = Vision::mm_per_px(old, shown_size(f.photo_path)->width);  // as Vision scaled it: by the photo's width
    ASSERT_FALSE(scale.isError());
    EXPECT_NEAR(Vision::lens_scale(f.camera.lens).x, scale(), 1e-9);
}

// the demo inspections shipped with the samples must open
TEST(WxInspection, SampleZipsOpen) {
    int found = 0;
    for (auto const& entry : fs::recursive_directory_iterator(INDICATOR_SAMPLES_DIR)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".zip") continue;
        TempDir into;
        auto const read = unpack_inspection(entry.path(), into.path);
        EXPECT_FALSE(read.isError()) << utf8(entry.path());
        ++found;
    }
    EXPECT_GT(found, 0);
}

TEST(WxInspection, MissingMaskIsAnError) {
    TempDir dir;
    auto const a = square_with_hole();
    auto entries = inspection_entries(files_for_calf(), a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    auto without = entries();
    without.erase(std::remove_if(without.begin(), without.end(), [](auto const& e) { return e.name == "mask.png"; }),
                  without.end());
    fs::path const zip = dir.path / "no_mask.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), without).isError());
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_TRUE(read.isError());
    EXPECT_NE(read.error().english().find("mask.png"), std::string::npos) << read.error().english();

    // a mask of another size than result.json's image
    auto other = entries();
    Vision::Mask small {20, 10, std::vector<std::uint8_t>(200, 0)};
    for (auto& e : other) {
        if (e.name != "mask.png") continue;
        auto const png = Vision::encode_mask_png(small);
        ASSERT_TRUE(png);
        e.data = *png;
    }
    fs::path const zip2 = dir.path / "small_mask.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip2), other).isError());
    auto read2 = unpack_inspection(zip2, dir.path);
    ASSERT_TRUE(read2.isError());
    EXPECT_NE(read2.error().english().find("mask.png"), std::string::npos) << read2.error().english();
}

// each change of a valid zip is an error naming its entry
TEST(WxInspection, InvalidValuesAreErrors) {
    TempDir dir;
    auto const a = square_with_hole();
    auto entries = inspection_entries(files_for_calf(), a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    ASSERT_EQ(a.area_mm2, 800);

    auto replaced = [&](std::string const& entry, std::string const& from, std::string const& to) {
        auto out = entries();
        for (auto& e : out) {
            if (e.name != entry) continue;
            auto const at = e.data.find(from);
            EXPECT_NE(at, std::string::npos) << from;
            if (at != std::string::npos) e.data.replace(at, from.size(), to);
        }
        return out;
    };
    std::string grey_png;
    {
        Vision::Mask grey = a.mask;
        grey.data[0] = 128;
        auto const png = Vision::encode_mask_png(grey);
        ASSERT_TRUE(png);
        grey_png = *png;
    }
    auto grey_mask = entries();
    for (auto& e : grey_mask) {
        if (e.name == "mask.png") e.data = grey_png;
    }
    auto renamed_mask = entries();
    for (auto& e : renamed_mask) {
        if (e.name == "mask.png") e.name = "mask.png.old";
    }

    struct Case {
        char const* what;
        std::vector<Tools::ZipEntry> entries;
        char const* named;
    };
    std::vector<Case> const cases {
        {"negative area", replaced("result.json", "\"area_mm2\": 800", "\"area_mm2\": -1"), "result.json"},
        {"fractional width", replaced("result.json", "\"width\": 200", "\"width\": 200.5"), "result.json"},
        {"huge height", replaced("result.json", "\"height\": 100", "\"height\": 1e12"), "result.json"},
        {"grey mask", grey_mask, "mask.png"},
        // the camera dialog's ranges
        {"distance under 1 mm", replaced("camera.json", "\"distance_mm\": 1200", "\"distance_mm\": 0.5"),
         "camera.json"},
        {"distance over 100000 mm", replaced("camera.json", "\"distance_mm\": 1200", "\"distance_mm\": 100001"),
         "camera.json"},
        {"focal length under 0.1 mm", replaced("camera.json", "\"focal_length_mm\": 16", "\"focal_length_mm\": 0.05"),
         "camera.json"},
        {"focal length over 2000 mm", replaced("camera.json", "\"focal_length_mm\": 16", "\"focal_length_mm\": 2001"),
         "camera.json"},
        {"pixel size zero", replaced("camera.json", "\"pixel_width_um\": 3.45", "\"pixel_width_um\": 0"),
         "camera.json"},
        {"unknown pixel size source", replaced("camera.json", "\"pixel_size_source\": \"settings\"", "\"pixel_size_source\": \"camera\""),
         "pixel_size_source"},
        {"unknown focal length source", replaced("camera.json", "\"focal_length_source\": \"settings\"", "\"focal_length_source\": \"exif\""),
         "focal_length_source"},
        {"mask.png only by prefix", renamed_mask, "mask.png"},
    };
    for (auto const& c : cases) {
        fs::path const zip = dir.path / "case.zip";
        fs::remove(zip);
        ASSERT_FALSE(Tools::write_zip(utf8(zip), c.entries).isError()) << c.what;
        auto read = unpack_inspection(zip, dir.path);
        ASSERT_TRUE(read.isError()) << c.what;
        EXPECT_NE(read.error().english().find(c.named), std::string::npos) << c.what << ": " << read.error().english();
    }
}

TEST(WxInspection, V3RoundTripKeepsSources) {
    TempDir dir;
    auto files = files_for_calf();
    files.camera = Vision::PhotoCamera {{1500, 4.2, 1.4}, Vision::CameraSource::photo, Vision::CameraSource::photo_35mm};
    auto entries = inspection_entries(files, square_with_hole());
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    fs::path const zip = dir.path / "v3.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries()).isError());
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    auto const& c = read().camera;
    EXPECT_EQ(c.lens.distance_mm, 1500);
    EXPECT_EQ(c.lens.focal_length_mm, 4.2);
    EXPECT_EQ(c.lens.pixel_width_um, 1.4);
    EXPECT_EQ(c.focal_source, Vision::CameraSource::photo);
    EXPECT_EQ(c.pixel_source, Vision::CameraSource::photo);
}

// the sample export was saved as version 2 (sensor_width_px, no sources)
TEST(WxInspection, V2StillOpens) {
    fs::path const zip = bad_result_zip;
    TempDir dir;
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    auto const& c = read().camera;
    EXPECT_GT(c.lens.pixel_width_um, 0);
    EXPECT_EQ(c.focal_source, Vision::CameraSource::settings);
    EXPECT_EQ(c.pixel_source, Vision::CameraSource::settings);
}

/// Writes `entries` as a zip and reads it back.
InspectionResult<InspectionFiles> through_zip(std::vector<Tools::ZipEntry> const& entries, TempDir const& dir) {
    fs::path const zip = dir.path / "through.zip";
    auto written = Tools::write_zip(utf8(zip), entries);
    if (written.isError()) return InspectionResult<InspectionFiles>(Core::Error<Message>(from_library(written.error())));
    return unpack_inspection(zip, dir.path);
}

TEST(WxInspection, NonSquareRoundTrip) {
    TempDir dir;
    auto files = files_for_calf();
    files.camera = Vision::PhotoCamera {{1500, 4.2, 3.76, 4.20}, Vision::CameraSource::photo, Vision::CameraSource::photo};
    auto a = square_with_hole();
    a.frame = {200, 100, 0.5, 0.6};
    auto entries = inspection_entries(files, a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    auto read = through_zip(entries(), dir);
    ASSERT_FALSE(read.isError()) << read.error().english();
    EXPECT_EQ(read().camera.lens.pixel_width_um, 3.76);
    EXPECT_EQ(read().camera.lens.pixel_height_um, 4.20);
    ASSERT_TRUE(read().result);
    EXPECT_EQ(read().result->frame.mm_per_px_x, 0.5);
    EXPECT_EQ(read().result->frame.mm_per_px_y, 0.6);
}

TEST(WxInspection, ResultV1ReadsSquare) {
    TempDir dir;
    auto entries = inspection_entries(files_for_calf(), square_with_hole());
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    for (auto& e : entries()) {
        if (e.name != "result.json") continue;
        auto replace = [&](std::string const& from, std::string const& to) {
            auto const at = e.data.find(from);
            ASSERT_NE(at, std::string::npos) << from;
            e.data.replace(at, from.size(), to);
        };
        replace("\"version\": 2", "\"version\": 1");
        replace("\"mm_per_px_x\": 0.5,", "\"mm_per_px\": 0.4");
        auto const y = e.data.find("\"mm_per_px_y\"");
        ASSERT_NE(y, std::string::npos);
        e.data.erase(y, e.data.find('\n', y) - y + 1);
    }
    auto read = through_zip(entries(), dir);
    ASSERT_FALSE(read.isError()) << read.error().english();
    ASSERT_TRUE(read().result);
    EXPECT_EQ(read().result->frame.mm_per_px_x, 0.4);
    EXPECT_EQ(read().result->frame.mm_per_px_y, 0.4);
}

TEST(WxInspection, V3CameraReadsSquare) {
    TempDir dir;
    auto entries = inspection_entries(files_for_calf(), square_with_hole());
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    for (auto& e : entries()) {
        if (e.name != "camera.json") continue;
        e.data =
            "{\"format\": \"contorno-camera\", \"version\": 3, \"distance_mm\": 1200, \"focal_length_mm\": 16,"
            " \"pixel_size_um\": 2.5, \"focal_length_source\": \"settings\", \"pixel_size_source\": \"settings\","
            " \"image_width_px\": 584, \"fov_deg\": 10}\n";
    }
    auto read = through_zip(entries(), dir);
    ASSERT_FALSE(read.isError()) << read.error().english();
    EXPECT_EQ(read().camera.lens.pixel_width_um, 2.5);
    EXPECT_EQ(read().camera.lens.pixel_height_um, 2.5);
}

// a saved pixel size is derived (older files rescaled it): any positive value reopens
TEST(WxInspection, LargeDerivedPixelSizeReopens) {
    TempDir dir;
    auto files = files_for_calf();
    files.camera.lens.pixel_width_um = files.camera.lens.pixel_height_um = 150;
    auto entries = inspection_entries(files, square_with_hole());
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    fs::path const zip = dir.path / "large.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries()).isError());
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    EXPECT_EQ(read().camera.lens.pixel_width_um, 150);
}

// a saved inspection shows the area it was saved with: version 2 scaled by sensor width / photo width
TEST(WxInspection, V2KeepsSavedScale) {
    fs::path const zip = bad_result_zip;
    TempDir dir;
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    ASSERT_TRUE(read().result);
    EXPECT_NEAR(Vision::lens_scale(read().camera.lens).x, read().result->frame.mm_per_px_x, 1e-6);
}

// version 1 (distance 1500, fov 60, width 4000) scaled by 4000 / photo width, as Vision did
TEST(WxInspection, V1KeepsLegacyScale) {
    TempDir dir;
    fs::path const zip = dir.path / "legacy.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), legacy_inspection_entries(calf, {1500, 60, 4000})).isError());
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    ASSERT_TRUE(read().legacy);
    auto const photo = shown_size(read().photo_path);
    ASSERT_TRUE(photo);
    auto const expected = Vision::mm_per_px({1500, 60, 4000}, photo->width);
    ASSERT_FALSE(expected.isError()) << expected.error();
    EXPECT_NEAR(Vision::lens_scale(read().camera.lens).x, expected(), 1e-9);
}

TEST(WxInspection, ReopenedV2ExportsConsistentCamera) {
    fs::path const zip = bad_result_zip;
    TempDir dir;
    auto first = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(first.isError()) << first.error().english();
    ASSERT_TRUE(first().result);
    auto entries = inspection_entries(first(), *first().result);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    fs::path const again = dir.path / "again.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(again), entries()).isError());
    TempDir dir2;
    auto second = unpack_inspection(again, dir2.path);
    ASSERT_FALSE(second.isError()) << second.error().english();
    ASSERT_TRUE(second().result);
    EXPECT_NEAR(Vision::lens_scale(second().camera.lens).x, second().result->frame.mm_per_px_x, 1e-6);
    EXPECT_NEAR(Vision::lens_scale(second().camera.lens).x, first().result->frame.mm_per_px_x, 1e-6);
}

// an older export's processed.png is ignored: the outlines come from result.json
TEST(WxInspection, OlderZipWithProcessedOpens) {
    TempDir dir;
    auto const a = square_with_hole();
    auto entries = inspection_entries(files_for_calf(), a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    auto older = entries();
    older.insert(older.begin() + 1, {"processed.png", older[1].data});
    fs::path const zip = dir.path / "older.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), older).isError());
    auto read = unpack_inspection(zip, dir.path);
    ASSERT_FALSE(read.isError()) << read.error().english();
    ASSERT_TRUE(read().result);
    EXPECT_EQ(read().result->objects[0].outer, a.objects[0].outer);
    EXPECT_EQ(read().result->mask.data, a.mask.data);
}

// zip and unpack folder both under a non-ASCII name
TEST(WxInspection, NonAsciiPathRoundTrips) {
    TempDir dir(path_from_utf8("K\xC3\xA4lber_\xD8\xAC\xD9\x84\xD8\xAF"));
    auto const a = square_with_hole();
    auto const files = files_for_calf();
    auto entries = inspection_entries(files, a);
    ASSERT_FALSE(entries.isError()) << entries.error().english();
    fs::path const zip = dir.path / "inspection.zip";
    ASSERT_FALSE(Tools::write_zip(utf8(zip), entries()).isError());
    fs::path const into = dir.path / "unpacked";
    fs::create_directories(into);
    auto read = unpack_inspection(zip, into);
    ASSERT_FALSE(read.isError()) << read.error().english();
    EXPECT_TRUE(fs::exists(read().photo_path));
    EXPECT_EQ(read().photo_path.parent_path(), into);
    ASSERT_TRUE(read().result);
    EXPECT_EQ(read().result->objects[0].outer, a.objects[0].outer);
    EXPECT_EQ(read().result->mask.data, a.mask.data);
    EXPECT_EQ(read().min_area_mm2, 500);
}

}  // namespace test_Inspection
