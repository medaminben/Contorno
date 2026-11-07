#include <Vision/Photo.h>

#include "Exif.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace test_Photo {

namespace fs = std::filesystem;
using test_Exif::ExifTag;
using Vision::CameraNoteKind;
using Vision::CameraSettings;
using Vision::CameraSource;
using Vision::PhotoMetadata;

namespace {
PhotoMetadata parse(std::vector<ExifTag> const& tags, bool big = false) {
    return Vision::parse_photo_metadata(test_Exif::exif_tiff(tags, big));
}
std::optional<double> px_per_mm(uint32_t res, std::optional<uint32_t> unit) {
    std::vector<ExifTag> tags {{0xA20E, 5, res, 1}};
    if (unit) tags.push_back({0xA210, 3, *unit});
    return parse(tags).focal_plane_px_per_mm;
}
/// Writes `bytes` to a file in the temporary directory and returns its path.
std::string temp_file(char const* name, std::string const& bytes) {
    auto const path = (fs::temp_directory_path() / name).string();
    std::ofstream(path, std::ios::binary) << bytes;
    return path;
}
CameraSettings sensor_mode() {
    CameraSettings s {};
    s.input = Vision::SensorInput::sensor;
    return s;
}
}  // namespace

TEST(PhotoMetadata, ReadsYResolution) {
    auto const m = parse({{0xA20E, 5, 1000, 1}, {0xA20F, 5, 500, 1}, {0xA210, 3, 4}});
    ASSERT_TRUE(m.focal_plane_px_per_mm);
    ASSERT_TRUE(m.focal_plane_y_px_per_mm);
    EXPECT_NEAR(*m.focal_plane_px_per_mm, 1000, 1e-9);
    EXPECT_NEAR(*m.focal_plane_y_px_per_mm, 500, 1e-9);
}
TEST(PhotoMetadata, ReadsOrientation) {
    auto const m = Vision::parse_photo_metadata(test_Exif::orientation_only_jpeg(6));
    ASSERT_TRUE(m.orientation);
    EXPECT_EQ(*m.orientation, 6);
    EXPECT_FALSE(Vision::parse_photo_metadata(test_Exif::exif_jpeg({{0x920A, 5, 42, 10}})).orientation);
    EXPECT_FALSE(Vision::parse_photo_metadata(test_Exif::orientation_only_jpeg(9)).orientation);
    EXPECT_FALSE(Vision::parse_photo_metadata(test_Exif::orientation_only_jpeg(0)).orientation);
}
TEST(PhotoMetadata, ReadsFocalLength) {
    auto const m = parse({{0x920A, 5, 42, 10}});
    ASSERT_TRUE(m.focal_length_mm);
    EXPECT_NEAR(*m.focal_length_mm, 4.2, 1e-12);
}
// some writers type the Exif IFD pointer IFD (13) instead of LONG (4)
TEST(PhotoMetadata, ExifPointerTypeIfd) {
    std::string tiff = test_Exif::exif_tiff({{0x920A, 5, 42, 10}});
    ASSERT_EQ(static_cast<unsigned char>(tiff[12]), 4);  // IFD0's only entry: tag at 10, type at 12 (little endian)
    tiff[12] = 13;
    auto const m = Vision::parse_photo_metadata(tiff);
    ASSERT_TRUE(m.focal_length_mm);
    EXPECT_NEAR(*m.focal_length_mm, 4.2, 1e-12);
}
TEST(PhotoMetadata, FocalPlaneUnits) {
    EXPECT_NEAR(*px_per_mm(2540, 2), 100.0, 1e-9);
    EXPECT_NEAR(*px_per_mm(1000, 3), 100.0, 1e-9);
    EXPECT_NEAR(*px_per_mm(100, 4), 100.0, 1e-9);
    EXPECT_NEAR(*px_per_mm(1, 5), 1000.0, 1e-9);
    EXPECT_NEAR(*px_per_mm(2540, std::nullopt), 100.0, 1e-9);
    EXPECT_FALSE(px_per_mm(2540, 7));
}
TEST(PhotoMetadata, ReadsThirtyFiveAndDimensions) {
    auto const m = parse({{0xA405, 3, 26}, {0xA002, 4, 4000}, {0xA003, 3, 3000}});
    EXPECT_EQ(m.focal_length_35mm, 26);
    EXPECT_EQ(m.pixel_x_dimension, 4000);
    EXPECT_EQ(m.pixel_y_dimension, 3000);
}
TEST(PhotoMetadata, BigEndian) {
    auto const m = parse({{0x920A, 5, 42, 10}}, true);
    ASSERT_TRUE(m.focal_length_mm);
    EXPECT_NEAR(*m.focal_length_mm, 4.2, 1e-12);
}
TEST(PhotoMetadata, JpegFile) {
    auto const path = temp_file("contorno_photo_exif.jpg", test_Exif::exif_jpeg({{0x920A, 5, 42, 10}}));
    auto const m = Vision::read_photo_metadata(path);
    fs::remove(path);
    ASSERT_TRUE(m.focal_length_mm);
    EXPECT_NEAR(*m.focal_length_mm, 4.2, 1e-12);
}
TEST(PhotoMetadata, NoCameraTagsIsEmpty) {
    auto const path = temp_file("contorno_photo_orientation.jpg", test_Exif::orientation_only_jpeg());
    for (auto const& m : {Vision::read_photo_metadata(path), Vision::parse_photo_metadata(std::string_view("\x89PNG\r\n", 6))}) {
        EXPECT_FALSE(m.focal_length_mm);
        EXPECT_FALSE(m.focal_plane_px_per_mm);
        EXPECT_FALSE(m.focal_length_35mm);
        EXPECT_FALSE(m.pixel_x_dimension);
        EXPECT_FALSE(m.pixel_y_dimension);
    }
    fs::remove(path);
}
TEST(PhotoMetadata, UnreadableFileIsEmpty) {
    EXPECT_FALSE(Vision::read_photo_metadata("/nonexistent/contorno.jpg").focal_length_mm);
}
TEST(PhotoMetadata, TruncatedExifIsEmpty) {
    std::string const full = test_Exif::exif_tiff({{0x920A, 5, 42, 10}});
    for (std::size_t n = 0; n < full.size(); ++n)
        EXPECT_FALSE(Vision::parse_photo_metadata(std::string_view(full).substr(0, n)).focal_length_mm) << n;
    std::string const jpeg = test_Exif::exif_jpeg({{0x920A, 5, 42, 10}});
    for (std::size_t n = 0; n + 2 < jpeg.size(); ++n)  // without the EOI the APP1 is still whole
        EXPECT_FALSE(Vision::parse_photo_metadata(std::string_view(jpeg).substr(0, n)).focal_length_mm) << n;
}
TEST(PhotoMetadata, OffsetPastEndIsEmpty) {
    std::string tiff = test_Exif::exif_tiff({{0x920A, 5, 42, 10}});
    tiff.replace(18, 4, std::string("\xF0\xFF\xFF\xFF", 4));  // little-endian 0xFFFFFFF0
    auto const m = Vision::parse_photo_metadata(tiff);
    EXPECT_FALSE(m.focal_length_mm);
    EXPECT_FALSE(m.focal_length_35mm);
    EXPECT_FALSE(m.pixel_x_dimension);
}
TEST(PhotoMetadata, ZeroDenominatorIsAbsent) {
    EXPECT_FALSE(parse({{0x920A, 5, 42, 0}}).focal_length_mm);
}

