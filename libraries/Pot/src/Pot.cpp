#include "Pot_impl.h"
#include <Pot/Pot.h>

namespace Pot {

PointD to_pointD(Point64 const& p) noexcept {
    return to_pointD_impl(p);
}

}  // namespace Pot
