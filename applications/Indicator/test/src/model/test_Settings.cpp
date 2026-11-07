#include "model/IndicatorSettings.h"

#include <gtest/gtest.h>

namespace {

Vision::LensCamera lens_of(Vision::CameraSettings const& c) {
    return {c.distance_mm, c.focal_length_mm, c.pixel_width_um, c.pixel_height_um};
}

}  // namespace

TEST(WxSettings, RoundTrip) {
    MemorySettingsStore store;
    IndicatorSettings s;
    s.camera = {2000, 16, Vision::SensorInput::sensor, 2.4, 2.4, 23.5, 15.6};
    s.min_area_mm2 = 500;
    save_indicator_settings(store, s);
    auto const l = load_indicator_settings(store);
    EXPECT_EQ(l.camera.distance_mm, 2000);
    EXPECT_EQ(l.camera.focal_length_mm, 16);
    EXPECT_EQ(l.camera.pixel_width_um, 2.4);
    EXPECT_EQ(l.camera.pixel_height_um, 2.4);
    EXPECT_EQ(l.camera.input, Vision::SensorInput::sensor);
    EXPECT_EQ(l.camera.sensor_width_mm, 23.5);
    EXPECT_EQ(l.camera.sensor_height_mm, 15.6);
    EXPECT_EQ(l.min_area_mm2, 500);
}
TEST(WxSettings, LanguageRoundTrips) {
    MemorySettingsStore store;
    IndicatorSettings s;
    s.language = "ar";
    save_indicator_settings(store, s);
    EXPECT_EQ(load_indicator_settings(store).language, "ar");
}
TEST(WxSettings, ThemeRoundTrips) {
    MemorySettingsStore store;
    EXPECT_FALSE(load_indicator_settings(store).light);
    IndicatorSettings s;
    s.light = true;
    save_indicator_settings(store, s);
    EXPECT_TRUE(load_indicator_settings(store).light);
    s.light = false;
    save_indicator_settings(store, s);
    EXPECT_FALSE(load_indicator_settings(store).light);
}
TEST(WxSettings, UnknownLanguageIsEnglish) {
    MemorySettingsStore store;
    EXPECT_EQ(load_indicator_settings(store).language, "en");
    store.set_value("ui/language", "xx");
    EXPECT_EQ(load_indicator_settings(store).language, "en");
}
TEST(WxSettings, PixelPairRoundTrip) {
    MemorySettingsStore store;
    IndicatorSettings s;
    s.camera.input = Vision::SensorInput::pixel;
    s.camera.pixel_width_um = 3.76;
    s.camera.pixel_height_um = 4.20;
    save_indicator_settings(store, s);
    auto const l = load_indicator_settings(store);
    EXPECT_EQ(l.camera.pixel_width_um, 3.76);
    EXPECT_EQ(l.camera.pixel_height_um, 4.20);
}
TEST(WxSettings, OldPixelSizeFillsBoth) {
    MemorySettingsStore store;
    store.set_value("camera/pixel_size_um", "2.4");
    auto const l = load_indicator_settings(store);
    EXPECT_EQ(l.camera.pixel_width_um, 2.4);
    EXPECT_EQ(l.camera.pixel_height_um, 2.4);
}
// sensor size works for downscaled photos too; pixel size only at full resolution
TEST(WxSettings, DefaultIsSensorMode) {
    MemorySettingsStore store;
    auto const l = load_indicator_settings(store);
    EXPECT_EQ(l.camera.input, Vision::SensorInput::sensor);
    EXPECT_EQ(l.camera.sensor_width_mm, 15.04);
    EXPECT_EQ(l.camera.sensor_height_mm, 11.28);
}
// a store from before the sensor size: its pixel size x sensor width, 4:3, keeps the old scale
TEST(WxSettings, OldPixelIniBecomesSensorMode) {
    MemorySettingsStore store;
    store.set_value("camera/pixel_size_um", "2.4");
    store.set_value("camera/sensor_width_px", "6000");
    auto const l = load_indicator_settings(store);
    EXPECT_EQ(l.camera.input, Vision::SensorInput::sensor);
    EXPECT_EQ(l.camera.pixel_width_um, 2.4);
    EXPECT_NEAR(l.camera.sensor_width_mm, 14.4, 1e-12);
    EXPECT_NEAR(l.camera.sensor_height_mm, 10.8, 1e-12);
}
TEST(WxSettings, ChosenPixelModeIsKept) {
    MemorySettingsStore store;
    store.set_value("camera/sensor_input", "pixel");
    EXPECT_EQ(load_indicator_settings(store).camera.input, Vision::SensorInput::pixel);
}
TEST(WxSettings, UnknownSensorInputIsTheDefault) {
    MemorySettingsStore store;
    store.set_value("camera/sensor_input", "foo");
    EXPECT_EQ(load_indicator_settings(store).camera.input, Vision::SensorInput::sensor);
}
TEST(WxSettings, InvalidSensorSizeFallsBack) {
    MemorySettingsStore store;
    store.set_value("camera/sensor_width_mm", "0");
    store.set_value("camera/sensor_height_mm", "500");
    auto const l = load_indicator_settings(store);
    Vision::CameraSettings const d {};
    EXPECT_EQ(l.camera.sensor_width_mm, d.sensor_width_mm);
    EXPECT_EQ(l.camera.sensor_height_mm, d.sensor_height_mm);
}
// The samples-or-home start folder and the folder-exists check belong to the
// UI layer; the model keeps what is stored.
TEST(WxSettings, LastFolder) {
    MemorySettingsStore store;
    EXPECT_EQ(load_indicator_settings(store).last_folder, "");
    IndicatorSettings s;
    s.last_folder = "/some/folder";
    save_indicator_settings(store, s);
    EXPECT_EQ(load_indicator_settings(store).last_folder, "/some/folder");
}
TEST(WxSettings, LegacyFovMigrates) {
    MemorySettingsStore store;
    store.set_value("camera/distance_mm", "1500");
    store.set_value("camera/fov_deg", "60");
    store.set_value("camera/image_width_px", "4000");
    auto const l = load_indicator_settings(store);
    auto expected = Vision::mm_per_px({1500, 60, 4000}, 4000);
    ASSERT_FALSE(expected.isError());
    EXPECT_NEAR(Vision::lens_scale(lens_of(l.camera)).x, expected(), 1e-9);
}
TEST(WxSettings, MinAreaSnaps) {
    MemorySettingsStore store;
    store.set_value("objects/min_area_mm2", "70");
    EXPECT_EQ(load_indicator_settings(store).min_area_mm2, 50);
}
TEST(WxSettings, NonNumericMinAreaFallsBack) {
    MemorySettingsStore store;
    store.set_value("objects/min_area_mm2", "abc");
    EXPECT_EQ(load_indicator_settings(store).min_area_mm2, 100);
}
TEST(WxSettings, ModelPathIsRead) {
    MemorySettingsStore store;
    store.set_value("ai/model_path", "/some/model.pt");
    EXPECT_EQ(load_indicator_settings(store).model_path, "/some/model.pt");
    MemorySettingsStore fresh;
    save_indicator_settings(fresh, IndicatorSettings {});
    EXPECT_FALSE(fresh.value("ai/model_path").has_value());
}
TEST(WxSettings, InvalidCameraFallsBack) {
    MemorySettingsStore store;
    store.set_value("camera/distance_mm", "abc");
    store.set_value("camera/focal_length_mm", "0");
    store.set_value("camera/pixel_size_um", "-2");
    auto const l = load_indicator_settings(store);
    Vision::CameraSettings const d {};
    EXPECT_EQ(l.camera.distance_mm, d.distance_mm);
    EXPECT_EQ(l.camera.focal_length_mm, d.focal_length_mm);
    EXPECT_EQ(l.camera.pixel_width_um, d.pixel_width_um);
    EXPECT_EQ(l.camera.pixel_height_um, d.pixel_height_um);
    MemorySettingsStore legacy;
    legacy.set_value("camera/distance_mm", "1500");
    legacy.set_value("camera/fov_deg", "200");
    legacy.set_value("camera/image_width_px", "4000");
    EXPECT_NEAR(Vision::lens_scale(lens_of(load_indicator_settings(legacy).camera)).x,
                Vision::lens_scale(lens_of(d)).x, 1e-12);
}
TEST(WxSettings, GarbageValuesGiveDefaults) {
    MemorySettingsStore store;
    for (char const* key :
         {"camera/distance_mm", "camera/focal_length_mm", "camera/fov_deg", "camera/image_width_px",
          "camera/pixel_size_um", "camera/pixel_width_um", "camera/pixel_height_um",
          "camera/sensor_input", "camera/sensor_width_mm", "camera/sensor_height_mm",
          "camera/sensor_width_px", "objects/min_area_mm2", "ui/language", "ui/theme"}) {
        store.set_value(key, "x");
    }
    // free text, kept as stored: no value can be garbage
    store.set_value("ai/model_path", "x");
    store.set_value("dialogs/last_folder", "x");
    auto const l = load_indicator_settings(store);
    IndicatorSettings const d {};
    EXPECT_EQ(l.camera.distance_mm, d.camera.distance_mm);
    EXPECT_EQ(l.camera.focal_length_mm, d.camera.focal_length_mm);
    EXPECT_EQ(l.camera.input, d.camera.input);
    EXPECT_EQ(l.camera.pixel_width_um, d.camera.pixel_width_um);
    EXPECT_EQ(l.camera.pixel_height_um, d.camera.pixel_height_um);
    EXPECT_EQ(l.camera.sensor_width_mm, d.camera.sensor_width_mm);
    EXPECT_EQ(l.camera.sensor_height_mm, d.camera.sensor_height_mm);
    EXPECT_EQ(l.min_area_mm2, d.min_area_mm2);
    EXPECT_EQ(l.language, d.language);
    EXPECT_EQ(l.light, d.light);
    EXPECT_EQ(l.model_path, "x");
    EXPECT_EQ(l.last_folder, "x");
}
