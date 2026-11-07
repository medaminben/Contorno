#include <gtest/gtest.h>

#include <memory>

#include "TempDir.h"

namespace fs = std::filesystem;

TEST(WxTempDir, EveryInstanceGetsItsOwnExistingRoot) {
    fs::path first_path, second_path;
    {
        TempDir first, second;
        first_path = first.path;
        second_path = second.path;
        EXPECT_NE(first_path, second_path);
        EXPECT_TRUE(fs::is_directory(first_path));
        EXPECT_TRUE(fs::is_directory(second_path));
        // one's removal leaves the other alone
    }
    EXPECT_FALSE(fs::exists(first_path));
    EXPECT_FALSE(fs::exists(second_path));
}

TEST(WxTempDir, RemovingOneLeavesTheOther) {
    auto first = std::make_unique<TempDir>();
    TempDir second;
    first.reset();
    EXPECT_TRUE(fs::is_directory(second.path));
}