// the default camera keeps its scale on a downscaled photo without metadata
TEST(PhotoCamera, DefaultSettingsScaleWithThePhoto) {
    Vision::LensCamera const full {1500, 13.02, 3.76, 3.76};
    auto const c = Vision::resolve_camera(CameraSettings {}, {}, 1500, 1125);
    EXPECT_NEAR(Vision::lens_scale(c.lens).x, Vision::lens_scale(full).x * 4000 / 1500, 1e-9);
}
TEST(PhotoCamera, NoMetadataUsesPixelSetting) {
    CameraSettings s {};
    s.input = Vision::SensorInput::pixel;
    auto const c = Vision::resolve_camera(s, {}, 4000, 3000);
    EXPECT_EQ(c.lens.distance_mm, 1500);
    EXPECT_EQ(c.lens.focal_length_mm, 13.02);
    EXPECT_EQ(c.lens.pixel_width_um, 3.76);
    EXPECT_EQ(c.lens.pixel_height_um, 3.76);
    EXPECT_EQ(c.focal_source, CameraSource::settings);
    EXPECT_EQ(c.pixel_source, CameraSource::settings);
    EXPECT_TRUE(c.notes.empty());
}
TEST(PhotoCamera, NoPhotoKeepsTheSettings) {
    auto const c = Vision::resolve_camera(sensor_mode(), {}, 0, 0);
    EXPECT_EQ(c.lens.pixel_width_um, 3.76);
    EXPECT_TRUE(c.notes.empty());
}
TEST(PhotoCamera, SensorSizeMatchesLongSide) {
    auto const s = sensor_mode();
    EXPECT_NEAR(Vision::resolve_camera(s, {}, 4000, 3000).lens.pixel_width_um, 3.76, 1e-9);
    EXPECT_NEAR(Vision::resolve_camera(s, {}, 3000, 4000).lens.pixel_width_um, 3.76, 1e-9);
    EXPECT_NEAR(Vision::resolve_camera(s, {}, 2000, 1500).lens.pixel_width_um, 7.52, 1e-9);
    EXPECT_NEAR(Vision::resolve_camera(s, {}, 2000, 1500).lens.pixel_height_um, 7.52, 1e-9);
}
TEST(PhotoCamera, CropNote) {
    auto const s = sensor_mode();
    auto const cropped = Vision::resolve_camera(s, {}, 4000, 2250);
    ASSERT_EQ(cropped.notes.size(), 1u);
    EXPECT_EQ(cropped.notes.front().kind, CameraNoteKind::possibly_cropped);
    EXPECT_TRUE(Vision::resolve_camera(s, {}, 4000, 3000).notes.empty());
}
TEST(PhotoCamera, FocalPlaneWins) {
    PhotoMetadata m;
    m.focal_length_mm = 4.2;
    m.focal_plane_px_per_mm = 1000 / 1.4;
    auto const c = Vision::resolve_camera(CameraSettings {}, m, 4000, 3000);
    EXPECT_NEAR(c.lens.focal_length_mm, 4.2, 1e-9);
    EXPECT_NEAR(c.lens.pixel_width_um, 1.4, 1e-9);
    EXPECT_NEAR(c.lens.pixel_height_um, 1.4, 1e-9);
    EXPECT_EQ(c.lens.distance_mm, 1500);
    EXPECT_EQ(c.focal_source, CameraSource::photo);
    EXPECT_EQ(c.pixel_source, CameraSource::photo);
    EXPECT_NEAR(Vision::lens_scale(c.lens).x, 0.5, 1e-9);
}
TEST(PhotoCamera, PixelDimensionCorrectionUsesLongSide) {
    PhotoMetadata m;
    m.focal_plane_px_per_mm = 1000 / 1.4;
    m.pixel_x_dimension = 4000;
    m.pixel_y_dimension = 3000;
    EXPECT_NEAR(Vision::resolve_camera(CameraSettings {}, m, 1500, 2000).lens.pixel_width_um, 2.8, 1e-9);
}
TEST(PhotoCamera, NonSquareFromMetadata) {
    PhotoMetadata m;
    m.focal_length_mm = 4.2;
    m.focal_plane_px_per_mm = 1000 / 1.4;
    m.focal_plane_y_px_per_mm = 1000 / 2.8;
    auto const c = Vision::resolve_camera(CameraSettings {}, m, 4000, 3000);
    EXPECT_NEAR(c.lens.pixel_width_um, 1.4, 1e-9);
    EXPECT_NEAR(c.lens.pixel_height_um, 2.8, 1e-9);
}
TEST(PhotoCamera, OnlyXResolutionIsSquare) {
    PhotoMetadata m;
    m.focal_plane_px_per_mm = 1000 / 1.4;
    auto const c = Vision::resolve_camera(CameraSettings {}, m, 4000, 3000);
    EXPECT_NEAR(c.lens.pixel_width_um, 1.4, 1e-9);
    EXPECT_NEAR(c.lens.pixel_height_um, 1.4, 1e-9);
}
TEST(PhotoCamera, InvalidYResolutionFallsBackWithNote) {
    PhotoMetadata m;
    m.focal_plane_px_per_mm = 1000 / 1.4;
    m.focal_plane_y_px_per_mm = 1e6;
    auto const c = Vision::resolve_camera(CameraSettings {}, m, 4000, 3000);
    EXPECT_EQ(c.pixel_source, CameraSource::settings);
    ASSERT_EQ(c.notes.size(), 1u);
    EXPECT_EQ(c.notes.front().kind, CameraNoteKind::focal_plane_invalid);
    EXPECT_NEAR(c.notes.front().value, 0.001, 1e-12);  // the invalid side, not the valid one
}
TEST(PhotoCamera, RotatedPhotoSwapsPixelSides) {
    CameraSettings s {};
    s.input = Vision::SensorInput::pixel;
    s.pixel_width_um = 3.76;
    s.pixel_height_um = 4.20;
    auto const c = Vision::resolve_camera(s, {}, 3000, 4000);
    double const k = 1500 * 1e-3 / 13.02;
    EXPECT_NEAR(Vision::lens_scale(c.lens, true).x, k * 4.20, 1e-12);
    EXPECT_NEAR(Vision::lens_scale(c.lens, true).y, k * 3.76, 1e-12);
}
TEST(PhotoCamera, ThirtyFiveEquivalent) {
    PhotoMetadata m;
    m.focal_length_mm = 4.2;
    m.focal_length_35mm = 26;
    auto const c = Vision::resolve_camera(CameraSettings {}, m, 4000, 3000);
    EXPECT_NEAR(c.lens.pixel_width_um, 1000 * (43.27 * 4.2 / 26 * 0.8) / 4000, 1e-9);
    EXPECT_EQ(c.lens.pixel_height_um, c.lens.pixel_width_um);
    EXPECT_EQ(c.pixel_source, CameraSource::photo_35mm);
}
TEST(PhotoCamera, ThirtyFiveNeedsPhotoFocal) {
    PhotoMetadata m;
    m.focal_length_35mm = 26;
    auto const c = Vision::resolve_camera(CameraSettings {}, m, 4000, 3000);
    EXPECT_EQ(c.lens.pixel_width_um, 3.76);
    EXPECT_EQ(c.pixel_source, CameraSource::settings);
}
TEST(PhotoCamera, InvalidPhotoValueFallsBackWithNote) {
    PhotoMetadata m;
    m.focal_length_mm = 0;
    m.focal_plane_px_per_mm = 1e6;
    CameraSettings const s {};
    auto const c = Vision::resolve_camera(s, m, 4000, 3000);
    EXPECT_EQ(c.lens.focal_length_mm, s.focal_length_mm);
    EXPECT_EQ(c.lens.pixel_width_um, s.pixel_width_um);
    EXPECT_EQ(c.lens.pixel_height_um, s.pixel_height_um);
    EXPECT_EQ(c.focal_source, CameraSource::settings);
    EXPECT_EQ(c.pixel_source, CameraSource::settings);
    ASSERT_EQ(c.notes.size(), 2u);
    EXPECT_EQ(c.notes[0], (Vision::CameraNote {CameraNoteKind::focal_length_invalid, 0}));
    EXPECT_EQ(c.notes[1].kind, CameraNoteKind::focal_plane_invalid);
    EXPECT_NEAR(c.notes[1].value, 0.001, 1e-12);
}
TEST(PhotoCamera, InvalidThirtyFiveNoteNamesItsSource) {
    PhotoMetadata m;
    m.focal_length_mm = 4.2;
    m.focal_length_35mm = 1000000;
    auto const c = Vision::resolve_camera(CameraSettings {}, m, 4000, 3000);
    EXPECT_EQ(c.pixel_source, CameraSource::settings);
    ASSERT_EQ(c.notes.size(), 1u);
    EXPECT_EQ(c.notes[0].kind, CameraNoteKind::equivalent_35mm_invalid);
    EXPECT_LT(c.notes[0].value, 0.1);
}

}  // namespace test_Photo
