#ifndef INDICATOR_MEASURE_H
#define INDICATOR_MEASURE_H
#include <Vision/Lens.h>

#include <string>

/// "0.578" when x == y (3 decimals), else "0.578 × 0.646"; callers append " mm/px".
std::string scale_text(Vision::PixelScale scale, int decimals = 3);

struct AreaText {
    std::string digits;
    std::string unit;
};

/// Readout text: m² (most decimals, <= 3, keeping <= 5 digits; capped 99999)
/// from 100 000 mm², cm² with one decimal below.
AreaText format_area(double area_mm2);

#endif  // INDICATOR_MEASURE_H
