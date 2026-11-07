#include <gtest/gtest.h>

#include "model/Language.h"

// gtest_main does not register the string tables. One environment here
// serves every test file in the binary.
namespace {
struct Strings : ::testing::Environment {
    void SetUp() override { register_indicator_strings(); }
};
::testing::Environment* const strings = ::testing::AddGlobalTestEnvironment(new Strings);
}  // namespace
