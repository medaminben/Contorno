#include <Inspection/Inspection.h>
#include <Tools/Json.h>
#include <Vision/Image.h>
#include <Vision/Lens.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace test_Inspection {

using Tools::ZipEntry;

namespace {

/// A 20 x 10 analysis: a 10 x 6 white square with a 2 x 2 hole, 0.5 mm per pixel.
Vision::ObjectsAnalysis square_with_hole() {
    Vision::ObjectsAnalysis a;
    a.frame = {20, 10, 0.5, 0.5};
    a.mask = {20, 10, std::vector<std::uint8_t>(200, 0)};
    for (int row = 2; row < 8; ++row) {
        for (int col = 5; col < 15; ++col)
            a.mask.data[static_cast<std::size_t>(row * 20 + col)] = 255;
    }
    for (int row = 4; row < 6; ++row) {
        for (int col = 9; col < 11; ++col)
            a.mask.data[static_cast<std::size_t>(row * 20 + col)] = 0;
    }
    auto const mm = [](double v) { return static_cast<std::int64_t>(v * Pot::units_per_mm); };
    a.objects = {{{{mm(2.5), mm(1)}, {mm(7.5), mm(1)}, {mm(7.5), mm(4)}, {mm(2.5), mm(4)}},
                  {{{mm(4.5), mm(2)}, {mm(4.5), mm(3)}, {mm(5.5), mm(3)}, {mm(5.5), mm(2)}}}}};
    a.area_mm2 = 56 * 0.25;
    return a;
}

/// A photo: any image OpenCV reads (here a 20 x 10 grey PNG).
std::string photo() {
    return *Vision::encode_mask_png(square_with_hole().mask);
}

Inspection::Record record() {
    Inspection::Record r;
    r.photo_name = "original.png";
    r.photo = photo();
    r.camera.lens = {2000, 16, 2.4, 3.2};
    r.camera.focal_source = Vision::CameraSource::photo;
    r.min_area_mm2 = 500;
    return r;
}

std::vector<ZipEntry> entries() {
    auto e = Inspection::to_entries(record(), square_with_hole(), false);
    EXPECT_FALSE(e.isError()) << (e.isError() ? e.error().text() : "");
    return e.isError() ? std::vector<ZipEntry> {} : std::move(e)();
}

ZipEntry* entry(std::vector<ZipEntry>& all, std::string const& name) {
    for (auto& e : all) {
        if (e.name == name)
            return &e;
    }
    return nullptr;
}

std::vector<ZipEntry> without(std::vector<ZipEntry> all, std::string const& name) {
    std::erase_if(all, [&](ZipEntry const& e) { return e.name == name; });
    return all;
}

/// The camera.json of a version-1 inspection.
std::string camera_v1(double distance, double fov, int width) {
    return "{\"format\": \"contorno-camera\", \"version\": 1, \"distance_mm\": " +
           std::to_string(distance) + ", \"fov_deg\": " + std::to_string(fov) +
           ", \"image_width_px\": " + std::to_string(width) + "}";
}

}  // namespace

TEST(Inspection, PhotoName) {
    EXPECT_EQ(Inspection::photo_name("/a/b/photo.JPG"), "original.jpg");
    EXPECT_EQ(Inspection::photo_name("C:\\x\\y.Png"), "original.png");
    EXPECT_EQ(Inspection::photo_name("/a/noext"), "original");
}

TEST(Inspection, EntryNamesInOrder) {
    auto const all = entries();
    std::vector<std::string> names;
    for (auto const& e : all)
        names.push_back(e.name);
    EXPECT_EQ(names, (std::vector<std::string> {"original.png", "mask.png", "camera.json",
                                                "parameters.json", "objects.dxf", "result.json"}));
    EXPECT_EQ(all[0].data, photo());  // the photo untouched
}

TEST(Inspection, NoObjectsNoDxf) {
    auto a = square_with_hole();
    a.objects.clear();
    auto const e = Inspection::to_entries(record(), a, false);
    ASSERT_FALSE(e.isError());
    for (auto const& x : e())
        EXPECT_NE(x.name, "objects.dxf");
}

TEST(Inspection, NoMaskIsAProblem) {
    auto a = square_with_hole();
    a.mask = {};
    auto const e = Inspection::to_entries(record(), a, false);
    ASSERT_TRUE(e.isError());
    EXPECT_EQ(e.error().code(), "inspection.no_mask");
}

TEST(Inspection, RoundTrip) {
    auto const read = Inspection::from_entries(entries(), "x.zip");
    ASSERT_FALSE(read.isError()) << read.error();
    auto const& r = read();
    EXPECT_FALSE(r.legacy);
    EXPECT_EQ(r.photo_name, "original.png");
    EXPECT_EQ(r.photo, photo());
    EXPECT_EQ(r.camera.lens.pixel_width_um, 2.4);
    EXPECT_EQ(r.camera.lens.pixel_height_um, 3.2);
    EXPECT_EQ(r.camera.focal_source, Vision::CameraSource::photo);
    EXPECT_EQ(r.camera.pixel_source, Vision::CameraSource::settings);
    EXPECT_EQ(r.min_area_mm2, 500);
    EXPECT_EQ(r.outline_tolerance_mm, 0.5);
    ASSERT_TRUE(r.result);
    auto const a = square_with_hole();
    EXPECT_EQ(r.result->mask.data, a.mask.data);
    EXPECT_EQ(r.result->objects.size(), 1u);
    EXPECT_EQ(r.result->objects[0].holes.size(), 1u);
    EXPECT_EQ(r.result->area_mm2, a.area_mm2);
    EXPECT_EQ(r.result->frame.mm_per_px_y, 0.5);
}

