#include <Vision/Image.h>

#include "Exif.h"

#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace test_Image {

namespace {
/// A w x h JPEG; with `orientation6` its EXIF says to turn it 90° clockwise to show it.
std::string jpeg(int w, int h, bool orientation6) {
    std::vector<std::uint8_t> bytes;
    cv::imencode(".jpg", cv::Mat(h, w, CV_8UC3, cv::Scalar(60, 90, 120)), bytes);
    std::string out(bytes.begin(), bytes.end());
    if (orientation6) {
        std::string const app1 = test_Exif::orientation_only_jpeg();  // SOI, APP1, EOI
        out.insert(2, app1.substr(2, app1.size() - 4));  // its APP1 right after our SOI
    }
    return out;
}
}  // namespace

TEST(Image, ShownSizeAppliesTheOrientation) {
    EXPECT_EQ(Vision::shown_image_size(jpeg(40, 20, false)), (Vision::ImageSize {40, 20}));
    EXPECT_EQ(Vision::shown_image_size(jpeg(40, 20, true)), (Vision::ImageSize {20, 40}));
}
TEST(Image, ShownSizeOfNoImage) {
    EXPECT_FALSE(Vision::shown_image_size(""));
    EXPECT_FALSE(Vision::shown_image_size("not an image"));
}
TEST(Image, MaskRoundTrip) {
    Vision::Mask const mask {3, 2, {0, 255, 0, 255, 255, 0}};
    auto const png = Vision::encode_mask_png(mask);
    ASSERT_TRUE(png);
    auto const back = Vision::decode_mask_png(*png);
    ASSERT_TRUE(back);
    EXPECT_EQ(back->width, 3);
    EXPECT_EQ(back->height, 2);
    EXPECT_EQ(back->data, mask.data);
}
TEST(Image, InconsistentMaskIsNotEncoded) {
    EXPECT_FALSE(Vision::encode_mask_png({}));
    EXPECT_FALSE(Vision::encode_mask_png({3, 2, {0, 255}}));
}
TEST(Image, OnlyPngIsAMask) {
    EXPECT_FALSE(Vision::decode_mask_png(jpeg(4, 4, false)));
    EXPECT_FALSE(Vision::decode_mask_png("\x89PNG\r\n\x1A\n but broken"));
}
TEST(Image, ColourPngBecomesGrey) {
    std::vector<std::uint8_t> bytes;
    cv::imencode(".png", cv::Mat(2, 2, CV_8UC3, cv::Scalar(255, 255, 255)), bytes);
    auto const mask = Vision::decode_mask_png(std::string(bytes.begin(), bytes.end()));
    ASSERT_TRUE(mask);
    EXPECT_EQ(mask->data, (std::vector<std::uint8_t> {255, 255, 255, 255}));
}

}  // namespace test_Image
