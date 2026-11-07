#include "model/Utf8Path.h"

#include <gtest/gtest.h>

TEST(WxUtf8Path, RoundTripsNonAsciiText) {
    std::string const text = "K\xC3\xA4lber_\xD8\xAC\xD9\x84\xD8\xAF/\xC3\xBC.jpg";  // Kälber_جلد/ü.jpg
    EXPECT_EQ(utf8(path_from_utf8(text)), text);
    EXPECT_EQ(path_from_utf8(text).filename().u8string().size(), 6u);  // ü.jpg
}
