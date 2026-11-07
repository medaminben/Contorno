#ifndef VISION_OBJECTS_IMPL_H
#define VISION_OBJECTS_IMPL_H
#include <Vision/Objects.h>

#include <utility>

namespace Vision {

/// find_objects without the exception guard; errors are prefixed "objects: ".
/// "objects: <reason>".
inline Core::Problem objects_problem(Core::Problem reason) {
    return Core::wrapped("objects.failed", "objects: {reason}", std::move(reason));
}

Result<ObjectsAnalysis> find_objects_impl(std::string const& image_path, PixelScale scale,
                                          ObjectOptions const& options);

}  // namespace Vision

#endif  // VISION_OBJECTS_IMPL_H
