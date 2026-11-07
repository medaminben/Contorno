#include <Vision/Vision.h>

// Include 3rd party headers
#include <gtest/gtest.h>

#include <cmath>

namespace test_Vision {

TEST(Camera, DefaultScale) {
    auto r = Vision::mm_per_px({}, 4000);
    ASSERT_FALSE(r.isError());
    EXPECT_NEAR(r(), 0.4330127, 1e-6);
}

TEST(Camera, ResizedImage) {
    auto r = Vision::mm_per_px({}, 2000);
    ASSERT_FALSE(r.isError());
    EXPECT_NEAR(r(), 0.8660254, 1e-6);
}

TEST(Camera, RejectsBadConfig) {
    Vision::CameraConfig c;
    c.distance_mm = 0;
    EXPECT_TRUE(Vision::mm_per_px(c, 4000).isError());
    c = {};
    c.fov_deg = 0;
    EXPECT_TRUE(Vision::mm_per_px(c, 4000).isError());
    c.fov_deg = 180;
    EXPECT_TRUE(Vision::mm_per_px(c, 4000).isError());
    c = {};
    EXPECT_TRUE(Vision::mm_per_px(c, 0).isError());
    c.image_width_px = 0;
    EXPECT_TRUE(Vision::mm_per_px(c, 4000).isError());
}

TEST(Camera, FrameRoundTrip) {
    Vision::PixelFrame f {200, 100, 0.5, 0.5};
    auto p = Vision::to_mm(f, 0, 0);
    EXPECT_DOUBLE_EQ(p.x, 0.25);
    EXPECT_DOUBLE_EQ(p.y, 49.75);
    auto q = Vision::to_px(f, Vision::to_mm(f, 37, 81));
    EXPECT_NEAR(q.x, 37, 1e-9);
    EXPECT_NEAR(q.y, 81, 1e-9);
}

TEST(Vision, ToMmUsesBothScales) {
    Vision::PixelFrame f {10, 20, 0.5, 2.0};
    auto p = Vision::to_mm(f, 3, 4);
    EXPECT_DOUBLE_EQ(p.x, 1.75);
    EXPECT_DOUBLE_EQ(p.y, 31.0);
    auto q = Vision::to_px(f, {1.75, 31.0});
    EXPECT_NEAR(q.x, 3, 1e-9);
    EXPECT_NEAR(q.y, 4, 1e-9);
}

TEST(Vision, LengthUsesSmallerScale) {
    Vision::PixelFrame f {1, 1, 0.5, 2.0};
    EXPECT_DOUBLE_EQ(Vision::px_for_mm(f, 1.0), 2.0);
}

TEST(Vision, AspectMustBePositive) {
    EXPECT_TRUE(Vision::mm_per_px({1500, 60, 4000, 0.0}, 4000).isError());
    EXPECT_TRUE(Vision::mm_per_px({1500, 60, 4000, std::nan("")}, 4000).isError());
    EXPECT_FALSE(Vision::mm_per_px({1500, 60, 4000, 2.0}, 4000).isError());
}

}  // namespace test_Vision
