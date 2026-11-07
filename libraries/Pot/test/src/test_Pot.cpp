#include <Pot/Pot.h>
// Include 3rd party headers
#include <gtest/gtest.h>

namespace test_Pot {
using namespace Pot;

TEST(test_Pot_point64, arithmetic_and_ordering) {
    Point64 const a {3, 4};
    Point64 const b {1, -2};
    EXPECT_EQ(a + b, (Point64 {4, 2}));
    EXPECT_EQ(a - b, (Point64 {2, 6}));
    EXPECT_EQ(-a, (Point64 {-3, -4}));
    EXPECT_LT(b, a);
    EXPECT_LT((Point64 {3, 1}), a);  // x first, then y
}

TEST(test_Pot_scaling, grid_to_millimetres) {
    EXPECT_EQ(to_pointD({12346, -500}), (PointD {12.346, -0.5}));
}
}  // namespace test_Pot
