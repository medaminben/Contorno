#ifndef VISION_TEST_GEOMETRY_H
#define VISION_TEST_GEOMETRY_H
// Plane geometry on grid polygons, for the Vision tests only.
#include <Pot/Pot.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace geom {

/// Signed area of a ring in grid units²: positive when counter-clockwise.
inline double signed_area(Pot::Path64 const& ring) {
    double twice = 0;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        auto const& a = ring[i];
        auto const& b = ring[(i + 1) % ring.size()];
        twice += static_cast<double>(a.x) * static_cast<double>(b.y) -
                 static_cast<double>(b.x) * static_cast<double>(a.y);
    }
    return twice / 2;
}

/// Area of a polygon in grid units²: the outer ring less the holes.
inline double area(Pot::Polygon64 const& polygon) {
    double result = std::abs(signed_area(polygon.outer));
    for (auto const& hole : polygon.holes)
        result -= std::abs(signed_area(hole));
    return result;
}

/// True when p lies strictly inside the ring (crossing number; a point on the
/// boundary may go either way, the tests use points well inside or outside).
inline bool inside(Pot::Point64 const& p, Pot::Path64 const& ring) {
    bool result = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        auto const& a = ring[i];
        auto const& b = ring[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            double const x = static_cast<double>(b.x - a.x) * static_cast<double>(p.y - a.y) /
                                 static_cast<double>(b.y - a.y) +
                             static_cast<double>(a.x);
            if (static_cast<double>(p.x) < x)
                result = !result;
        }
    }
    return result;
}

/// The Pot polygon conventions: the outer ring
/// counter-clockwise, the holes clockwise and inside it.
inline void expect_oriented(Pot::Polygon64 const& polygon) {
    ASSERT_GE(polygon.outer.size(), 3u);
    EXPECT_GT(signed_area(polygon.outer), 0.0) << "outer ring is counter-clockwise";
    for (auto const& hole : polygon.holes) {
        ASSERT_GE(hole.size(), 3u);
        EXPECT_LT(signed_area(hole), 0.0) << "holes are clockwise";
        for (auto const& p : hole) {
            EXPECT_TRUE(inside(p, polygon.outer)) << "hole vertex " << p.x << ", " << p.y;
        }
    }
}

}  // namespace geom

#endif  // VISION_TEST_GEOMETRY_H
