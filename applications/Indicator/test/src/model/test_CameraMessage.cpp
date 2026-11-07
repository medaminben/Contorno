#include <gtest/gtest.h>

#include "model/CameraMessage.h"
#include "model/Language.h"
#include "model/Measure.h"

#include <regex>
#include <string>

// the camera values themselves are tested in Vision (test_Photo.cpp); here the log line
namespace test_CameraMessage {

using Vision::CameraNote;
using Vision::CameraNoteKind;
using Vision::CameraSettings;
using Vision::PhotoMetadata;

namespace {
bool contains(std::string const& text, std::string const& part) { return text.find(part) != std::string::npos; }
bool has_prefix(std::string const& text, std::string const& prefix) { return text.compare(0, prefix.size(), prefix) == 0; }
bool has_suffix(std::string const& text, std::string const& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}
Vision::PhotoCamera resolve(CameraSettings const& s, PhotoMetadata const& m, int w = 4000, int h = 3000) {
    return Vision::resolve_camera(s, m, w, h);
}
CameraSettings pixel_mode(double width_um = 3.76, double height_um = 3.76) {
    CameraSettings s {};
    s.input = Vision::SensorInput::pixel;
    s.pixel_width_um = width_um;
    s.pixel_height_um = height_um;
    return s;
}
}  // namespace

TEST(WxCameraMessage, NoMetadata) {
    EXPECT_EQ(camera_message(resolve(pixel_mode(), {})).english(),
              "Camera: no camera data in the photo, settings used: 0.433 mm/px.");
}
TEST(WxCameraMessage, Notes) {
    EXPECT_EQ(note_message({CameraNoteKind::focal_length_invalid, 0}).english(),
              "the photo's focal length 0.00 mm is not valid");
    EXPECT_EQ(note_message({CameraNoteKind::focal_plane_invalid, 0.001}).english(),
              "the photo's focal-plane pixel size 0.00 µm is not valid");
    EXPECT_EQ(note_message({CameraNoteKind::equivalent_35mm_invalid, 0.004}).english(),
              "the photo's 35 mm equivalent pixel size 0.00 µm is not valid");
    EXPECT_TRUE(contains(note_message({CameraNoteKind::possibly_cropped}).english(), "cropped"));
}
TEST(WxCameraMessage, NotesInTheLine) {
    PhotoMetadata m;
    m.focal_length_mm = 0;
    m.focal_plane_px_per_mm = 1e6;
    auto const c = resolve(CameraSettings {}, m);
    ASSERT_EQ(c.notes.size(), 2u);
    auto const msg = camera_message(c).english();
    EXPECT_FALSE(has_prefix(msg, "Camera: no camera data"));
    EXPECT_TRUE(contains(msg, note_message(c.notes[0]).english())) << msg;
    EXPECT_TRUE(contains(msg, note_message(c.notes[1]).english())) << msg;
}
TEST(WxCameraMessage, FocalPlaneFromThePhoto) {
    PhotoMetadata m;
    m.focal_length_mm = 4.2;
    m.focal_plane_px_per_mm = 1000 / 1.4;
    auto const msg = camera_message(resolve(CameraSettings {}, m)).english();
    EXPECT_TRUE(contains(msg, "focal length 4.20 mm")) << msg;
    EXPECT_TRUE(contains(msg, "pixel size 1.40 µm")) << msg;
    EXPECT_TRUE(contains(msg, "from the photo")) << msg;
    EXPECT_TRUE(contains(msg, "1500 mm from the settings")) << msg;
    EXPECT_TRUE(has_suffix(msg, "0.500 mm/px.")) << msg;
}
TEST(WxCameraMessage, NonSquareFromMetadata) {
    PhotoMetadata m;
    m.focal_length_mm = 4.2;
    m.focal_plane_px_per_mm = 1000 / 1.4;
    m.focal_plane_y_px_per_mm = 1000 / 2.8;
    auto const msg = camera_message(resolve(CameraSettings {}, m)).english();
    EXPECT_TRUE(contains(msg, "pixel size 1.40 × 2.80 µm")) << msg;
}
TEST(WxCameraMessage, PixelModeNonSquare) {
    PhotoMetadata m;
    m.focal_length_mm = 13.02;  // some photo data, so the message names the pixel size
    auto const msg = camera_message(resolve(pixel_mode(3.76, 4.20), m)).english();
    EXPECT_TRUE(contains(msg, "3.76 × 4.20")) << msg;
}
TEST(WxCameraMessage, ScaleFollowsRotation) {
    PhotoMetadata m;
    m.focal_length_mm = 13.02;
    auto const c = resolve(pixel_mode(3.76, 4.20), m);
    auto const sc = Vision::lens_scale(c.lens);
    std::string const plain = scale_text(sc);
    std::string const swapped = scale_text({sc.y, sc.x});
    ASSERT_NE(plain, swapped);
    EXPECT_TRUE(contains(camera_message(c, true).english(), swapped + " mm/px")) << camera_message(c, true).english();
    EXPECT_TRUE(contains(camera_message(c).english(), plain + " mm/px")) << camera_message(c).english();
    EXPECT_TRUE(contains(camera_message(c, true).english(), "3.76 × 4.20"));
}
TEST(WxCameraMessage, ThirtyFiveEquivalent) {
    PhotoMetadata m;
    m.focal_length_mm = 4.2;
    m.focal_length_35mm = 26;
    auto const msg = camera_message(resolve(CameraSettings {}, m)).english();
    EXPECT_TRUE(contains(msg, "(35 mm equivalent)")) << msg;
}

// the camera line is a message: another language changes the words, never the numbers
TEST(WxCameraMessage, Translatable) {
    PhotoMetadata m;
    m.focal_length_mm = 4.2;
    m.focal_length_35mm = 24;
    auto const c = resolve(CameraSettings {}, m);
    ASSERT_EQ(c.pixel_source, Vision::CameraSource::photo_35mm);
    Message const message = camera_message(c);
    register_indicator_strings();
    std::string const english = message.english();
    std::string const spanish = message.render("es");
    EXPECT_NE(spanish, english);
    std::regex const unit(R"(\d+(\.\d+)?( × \d+(\.\d+)?)? (mm/px|µm|mm))");
    int count = 0;
    for (std::sregex_iterator it(english.begin(), english.end(), unit), end; it != end; ++it) {
        std::string const value = it->str();
        EXPECT_TRUE(contains(spanish, value)) << value << " in " << spanish;
        ++count;
    }
    EXPECT_GE(count, 3);
    // "35 mm" is a value, not words: the same in every language
    EXPECT_TRUE(contains(english, "(35 mm equivalent)"));
    for (std::string const code : {"de", "ar", "fr", "it", "nl"}) {
        EXPECT_TRUE(contains(message.render(code), "35 mm")) << code << ": " << message.render(code);
    }
}

}  // namespace test_CameraMessage
