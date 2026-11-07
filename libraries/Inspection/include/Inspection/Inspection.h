#ifndef INSPECTION_H
#define INSPECTION_H
#include <Inspection/Inspection_export.h>
#include <Core/Core.h>
#include <Tools/Zip.h>
#include <Vision/Objects.h>
#include <Vision/Photo.h>

#include <optional>
#include <string>
#include <vector>

namespace Inspection {

/// What an inspection zip holds: the photo, the camera, the analysis
/// parameters and the saved result.
struct Record {
    std::string photo_name {};  ///< "original.<ext>", the extension lower case
    std::string photo {};       ///< the photo's bytes, untouched
    Vision::PhotoCamera camera {};
    double min_area_mm2 {100};
    double outline_tolerance_mm {0.5};  ///< what the analysis was run with (written, not read back)
    /// The saved result (result.json with the mask from mask.png); none for an
    /// older inspection.
    std::optional<Vision::ObjectsAnalysis> result {};
    /// An older hide inspection (contorno-hide-*, conqueror-*): its parameters
    /// and result are ignored, the camera comes from its camera.json.
    bool legacy {false};
};

/// "original" plus the extension of `photo_path` in lower case: "original.png"
/// for "a/photo.PNG", "original" without one.
std::string INSPECTION_API photo_name(std::string const& photo_path);

/**
 * @brief The entries of an inspection zip, in this order
 *
 * The photo (record.photo_name, its bytes untouched), mask.png (the mask, 8-bit
 * grey 0/255; the outlines are drawn from the result, never saved),
 * camera.json (contorno-camera v4: the lens with pixel width and height, where
 * its focal length and pixel size came from, and the field of view derived
 * for the analysed width), parameters.json (contorno-objects-parameters v1),
 * objects.dxf (every outer ring and hole, layer OBJECT; omitted without
 * objects) and result.json (contorno-objects-result v2, image mm frame with mm
 * per pixel along x and y). `rotated`: the photo is shown turned by 90°, so
 * the derived field of view uses the pixel's other side.
 *
 * @return error "inspection.no_mask" for an analysis without a mask, or
 *         "inspection.entry" naming the entry that cannot be written
 */
Result<std::vector<Tools::ZipEntry>> INSPECTION_API
to_entries(Record const& record, Vision::ObjectsAnalysis const& analysis, bool rotated) noexcept;

/**
 * @brief An inspection from the entries of its zip
 *
 * The photo is the entry named "original.<anything>"; camera.json versions 1
 * to 4 are read (1 to 3 as square pixels; 1 and 2 scaled from the width they
 * were saved with to the photo's width as shown); the minimum area is snapped
 * to the presets. An older hide inspection (`legacy`) gives the photo and the
 * camera only. A processed.png from an older export is ignored. `zip_name`
 * names the archive in the problems.
 *
 * @return error naming the entry that is missing or invalid (mask.png must
 *         have result.json's image size and hold only 0 and 255)
 */
Result<Record> INSPECTION_API from_entries(std::vector<Tools::ZipEntry> const& entries,
                                           std::string const& zip_name) noexcept;

}  // namespace Inspection

#endif  // INSPECTION_H
