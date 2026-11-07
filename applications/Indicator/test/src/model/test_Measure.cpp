#include <gtest/gtest.h>

#include <algorithm>

#include "model/Measure.h"

namespace test_Measure {

// the lens model is tested in Vision (test_Lens.cpp); here the readout text
TEST(WxMeasure, ScaleText) {
    EXPECT_EQ(scale_text({0.5776, 0.5776}), "0.578");
    EXPECT_EQ(scale_text({0.578, 0.646}), "0.578 × 0.646");
}
TEST(WxMeasure, AreaFormats) {
    EXPECT_EQ(format_area(1.842e6).digits, "1.842");
    EXPECT_EQ(format_area(1.842e6).unit, "m²");
    EXPECT_EQ(format_area(12.3456e6).digits, "12.346");
    EXPECT_EQ(format_area(1234.56e6).digits, "1234.6");
    EXPECT_EQ(format_area(12540).digits, "125.4");
    EXPECT_EQ(format_area(12540).unit, "cm²");
    EXPECT_EQ(format_area(100000).unit, "m²");
}
TEST(WxMeasure, ZeroAreaIsCm2) {
    EXPECT_EQ(format_area(0).digits, "0.0");
    EXPECT_EQ(format_area(0).unit, "cm²");
}
TEST(WxMeasure, HugeAreaFitsFiveCells) {
    for (double a : {1e11, 1e15}) {
        std::string digits = format_area(a).digits;
        digits.erase(std::remove(digits.begin(), digits.end(), '.'), digits.end());
        EXPECT_LE(digits.size(), 5u);
    }
}

}  // namespace test_Measure
