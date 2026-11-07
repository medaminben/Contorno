#ifndef POT_H
#define POT_H
#include <Pot/Pot_export.h>
#include <Core/Core.h>
#include <compare>
#include <cstdint>
#include <vector>

namespace Pot {

/**
 * @brief Integer grid of all geometry
 *
 * Public coordinates are double millimetres; internally they are scaled to
 * integer units (1 unit = 1 micrometre).
 */
inline constexpr double units_per_mm = 1000.0;

/**
 * @brief 2D point on the integer grid
 */
struct Point64 {
    int64_t x {0};
    int64_t y {0};

    constexpr auto operator<=>(Point64 const&) const = default;

    constexpr Point64& operator+=(Point64 const& o) noexcept {
        x += o.x;
        y += o.y;
        return *this;
    }
    constexpr Point64& operator-=(Point64 const& o) noexcept {
        x -= o.x;
        y -= o.y;
        return *this;
    }
    friend constexpr Point64 operator+(Point64 a, Point64 const& b) noexcept { return a += b; }
    friend constexpr Point64 operator-(Point64 a, Point64 const& b) noexcept { return a -= b; }
    friend constexpr Point64 operator-(Point64 const& a) noexcept { return {-a.x, -a.y}; }
};

/**
 * @brief 2D point in double millimetres (public API)
 */
struct PointD {
    double x {0};
    double y {0};

    constexpr bool operator==(PointD const&) const = default;
};

/// Closed ring: the last vertex connects back to the first, which is not repeated.
using Path64 = std::vector<Point64>;
using Paths64 = std::vector<Path64>;

/**
 * @brief Polygon with holes on the integer grid
 *
 * By convention the outer ring is counter-clockwise and holes are clockwise
 * (y axis pointing up).
 */
struct Polygon64 {
    Path64 outer {};
    Paths64 holes {};
};

/**
 * @brief A ring whose edges are straight segments or circular arcs
 *
 * Edge i joins vertices[i] and vertices[i + 1] (the last edge closes the
 * ring). bulges[i] describes it as in DXF: 0 for a straight edge, otherwise
 * the tangent of a quarter of the arc's central angle, positive when the arc
 * turns counter-clockwise from vertices[i] to vertices[i + 1]. A bulge of 1
 * is a half circle. bulges has one entry per vertex (missing entries are
 * straight).
 */
struct ArcPath64 {
    Path64 vertices {};
    std::vector<double> bulges {};

    bool operator==(ArcPath64 const&) const = default;
};

/// A grid point in millimetres.
PointD POT_API to_pointD(Point64 const& p) noexcept;

}  // namespace Pot

#endif  // POT_H
