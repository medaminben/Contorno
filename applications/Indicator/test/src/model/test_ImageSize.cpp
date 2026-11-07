#include <gtest/gtest.h>

#include "TempDir.h"
#include "TestImage.h"
#include "model/ImageSize.h"

#include <filesystem>

TEST(WxImageSize, RotatedJpegIsShownRotated) {
    TempDir dir;
    auto const rotated = dir.path / "rotated.jpg";
    ASSERT_TRUE(write_rotated_jpeg(rotated));
    EXPECT_TRUE(shown_rotated(rotated));
    auto const size = shown_size(rotated);
    ASSERT_TRUE(size);
    EXPECT_EQ(*size, (Vision::ImageSize {20, 40}));  // stored 40 x 20

    auto const plain = dir.path / "plain.jpg";
    ASSERT_TRUE(write_plain_jpeg(plain));
    EXPECT_FALSE(shown_rotated(plain));
    ASSERT_TRUE(shown_size(plain));
    EXPECT_EQ(*shown_size(plain), (Vision::ImageSize {40, 20}));
}