TEST(Inspection, ToleranceIsWrittenDown) {
    auto rec = record();
    rec.outline_tolerance_mm = 0.25;
    auto e = Inspection::to_entries(rec, square_with_hole(), false);
    ASSERT_FALSE(e.isError());
    auto const parameters = Tools::parse_json(entry(e(), "parameters.json")->data);
    ASSERT_FALSE(parameters.isError());
    EXPECT_EQ(parameters().find("outline_tolerance_mm")->as_number(), 0.25);
}

TEST(Inspection, MinAreaSnapsToAPreset) {
    auto all = entries();
    entry(all, "parameters.json")->data =
        "{\"format\": \"contorno-objects-parameters\", \"version\": 1, \"min_area_mm2\": 480, "
        "\"outline_tolerance_mm\": 0.5}";
    auto const read = Inspection::from_entries(all, "x.zip");
    ASSERT_FALSE(read.isError()) << read.error();
    EXPECT_EQ(read().min_area_mm2, 500);
}

TEST(Inspection, MissingEntriesNameTheZip) {
    for (char const* name : {"parameters.json", "camera.json", "result.json", "mask.png"}) {
        auto const read = Inspection::from_entries(without(entries(), name), "x.zip");
        ASSERT_TRUE(read.isError()) << name;
        EXPECT_EQ(read.error().code(), "inspection.missing_file") << name;
        EXPECT_EQ(read.error().text(),
                  std::string("x.zip is not an inspection: it has no ") + name + ".");
    }
    auto const read = Inspection::from_entries(without(entries(), "original.png"), "x.zip");
    ASSERT_TRUE(read.isError());
    EXPECT_EQ(read.error().code(), "inspection.missing_photo");
}

TEST(Inspection, InvalidValueNamesEntryAndKey) {
    auto all = entries();
    entry(all, "camera.json")->data = "{\"format\": \"contorno-camera\", \"version\": 9}";
    auto const read = Inspection::from_entries(all, "x.zip");
    ASSERT_TRUE(read.isError());
    EXPECT_EQ(read.error().code(), "inspection.must_be");
    EXPECT_EQ(read.error().text(), "camera.json: \"version\" must be 1 to 4");
    EXPECT_EQ(read.error().value("expected")->nested->code(), "inspection.what.version");
}

TEST(Inspection, BrokenJsonNamesTheEntry) {
    auto all = entries();
    entry(all, "result.json")->data = "{";
    auto const read = Inspection::from_entries(all, "x.zip");
    ASSERT_TRUE(read.isError());
    EXPECT_EQ(read.error().code(), "inspection.entry");
    EXPECT_EQ(read.error().value("entry")->plain, "result.json");
    EXPECT_EQ(read.error().value("reason")->nested->code(), "json.at");
}

TEST(Inspection, MaskMustMatchAndBeBlackOrWhite) {
    auto all = entries();
    entry(all, "mask.png")->data = "not a png";
    EXPECT_EQ(Inspection::from_entries(all, "x.zip").error().code(), "inspection.mask_not_png");

    entry(all, "mask.png")->data = *Vision::encode_mask_png({2, 2, {0, 255, 255, 0}});
    auto const small = Inspection::from_entries(all, "x.zip");
    EXPECT_EQ(small.error().code(), "inspection.mask_size");
    EXPECT_EQ(small.error().text(),
              "mask.png: 2 x 2 pixels, but the image in result.json is 20 x 10.");

    auto grey = square_with_hole().mask;
    grey.data[0] = 128;
    entry(all, "mask.png")->data = *Vision::encode_mask_png(grey);
    EXPECT_EQ(Inspection::from_entries(all, "x.zip").error().code(), "inspection.mask_values");
}

// a version-1 camera kept the scale of the width it was saved with: the photo's width as shown
// rescales it
TEST(Inspection, LegacyCameraKeepsItsScale) {
    auto all = entries();
    entry(all, "camera.json")->data =
        camera_v1(1500, 60, 40);  // saved for a photo 40 px wide; this one is 20
    auto const read = Inspection::from_entries(all, "x.zip");
    ASSERT_FALSE(read.isError()) << read.error();
    Vision::CameraConfig const old {1500, 60, 40};
    double const old_scale = Vision::mm_per_px(old, 40)();
    EXPECT_NEAR(Vision::lens_scale(read().camera.lens).x, old_scale * 2,
                1e-9);  // half the pixels for the same width
}

TEST(Inspection, LegacyNeedsAReadablePhoto) {
    auto all = entries();
    entry(all, "camera.json")->data = camera_v1(1500, 60, 40);
    entry(all, "original.png")->data = "not an image";
    EXPECT_EQ(Inspection::from_entries(all, "x.zip").error().code(), "inspection.photo_unreadable");
}

TEST(Inspection, OlderHideInspectionGivesPhotoAndCamera) {
    auto all = entries();
    entry(all, "parameters.json")->data =
        "{\"format\": \"contorno-hide-parameters\", \"version\": 1}";
    entry(all, "result.json")->data = "{\"format\": \"contorno-hide-result\"}";  // not read
    auto const read = Inspection::from_entries(all, "x.zip");
    ASSERT_FALSE(read.isError()) << read.error();
    EXPECT_TRUE(read().legacy);
    EXPECT_FALSE(read().result);
    EXPECT_EQ(read().camera.lens.pixel_height_um, 3.2);
}

}  // namespace test_Inspection
