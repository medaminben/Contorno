#ifndef VISION_SEGMENT_IMPL_H
#define VISION_SEGMENT_IMPL_H
// Private: separation of what lies on the table from the table, and the
// polygon of a mask. Includes OpenCV.
#include <Vision/Camera.h>

#include <opencv2/core.hpp>

#include <limits>
#include <utility>
#include <vector>

namespace Vision {

/// Tuning of background_object_impl; the defaults are the tuned values.
struct BackgroundOptions {
    int coarse_side_px {160};           ///< longest side of the coarse copy
    int coarse_median_px {5};           ///< median aperture on the coarse copy (removes lines of a pixel)
    double border_band_pct {4};         ///< width of the band the background is fitted to, % of the smaller side
    double max_deviation {6};           ///< deviation, in robust sigmas, up to which a pixel is background-like
    double max_shadow {1.15};           ///< darker by up to this factor with the background's colour: a shadow
    int fine_median_px {9};             ///< median aperture at full size before the edge is placed
    double fine_band_coarse_px {2.5};   ///< half-width of the band the edge is placed in, in coarse pixels
};

/**
 * @brief The object as what the background, found from the image border, does not reach
 *
 * Coarse: the image is shrunk (INTER_AREA) to coarse_side_px on its longest
 * side and median-filtered (coarse_median_px): thin lines on the table (the
 * grid of a cutting mat, the seams of planks) are gone. Per pixel, three
 * features: log lightness (CIELAB L), and the green and red shares of the
 * linear RGB sum (a shadow, which scales all three, leaves them). The
 * background is modelled per feature as a quadratic surface plus a fourth-
 * power radial term (uneven light, vignetting), fitted to the border band by
 * least squares five times, each time without the pixels over 3 robust
 * sigmas (1.4826 x the median absolute deviation): the object where it
 * reaches the border. A pixel's deviation is the root of the sum of its
 * squared residuals over their sigmas (at least 0.03, 0.4, 0.4); being darker
 * by up to max_shadow does not count (a contact shadow), and further only
 * beyond it. Background: the 4-connected background-like areas (deviation up
 * to max_deviation) that reach the border. The object: the largest
 * 8-connected region of the rest after an open (3 x 3), its enclosed areas
 * filled unless they are background-like and at least 4 coarse pixels.
 *
 * Fine: at full size, in a band of fine_band_coarse_px coarse pixels on each
 * side of the coarse outline, a marker watershed on the median-filtered image
 * (fine_median_px, the lines removed again) puts the edge where the colour
 * changes most sharply: the object's edge rather than a shadow's soft one.
 * The largest 8-connected region of the result, closed (3 x 3), is returned.
 *
 * It fails, by design, where the border is not background: an object that
 * touches or fills the frame, two backgrounds (a mat smaller than the frame).
 * Parts thinner than about two coarse pixels (legs) are lost with the lines.
 *
 * @param bgr 8-bit, 3-channel image
 * @return CV_8U 0/255 of the image's size; empty when no object is found
 */
cv::Mat background_object_impl(cv::Mat const& bgr, BackgroundOptions const& options = {});

/**
 * @brief Contour in pixels to a counter-clockwise ring on the grid in the image mm frame
 *
 * The pixel contour is simplified by cv::approxPolyDP within px_for_mm
 * (tolerance_mm over the smaller of the frame's sx, sy) pixels; when the result is not a simple ring
 * (see simple_polygon_impl), the epsilon is halved, down to 0 (collinear
 * points removed only). Orientation comes from the sign of cv::contourArea
 * on the simplified pixel contour: rows run down, so a positive pixel area is
 * clockwise in mm. Pixels map by to_mm. Empty when no simple ring of at least
 * three vertices is found.
 */
Pot::Path64 contour_to_path_impl(std::vector<cv::Point> const& contour, PixelFrame const& frame,
                                 double tolerance_mm);

/**
 * @brief True when the polygon's rings are simple, disjoint and nested as a polygon with holes
 *
 * Every ring has at least three distinct vertices and a non-zero area; no
 * two edges of the outline and the holes cross or touch (adjacent edges of a
 * ring share only their common vertex), by brute force on the grid (edges
 * pruned by their extent), for the short rings of simplified contours. Then,
 * since no edges meet, one vertex per hole tells that every hole lies inside
 * the outline and outside the other holes: cv::pointPolygonTest on
 * pixel_rings, the pixel vertices the rings were made from (outline first,
 * then the holes in order). Orientation is not checked.
 */
bool simple_polygon_impl(Pot::Polygon64 const& polygon, std::vector<std::vector<cv::Point>> const& pixel_rings);

/**
 * @brief Make the mask's pixel contours simple rings
 *
 * Repeats until nothing changes: pixels not in any 2x2 block of set pixels
 * are cleared (lines and spurs one pixel wide, whose contours run back over
 * themselves), and in each 2x2 block where exactly the two diagonal pixels
 * are set, one of them is cleared (two regions meeting at a corner).
 *
 * @param mask CV_8U 0/255, changed in place
 */
void clean_mask_impl(cv::Mat& mask);

/**
 * @brief Holes of a CV_8U 0/255 mask
 *
 * The 4-connected components of the mask's complement (the dual of its
 * 8-connected regions) that do not reach the image border and have fewer
 * than max_area_px pixels (all of them by default).
 *
 * @return CV_8U 0/255 of the mask's size
 */
cv::Mat enclosed_holes_impl(cv::Mat const& region, double max_area_px = std::numeric_limits<double>::infinity());

/// Largest 8-connected region of a CV_8U 0/255 mask (all zero when none) and its area in pixels.
std::pair<cv::Mat, int> largest_region_impl(cv::Mat const& mask);

/**
 * @brief Polygon of a mask of one 8-connected region, traced with RETR_CCOMP
 *
 * The region's outer contour is the outline (counter-clockwise), its inner
 * contours enclosing at least min_hole_area_px square pixels (cv::contourArea
 * of the contour) are the holes (clockwise); each ring goes through
 * contour_to_path_impl. When the polygon is not simple (simple_polygon_impl:
 * rings simplified separately can cross each other), all rings are traced
 * again with half the epsilon, down to 0. Clean the mask with
 * clean_mask_impl first, so that the unsimplified rings are simple.
 *
 * @return error when the mask is empty or no simple polygon is found
 */
Result<Pot::Polygon64> trace_polygon_impl(cv::Mat const& region, PixelFrame const& frame, double tolerance_mm,
                                          double min_hole_area_px = 0);

/// Row-major copy of a CV_8U mask.
Mask to_mask_impl(cv::Mat const& mask);

}  // namespace Vision

#endif  // VISION_SEGMENT_IMPL_H
