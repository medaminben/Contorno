#ifndef TOOLS_DXF_H
#define TOOLS_DXF_H
#include <Tools/Tools_export.h>
#include <Core/Core.h>
#include <Pot/Pot.h>
#include <string>
#include <vector>

namespace Tools {

/**
 * @brief Closed rings of a DXF drawing (ASCII, millimetres), with their bulges
 * Reads the ENTITIES section: LWPOLYLINE and POLYLINE (with bulges),
 * CIRCLE, and LINE and ARC entities joined end to end into closed chains
 * (ends within 0.01 mm); open chains are ignored, as are blocks (INSERT).
 * Rings come in file order and in drawn orientation; rings with fewer than
 * two vertices are dropped. Error for a drawing with no closed shape.
 */
Result<std::vector<Pot::ArcPath64>> TOOLS_API parse_dxf(std::string const& text) noexcept;

/**
 * @brief A sheet as a DXF drawing (R12 ASCII, millimetres)
 * Layers SHEET (the outer ring) and DEFECT (the holes), shifted so the
 * outline's bounding box starts at the origin.
 */
Result<std::string> TOOLS_API to_dxf(Pot::Polygon64 const& sheet) noexcept;

/**
 * @brief Several polygons as a DXF drawing (R12 ASCII, millimetres) on one layer
 * Every outer ring and hole with at least 3 points is a closed polyline on
 * `layer`, in the polygons' own frame (not shifted: e.g. the image mm frame).
 * Error when no ring has at least 3 points.
 */
Result<std::string> TOOLS_API to_dxf(std::vector<Pot::Polygon64> const& polygons,
                                     std::string const& layer) noexcept;

}  // namespace Tools
#endif  // TOOLS_DXF_H
