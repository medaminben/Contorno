#include "Pot_impl.h"

namespace Pot {

PointD to_pointD_impl(Point64 const& p) noexcept {
    return {static_cast<double>(p.x) / units_per_mm, static_cast<double>(p.y) / units_per_mm};
}

}  // namespace Pot
