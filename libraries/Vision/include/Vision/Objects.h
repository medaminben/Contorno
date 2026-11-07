#ifndef VISION_OBJECTS_H
#define VISION_OBJECTS_H
#include <Vision/Vision_export.h>
#include <Vision/Camera.h>
#include <Vision/Lens.h>
#include <Core/Core.h>
#include <Pot/Pot.h>
#include <span>
#include <string>
#include <vector>

namespace Vision {

/// Parameters of find_objects.
struct ObjectOptions {
    std::string model_path {};          ///< TorchScript IS-Net (.pt); required
    double outline_tolerance_mm {1.0};  ///< simplification of the outline, in mm
    double threshold {0.5};             ///< mask probability above which a pixel is an object
    double min_area_pct {0.1};          ///< regions (and holes) under this % of the image are dropped
    /// A hole is kept only where the model sees background: its mean
    /// probability must be under this, else (an unsure part of the object,
    /// such as marks drawn on it) it is filled.
    double hole_background {0.2};
    /// Also find the object from the background, and take it when the model
    /// has missed part of the hide (see find_objects); false: the model's
    /// region alone.
    bool check_background {true};
};

/// The objects found on a photo.
struct ObjectsAnalysis {
    PixelFrame frame {};
    std::vector<Pot::Polygon64> objects {};  ///< the largest 8-connected region, or none;
                                             ///< outer ring CCW, holes CW (e.g. handle loops)
    double area_mm2 {0};                     ///< pixels of the mask x s²
    Mask mask {};                            ///< 0/255, the object
    /// True when the object found from the background replaced the model's
    /// region (see find_objects): worth a look at the outline.
    bool from_background {false};
};

/**
 * @brief Find the outline of the largest object on a photo with the IS-Net model
 *
 * The model (options.model_path, run through LibTorch on the CPU) gives each
 * pixel's probability of belonging to an object; pixels above
 * options.threshold form the mask. The mask is cleaned (clean_mask_impl) and
 * split into 8-connected regions; regions under options.min_area_pct of the
 * image area are dropped, and of the others only the largest is kept (the
 * rest, dust and specks, would make the measured area ambiguous). The
 * object is also found from the background, grown from the image border
 * (see background_object_impl): when that object holds at least 95 % of the
 * model's region and is more than 10 % larger, the model has taken part of
 * the hide (the dark patches of a spotted hide) for the object, and the
 * object found from the background is taken instead. It is
 * traced into a polygon in the image mm frame (see to_mm) at `scale`
 * (mm per pixel of the photo as loaded, see lens_scale), simplified within
 * options.outline_tolerance_mm, with holes under the same minimum area
 * filled. Touching objects form one region. Nothing on the photo is a
 * success with no objects, an area of 0 and an all-zero mask.
 *
 * @return error (prefixed "objects: ") when Vision was built without
 *         LibTorch, no model path is given, the model or the image cannot be
 *         read, or the scale is not positive and finite
 */
Result<ObjectsAnalysis> VISION_API find_objects(std::string const& image_path,
                                                PixelScale scale,
                                                ObjectOptions const& options) noexcept;

/// ObjectOptions::min_area_pct for a minimum area in mm2: a percentage of the
/// image (width · height · x · y), at most 100 (a minimum larger than the photo
/// finds no object); 0 when any input is not finite or <= 0.
double VISION_API min_area_pct(double min_area_mm2, int width_px, int height_px, PixelScale scale) noexcept;

/// The minimum areas offered, in mm2: {0, 10, 50, 100, 500, 1000, 5000, 10000, 50000}.
std::span<double const> VISION_API min_area_presets() noexcept;

/// The preset nearest to `min_area_mm2` (the first of two equally near), so a
/// stored minimum is always one of min_area_presets().
double VISION_API snap_min_area(double min_area_mm2) noexcept;

/// True when Vision was built with LibTorch (find_objects and
/// ObjectOptions::model_path are usable).
bool VISION_API ai_available() noexcept;

}  // namespace Vision
#endif  // VISION_OBJECTS_H
